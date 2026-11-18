// SPDX-License-Identifier: GPL-3.0-only
#include "pawn_boundary.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <memory>

namespace pawn = sourcetv::pawn;

static void Check(bool condition, const char *description)
{
	if (!condition) {
		std::fprintf(stderr, "pawn_boundary_vm: %s\n", description);
		std::exit(1);
	}
}

int main(int argc, char **argv)
{
	Check(argc == 3, "requires the VM library and fixture plugin");
	void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!library) {
		std::fprintf(stderr, "pawn_boundary_vm: %s\n", dlerror());
		return 1;
	}
	auto factoryEntry = reinterpret_cast<SourcePawn::GetSourcePawnFactoryFn>(
		dlsym(library, "GetSourcePawnFactory"));
	Check(factoryEntry != nullptr, "VM factory export");
	auto *factory = factoryEntry(SOURCEPAWN_API_VERSION);
	Check(factory != nullptr, "VM factory API");
	{
		std::unique_ptr<SourcePawn::ISourcePawnEnvironment> environment(factory->NewEnvironment());
		Check(environment != nullptr, "VM environment");
		char error[256] = {};
		std::unique_ptr<SourcePawn::IPluginRuntime> runtime(
			environment->APIv2()->LoadBinaryFromFile(argv[2], error, sizeof(error)));
		Check(runtime != nullptr, error);
		auto& context = *runtime->GetDefaultContext();
		cell_t address;
		cell_t *heap = nullptr;
		Check(context.HeapAlloc(2, &address, &heap) == SP_ERROR_NONE, "allocate two cells");
		heap[0] = 0x41414141;
		heap[1] = 0x41414141;

		cell_t *cells = nullptr;
		Check(pawn::ResolveCells(context, address, cells, 2) && cells == heap, "exact cell range");
		Check(!pawn::ResolveCells(context, address, cells, 3), "cell range enters heap gap");
		const char *input = nullptr;
		Check(!pawn::ReadString(context, address, input), "unterminated input reaches heap gap");
		char *output = nullptr;
		Check(!pawn::ResolveBuffer(context, address + 1, 8, output), "unaligned span crosses heap end");
		Check(heap[0] == 0x41414141 && heap[1] == 0x41414141, "invalid ranges leave canaries intact");
		Check(pawn::ResolveBuffer(context, address + 2, 6, output), "exact unaligned range");
		Check(pawn::WriteString(context, address + 2, 1, output, "x") && !*output, "capacity one");
		Check(pawn::WriteString(context, address + 2, 2, output, "\xc3\xa9") && !*output,
			"UTF-8 truncation stops before a partial character");
		Check(pawn::WriteString(context, address + 2, 2, output, "\x80\x80") && !*output,
			"no leading byte to scan on a malformed prefix");
		Check(pawn::WriteString(context, address + 2, 6, output, "demo"), "valid Pawn string output");
		Check(pawn::ReadString(context, address + 2, input) && !std::strcmp(input, "demo"), "read Pawn string back");
		Check(context.HeapPop(address) == SP_ERROR_NONE, "release fixture heap");
		Check(!pawn::ResolveCells(context, address, cells), "released cell address rejected");
	}
	Check(dlclose(library) == 0, "close VM library");
	std::printf("pawn_boundary_vm: checks passed (%zu-bit)\n", sizeof(void*) * CHAR_BIT);
}
