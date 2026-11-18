// SPDX-License-Identifier: GPL-3.0-only
#include "core/hook_context.h"
#include "hooks/manual_recall.h"

#include <sourcehook_impl.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace SourceHook;
ISourceHook* g_SHPtr;
int g_PLID;

namespace {

struct Request
{
	int spectators;
	int cookie;
	bool nested = false;
};

struct Provider
{
	virtual int* Connect(Request&, const char* password);

	int originals = 0;
	int client = 7;
};

__attribute__((noinline)) int* Dispatch(Provider& provider, Request& request, const char* password)
{
	return provider.Connect(request, password);
}

int* Provider::Connect(Request& request, const char* password)
{
	++originals;
	if (!request.nested) {
		assert(std::strcmp(password, "outer") == 0);
		Request inner{1, 8, true};
		assert(Dispatch(*this, inner, "original-inner") == &client);
		assert(std::strcmp(password, "outer") == 0);
	} else {
		assert(std::strcmp(password, "inner") == 0);
	}
	return &client;
}

SH_DECL_MANUALHOOK2(TestConnect, 0, 0, 0, int*, Request&, const char*);

int managerCalls;
int supportCalls;
int connected;
int dropOnConnect;

int* Manager(Request& request, const char* password)
{
	if (META_RESULT_STATUS >= MRES_SUPERCEDE)
		RETURN_META_VALUE(MRES_IGNORED, nullptr);
	++managerCalls;
	sourcetv::ConnectionBuffers context(password);
	std::strcpy(context.password.data(), request.nested ? "inner" : "outer");
	if (dropOnConnect) {
		assert(SH_REMOVE_HOOK_ID(dropOnConnect));
		dropOnConnect = 0;
	}

	int* result = SOURCETV_RECALL_MANUAL(TestConnect, request, context.password.data());
	if (result)
		++connected;
	RETURN_META_VALUE(MRES_SUPERCEDE, result);
}

int* Support(Request& request, const char*)
{
	++supportCalls;
	if (request.spectators > 1)
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);
	RETURN_META_VALUE(MRES_IGNORED, nullptr);
}

void TestOrder(bool managerFirst, bool removeManagerFirst)
{
	Provider provider;
	auto addManager = [&] {
		g_PLID = 1;
		return SH_ADD_MANUALHOOK(TestConnect, &provider, SH_STATIC(Manager), false);
	};
	auto addSupport = [&] {
		g_PLID = 2;
		return SH_ADD_MANUALHOOK(TestConnect, &provider, SH_STATIC(Support), false);
	};

	int manager, support;
	if (managerFirst) {
		manager = addManager();
		support = addSupport();
	} else {
		support = addSupport();
		manager = addManager();
	}
	assert(manager && support);
	managerCalls = supportCalls = connected = 0;
	Request accepted{1, 9};
	Request denied{2, 9};
	assert(Dispatch(provider, accepted, "original") == &provider.client);
	assert(provider.originals == 2 && managerCalls == 2 && supportCalls == 2 && connected == 2);
	assert(!Dispatch(provider, denied, "original"));
	assert(provider.originals == 2 && supportCalls == 3 && connected == 2);
	assert(managerCalls == (managerFirst ? 3 : 2));

	if (removeManagerFirst) {
		assert(SH_REMOVE_HOOK_ID(manager));
		assert(!Dispatch(provider, denied, "original"));
		assert(provider.originals == 2 && supportCalls == 4);
		assert(SH_REMOVE_HOOK_ID(support));
	} else {
		assert(SH_REMOVE_HOOK_ID(support));
		assert(Dispatch(provider, accepted, "original") == &provider.client);
		assert(provider.originals == 4 && connected == 4 && supportCalls == 3);
		assert(SH_REMOVE_HOOK_ID(manager));
	}
}

void TestRemovalInCallback()
{
	Provider provider;
	g_PLID = 1;
	const int manager = SH_ADD_MANUALHOOK(TestConnect, &provider, SH_STATIC(Manager), false);
	g_PLID = 2;
	const int support = SH_ADD_MANUALHOOK(TestConnect, &provider, SH_STATIC(Support), false);
	assert(manager && support);
	dropOnConnect = support;
	supportCalls = connected = 0;
	Request request{1, 9, true};
	assert(Dispatch(provider, request, "original") == &provider.client);
	assert(provider.originals == 1 && supportCalls == 0 && connected == 1);
	assert(!SH_REMOVE_HOOK_ID(support));
	assert(SH_REMOVE_HOOK_ID(manager));
}

} // namespace

int main()
{
	static_assert(sizeof(void*) == 4);
	SourceHook::Impl::CSourceHookImpl hooks;
	g_SHPtr = &hooks;
	for (bool managerFirst : {false, true})
		for (bool removeManagerFirst : {false, true})
			TestOrder(managerFirst, removeManagerFirst);
	TestRemovalInCallback();
	std::puts("connect_coexistence: both load/unload orders, nested calls and removal passed");
}
