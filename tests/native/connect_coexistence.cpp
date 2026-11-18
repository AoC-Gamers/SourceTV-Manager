// SPDX-License-Identifier: GPL-3.0-only
#include "core/hook_context.h"
#include "hooks/manual_recall.h"

#include <sourcehook_impl.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
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

// Independent x86 provider for the ten-argument call shape. The vector
// prefix is derived from the dedicated ELF and never from a product mirror.
struct AddressFixture {
	std::uint32_t canary = 0x80604020;
};

struct PlayersFixture {
	std::byte allocator[12]{};
	int count = 1;
	std::byte* elements = nullptr;
};

static_assert(offsetof(PlayersFixture, count) == 0xc);
static_assert(sizeof(PlayersFixture) == 20);

struct ConnectionProvider {
	virtual int* Connect(AddressFixture&, int, int, int, const char*, const char*, const char*, int,
		PlayersFixture&, bool);
	int originals = 0;
	int client = 77;
};

__attribute__((noinline)) int* Dispatch(ConnectionProvider& provider, AddressFixture& address, int protocol,
	int challenge, int auth, const char* name, const char* password, const char* cookie, int length,
	PlayersFixture& players, bool flag)
{
	return provider.Connect(address, protocol, challenge, auth, name, password, cookie, length, players, flag);
}

AddressFixture* expectedAddress;
PlayersFixture* expectedPlayers;
const char* expectedName;
const char* expectedCookie;
ConnectionProvider* expectedReceiver;
int marshallingCalls;

void CheckArguments(AddressFixture& address, int protocol, int challenge, int auth,
	const char* name, const char* cookie, int length, PlayersFixture& players, bool flag)
{
	assert(&address == expectedAddress && address.canary == 0x80604020);
	assert(&players == expectedPlayers && players.count >= 1);
	assert(protocol == 42 && challenge == -12345 && auth == 3);
	assert(name == expectedName && cookie == expectedCookie && length == 8 && flag);
}

int* ConnectionProvider::Connect(AddressFixture& address, int protocol, int challenge, int auth, const char* name,
	const char* password, const char* cookie, int length, PlayersFixture& players, bool flag)
{
	assert(this == expectedReceiver);
	CheckArguments(address, protocol, challenge, auth, name, cookie, length, players, flag);
	assert(std::strcmp(password, "recalled") == 0);
	++originals;
	return &client;
}

SH_DECL_MANUALHOOK10(TestConnectMarshalling, 0, 0, 0, int*, AddressFixture&, int, int, int,
	const char*, const char*, const char*, int, PlayersFixture&, bool);

int* CheckMarshalling(AddressFixture& address, int protocol, int challenge, int auth, const char* name,
	const char* password, const char* cookie, int length, PlayersFixture& players, bool flag)
{
	assert(META_IFACEPTR(ConnectionProvider) == expectedReceiver);
	CheckArguments(address, protocol, challenge, auth, name, cookie, length, players, flag);
	assert(std::strcmp(password, "incoming") == 0);
	++marshallingCalls;
	if (players.count > 1)
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);
	auto* client = SOURCETV_RECALL_MANUAL(TestConnectMarshalling, address, protocol, challenge, auth,
		name, "recalled", cookie, length, players, flag);
	RETURN_META_VALUE(MRES_SUPERCEDE, client);
}

void TestConnectionMarshalling()
{
	ConnectionProvider provider;
	AddressFixture address;
	PlayersFixture players;
	const char name[] = "spectator";
	const char cookie[8] = {};
	expectedReceiver = &provider;
	expectedAddress = &address;
	expectedPlayers = &players;
	expectedName = name;
	expectedCookie = cookie;
	const int hook = SH_ADD_MANUALHOOK(TestConnectMarshalling, &provider, SH_STATIC(CheckMarshalling), false);
	assert(hook);
	assert(Dispatch(provider, address, 42, -12345, 3, name, "incoming", cookie, sizeof(cookie),
		players, true) == &provider.client);
	players.count = 2;
	assert(!Dispatch(provider, address, 42, -12345, 3, name, "incoming", cookie, sizeof(cookie), players, true));
	assert(provider.originals == 1 && marshallingCalls == 2);
	assert(SH_REMOVE_HOOK_ID(hook));
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
	TestConnectionMarshalling();
	std::puts("connect_marshalling: independent x86 provider checked all ten arguments");
}
