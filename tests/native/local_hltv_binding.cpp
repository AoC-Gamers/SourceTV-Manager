// SPDX-License-Identifier: GPL-3.0-only
#include "core/hltv_server_binding.h"

#include "CallMaker.h"
#include <sp_vm_api.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>

using namespace SourcePawn;

CallMaker2 g_CallMaker2;
ISourcePawnEngine* g_SPEngine = nullptr;

namespace {

class TestPages final : public ISourcePawnEngine
{
public:
	void* AllocatePageMemory(size_t size) override
	{
		assert(!m_Page && size <= m_Size);
		m_Page = mmap(nullptr, m_Size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		assert(m_Page != MAP_FAILED);
		return m_Page;
	}

	void FreePageMemory(void* address) override
	{
		assert(address == m_Page);
		const int result = munmap(address, m_Size);
		assert(result == 0);
		m_Page = nullptr;
	}

	// The x86 call JIT uses the page allocation methods.
	sp_plugin_t* LoadFromFilePointer(FILE*, int*) override { std::abort(); }
	sp_plugin_t* LoadFromMemory(void*, sp_plugin_t*, int*) override { std::abort(); }
	int FreeFromMemory(sp_plugin_t*) override { std::abort(); }
	void* BaseAlloc(size_t) override { std::abort(); }
	void BaseFree(void*) override { std::abort(); }
	void* ExecAlloc(size_t) override { std::abort(); }
	void ExecFree(void*) override { std::abort(); }
	IDebugListener* SetDebugListener(IDebugListener*) override { std::abort(); }
	unsigned int GetContextCallCount() override { std::abort(); }
	unsigned int GetEngineAPIVersion() override { std::abort(); }
	void SetReadWrite(void* address) override { Protect(address, PROT_READ | PROT_WRITE); }
	void SetReadExecute(void* address) override { Protect(address, PROT_READ | PROT_EXEC); }
	int SetDebugBreakHandler(SPVM_DEBUGBREAK) override { std::abort(); }

private:
	void Protect(void* address, int flags)
	{
		assert(address == m_Page);
		const int result = mprotect(address, m_Size, flags);
		assert(result == 0);
	}

	void* m_Page = nullptr;
	size_t m_Size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
};

struct Observation
{
	void* receiver;
	void* event;
	bool reliable;
	int calls;
} observed{};

// Independent x86 provider. It knows the ABI argument widths and order, and
// doesn't inspect memory through any declaration of a game class.
extern "C" void ReceiveLocalEvent(void* receiver, void* event, bool reliable)
{
	observed = {receiver, event, reliable, observed.calls + 1};
}

} // namespace

int main()
{
	static_assert(sizeof(void*) == 4 && sizeof(bool) == 1);
	using sourcetv::abi::HltvServerBase;
	assert(!HltvServerBase::FromOffset(-1));

	TestPages pages;
	g_SPEngine = &pages;

	CallMaker bintools;
	SourceMod::PassInfo inputs[2]{};
	inputs[0].size = 4;
	inputs[1].size = 1;
	for (auto& input : inputs) {
		input.type = SourceMod::PassType_Basic;
		input.flags = PASSFLAG_BYVAL;
	}
	auto* call = bintools.CreateCall(reinterpret_cast<void*>(&ReceiveLocalEvent),
		SourceMod::CallConv_ThisCall, nullptr, inputs, 2, 0);
	assert(call);

	std::array<std::byte, 32> storage{};
	void* complete = storage.data() + 8;
	void* base = storage.data() + 12;
	int event = 42;
	const auto layout = HltvServerBase::FromOffset(4);
	assert(layout && layout->ServerBase(complete) == base);
	assert(!layout->CompleteServer(nullptr) && !layout->ServerBase(nullptr));

	auto args = sourcetv::abi::LocalEventArguments(layout->CompleteServer(base), &event, true);
	call->Execute(args.data(), nullptr);
	assert(observed.calls == 1 && observed.receiver == complete);
	assert(observed.event == &event && observed.reliable);

	const auto wrong = HltvServerBase::FromOffset(8);
	args = sourcetv::abi::LocalEventArguments(wrong->CompleteServer(base), &event, false);
	call->Execute(args.data(), nullptr);
	assert(observed.calls == 2 && observed.receiver != complete);
	assert(observed.event == &event && !observed.reliable);

	call->Destroy();
	std::puts("local_hltv_binding: BinTools x86 direct calls validated");
}
