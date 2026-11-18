#include "core/recording_hooks.h"
#include "core/hltvserverwrapper.h"
#include "core/hook_context.h"
#include "hooks/forwards.h"
#include "hooks/commonhooks.h"

namespace {
sourcetv::PointerOverrides hostOverrides;
#if SOURCE_ENGINE != SE_CSGO
sourcetv::PointerOverrides channelOverrides;
struct PrintInvocation
{
	const HLTVServerWrapper* wrapper;
	IClient* client;
};
std::vector<PrintInvocation> printInvocations;
#endif
}

SH_DECL_MANUALHOOK0_void(CHLTVServer_Shutdown, 0, 0, 0);

#if SOURCE_ENGINE != SE_CSGO

// Stuff to print to demo console
SH_DECL_HOOK0_void_vafmt(IClient, ClientPrintf, SH_NOATTRIB, 0);

// Linux has the ClientPrintf method in both CGameClient and IClient's vtables
// and uses both.. Need to hook both....... i guess?
#ifndef WIN32
SH_DECL_MANUALHOOK0_void_vafmt(CGameClient_ClientPrintf, 0, 0, 0);
#endif

// This should be large enough.
#define FAKE_VTBL_LENGTH 70
static void *FakeNetChanVtbl[FAKE_VTBL_LENGTH];
static void *FakeNetChan = &FakeNetChanVtbl;
SH_DECL_MANUALHOOK3(NetChan_SendNetMsg, 0, 0, 0, bool, INetMessage &, bool, bool);
#endif // SOURCE_ENGINE != SE_CSGO

HLTVServerWrapper::HLTVServerWrapper(IHLTVServer *server)
	: m_BaseServer(server->GetBaseServer()),
	  m_HLTVServer(server),
	  m_DemoRecorder(g_HLTVServers.GetDemoRecorderPtr(server))
{
}

bool HLTVServerWrapper::BeginShutdown()
{
	if (!m_Active)
		return false;

	m_Active = false;
	if (hltvserver == this)
		hltvserver = nullptr;

	Unhook();
	return true;
}

IServer *HLTVServerWrapper::GetBaseServer()
{
	return m_BaseServer;
}

IHLTVServer *HLTVServerWrapper::GetHLTVServer()
{
	return m_HLTVServer;
}

IDemoRecorder *HLTVServerWrapper::GetDemoRecorder()
{
	return m_DemoRecorder;
}

const char *HLTVServerWrapper::GetDemoFileName()
{
	if (!m_DemoRecorder)
		return nullptr;

#if SOURCE_ENGINE == SE_CSGO
	return reinterpret_cast<const char *>(m_DemoRecorder) + 8;
#else
	// In the L4D2 SDK, CDemoFile starts with m_szFileName[MAX_PATH]. GetDemoFile()
	// returns that recorder-owned object, so its address is the filename buffer.
	// The buffer belongs to the recorder; callers must copy it before teardown.
	return reinterpret_cast<const char *>(m_DemoRecorder->GetDemoFile());
#endif
}

int HLTVServerWrapper::GetInstanceNumber()
{
	return g_HLTVServers.GetInstanceNumber(m_HLTVServer);
}

HLTVClientWrapper *HLTVServerWrapper::GetClient(int index)
{
	// The public API uses one-based client indexes; reject invalid values before
	// converting to size_t or subtracting one for the vector position.
	if (index <= 0)
		return nullptr;

	// Grow the vector with null pointers
	// There might have been clients with lower indexes before we were loaded.
	const size_t clientCount = static_cast<size_t>(index);
	const size_t clientSlot = clientCount - 1;
	if (m_Clients.size() < clientCount)
	{
		// resize value-initializes the new unique_ptr entries to nullptr.
		m_Clients.resize(clientCount);
	}

	if (!m_Clients[clientSlot])
	{
		m_Clients[clientSlot] = std::make_unique<HLTVClientWrapper>();
	}

	return m_Clients[clientSlot].get();
}

HLTVClientWrapper *HLTVServerWrapper::FindClient(int index) const
{
	// Native queries must not allocate wrappers for empty slots. Connected
	// clients already have wrappers from HookServer (including late load) or
	// OnSpectatorConnect before SourceTV_OnSpectatorConnected is fired.
	if (index <= 0)
		return nullptr;

	const size_t clientSlot = static_cast<size_t>(index - 1);
	if (clientSlot >= m_Clients.size())
		return nullptr;

	return m_Clients[clientSlot].get();
}

void HLTVServerWrapper::Hook()
{
	g_pSTVForwards.HookServer(this);
	if (m_DemoRecorder)
		g_pSTVForwards.HookRecorder(m_DemoRecorder);

	if (g_HLTVServers.HasShutdownOffset())
		SH_ADD_MANUALHOOK(CHLTVServer_Shutdown, m_BaseServer, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVServerShutdown), false);

	if (!iserver)
		return;

	const int slot = m_HLTVServer->GetHLTVSlot();
	if (slot < 0 || slot >= iserver->GetClientCount())
		return;

	IClient *pClient = m_BotClient = iserver->GetClient(slot);
	if (!pClient)
		return;

	g_pSTVCommonHooks.AddHLTVClientHook(this, pClient);
#if SOURCE_ENGINE != SE_CSGO
	SH_ADD_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnIClient_ClientPrintf_Post), false);
#ifndef WIN32
	// The IClient vtable is +4 from the CBaseClient vtable due to multiple inheritance.
	void *pGameClient = (void *)((intptr_t)pClient - 4);
	if (g_HLTVServers.HasClientPrintfOffset())
		SH_ADD_MANUALHOOK(CGameClient_ClientPrintf, pGameClient, SH_MEMBER(this, &HLTVServerWrapper::OnCGameClient_ClientPrintf_Post), false);
#endif // !WIN32
#endif // SOURCE_ENGINE != SE_CSGO
}

void HLTVServerWrapper::Unhook()
{
	g_pSTVForwards.UnhookServer(this);
	if (m_DemoRecorder)
		g_pSTVForwards.UnhookRecorder(m_DemoRecorder);

	if (g_HLTVServers.HasShutdownOffset())
		SH_REMOVE_MANUALHOOK(CHLTVServer_Shutdown, m_BaseServer, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVServerShutdown), false);

	hostOverrides.Clear(this);
#if SOURCE_ENGINE != SE_CSGO
	channelOverrides.Clear(this);
#endif
	if (IClient *pClient = m_BotClient)
	{
		// Remove ExecuteStringCommand hook
		g_pSTVCommonHooks.RemoveHLTVClientHook(this, pClient);
#if SOURCE_ENGINE != SE_CSGO
		SH_REMOVE_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnIClient_ClientPrintf_Post), false);
#ifndef WIN32
		// The IClient vtable is +4 from the CBaseClient vtable due to multiple inheritance.
		void *pGameClient = (void *)((intptr_t)pClient - 4);
		if (g_HLTVServers.HasClientPrintfOffset())
			SH_REMOVE_MANUALHOOK(CGameClient_ClientPrintf, pGameClient, SH_MEMBER(this, &HLTVServerWrapper::OnCGameClient_ClientPrintf_Post), false);
#endif // !WIN32
#endif // SOURCE_ENGINE != SE_CSGO
		m_BotClient = nullptr;
	}
}

// CHLTVServer::Shutdown deregisters the hltvserver from the hltvdirector, 
// so RemoveHLTVServer/SetHLTVServer(NULL) is called too on the master proxy.
void HLTVServerWrapper::OnHLTVServerShutdown()
{
	const auto self = g_HLTVServers.Pin(this);
	if (self && self->IsActive())
		g_HLTVServers.RemoveServer(m_HLTVServer, true);

	RETURN_META(MRES_IGNORED);
}

// When bots issue a command that would print stuff to their console, 
// the server might crash, because ExecuteStringCommand doesn't set the 
// global host_client pointer to the client on whom the command is run.
// Host_Client_Printf blatantly tries to call host_client->ClientPrintf
// while the pointer might point to some other player or garbage.
// This leads to e.g. the output of the "status" command not being 
// recorded in the SourceTV demo.
// The approach here is to set host_client correctly for the SourceTV
// bot and reset it to the old value after command execution.
bool HLTVServerWrapper::OnHLTVBotExecuteStringCommand(const char *s)
{
	IClient *client = META_IFACEPTR(IClient);
	void** slot = m_Active && client ? static_cast<void**>(host_client) : nullptr;
	// The IClient subobject is +4 from the CBaseClient subobject.
	void* gameClient = slot ? reinterpret_cast<void*>(reinterpret_cast<intptr_t>(client) - 4) : nullptr;
	hostOverrides.Push(this, slot, gameClient);
	if (!slot && s && !Q_stricmp(s, "status"))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	RETURN_META_VALUE(MRES_IGNORED, false);
}

bool HLTVServerWrapper::OnHLTVBotExecuteStringCommand_Post(const char *s)
{
	hostOverrides.Pop(this);
	RETURN_META_VALUE(MRES_IGNORED, false);
}

#if SOURCE_ENGINE != SE_CSGO
void HLTVServerWrapper::OnCGameClient_ClientPrintf_Post(const char* buf)
{
	void *pGameClient = META_IFACEPTR(void);
	IClient *pClient = (IClient *)((intptr_t)pGameClient + 4);
	HandleClientPrintf(pClient, buf);

	// We already called the function in HandleClientPrintf.
	// Would crash or not do anything anyways.
	RETURN_META(MRES_SUPERCEDE);
}

void HLTVServerWrapper::OnIClient_ClientPrintf_Post(const char* buf)
{
	IClient *pClient = META_IFACEPTR(IClient);
	HandleClientPrintf(pClient, buf);

	// We already called the function in HandleClientPrintf.
	// Would crash or not do anything anyways.
	RETURN_META(MRES_SUPERCEDE);
}

void HLTVServerWrapper::HandleClientPrintf(IClient *pClient, const char* buf)
{
	const auto self = g_HLTVServers.Pin(this);
	if (!self || !m_Active || !pClient || !g_HLTVServers.CanRedirectPrintf())
		return;

	// Craft our own "NetChan" pointer
	static int offset = -1;
	if (!g_pGameConf->GetOffset("CBaseClient::m_NetChannel", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to find CBaseClient::m_NetChannel offset. Can't print to demo console.");
		return;
	}

#ifdef WIN32
	void *pNetChannel = (void *)((char *)pClient + offset);
#else
	void *pNetChannel = (void *)((char *)pClient + offset - 4);
#endif
	PrintInvocation call{this, pClient};
	printInvocations.push_back(call);
	channelOverrides.Push(this, static_cast<void**>(pNetChannel), &FakeNetChan, &call);
	const auto restore = ke::MakeScopeGuard([&] {
		channelOverrides.Pop(this, &call);
		printInvocations.pop_back();
	});
	SH_CALL(pClient, &IClient::ClientPrintf)("%s", buf);
}
#endif

/**
 * Manage the wrappers!
 */
void HLTVServerWrapperManager::InitHooks()
{
	int offset;
	if (g_pGameConf->GetOffset("CHLTVServer::Shutdown", &offset) && offset >= 0)
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_Shutdown, offset, 0, 0);
		m_bHasShutdownOffset = true;
	}
	else
	{
		smutils->LogError(myself, "Failed to find CHLTVServer::Shutdown offset.");
	}

#if SOURCE_ENGINE != SE_CSGO
#ifndef WIN32
	if (g_pGameConf->GetOffset("CGameClient::ClientPrintf", &offset) && offset >= 0)
	{
		SH_MANUALHOOK_RECONFIGURE(CGameClient_ClientPrintf, offset, 0, 0);
		m_bHasClientPrintfOffset = true;
	}
	else
	{
		smutils->LogError(myself, "Failed to find CGameClient::ClientPrintf offset. Won't catch \"status\" console output.");
	}
#endif // !WIN32

	if (g_pGameConf->GetOffset("CNetChan::SendNetMsg", &offset))
	{
		if (offset < 0 || offset >= FAKE_VTBL_LENGTH)
		{
			smutils->LogError(myself, "CNetChan::SendNetMsg offset is outside the fake vtable.");
		}
		else
		{
			// This is a hack. Bots don't have a net channel, but ClientPrintf tries to call m_NetChannel->SendNetMsg directly.
			// CGameClient::SendNetMsg would have redirected it to the hltvserver correctly, but isn't used there..
			// We craft a fake object with a large enough "vtable" and hook it using sourcehook.
			// Before a call to ClientPrintf, this fake object is set as CBaseClient::m_NetChannel, so ClientPrintf creates 
			// the SVC_Print INetMessage and calls our "hooked" m_NetChannel->SendNetMsg function.
			// In that function we just call CGameClient::SendNetMsg with the given INetMessage to flow it through the same
			// path as other net messages.
			SH_MANUALHOOK_RECONFIGURE(NetChan_SendNetMsg, offset, 0, 0);
			m_bSendNetMsgHooked = SH_ADD_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan,
				SH_MEMBER(this, &HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg), false) != 0;
		}
	}
	else
	{
		smutils->LogError(myself, "Failed to find CNetChan::SendNetMsg offset. Can't print to demo console.");
	}
#endif
}

void HLTVServerWrapperManager::ShutdownHooks()
{
#if SOURCE_ENGINE != SE_CSGO
	if (m_bSendNetMsgHooked)
	{
		SH_REMOVE_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg), false);
		m_bSendNetMsgHooked = false;
	}
#endif
	m_bHasClientPrintfOffset = false;
	m_bHasShutdownOffset = false;
}

ke::RefPtr<HLTVServerWrapper> HLTVServerWrapperManager::Pin(const HLTVServerWrapper *wrapper) const
{
	for (const auto& candidate : m_HLTVServers) {
		if (candidate.get() == wrapper)
			return candidate;
	}
	return {};
}

void HLTVServerWrapperManager::AddServer(IHLTVServer *server)
{
	if (!m_AcceptServers || !server)
		return;
	for (const auto& existing : m_HLTVServers) {
		if (existing->GetHLTVServer() == server)
			return;
	}

	g_pSTVForwards.CreateBroadcastLocalChatDetour();

	const auto wrapper = ke::RefPtr<HLTVServerWrapper>(new HLTVServerWrapper(server));
	if (!wrapper->GetBaseServer())
		return;
	m_HLTVServers.push_back(wrapper);
	wrapper->Hook();
	if (!hltvserver)
		hltvserver = wrapper.get();

	g_pSTVForwards.CallOnServerStart(server);
	if (wrapper->IsActive()) {
		const bool available = sourcetv::recording::RecorderAvailable(g_pSTVForwards.RecordingSessions(), wrapper->GetDemoRecorder());
		wrapper->m_AnnouncedRecorder = available;
		g_pSTVForwards.CallOnRecorderAvailabilityChanged(wrapper->GetInstanceNumber(), available);
	}
}

void HLTVServerWrapperManager::RemoveServer(IHLTVServer *server, bool informPlugins)
{
	const auto wrapper = Pin(GetWrapper(server));
	if (!wrapper || !wrapper->BeginShutdown())
		return;

	const int instance = wrapper->GetInstanceNumber();
	if (informPlugins)
		g_pSTVForwards.CallOnServerShutdown(server);
	if (std::exchange(wrapper->m_AnnouncedRecorder, false) && m_AcceptServers)
		g_pSTVForwards.CallOnRecorderAvailabilityChanged(instance, false);
	m_HLTVServers.erase(std::remove_if(m_HLTVServers.begin(), m_HLTVServers.end(),
		[&](const auto& candidate) { return candidate.get() == wrapper.get(); }), m_HLTVServers.end());
}

void HLTVServerWrapperManager::ShutdownServers()
{
	m_AcceptServers = false;
	// Retire the registry before notifying listeners so callbacks cannot select
	// a different wrapper that is also being unloaded.
	auto servers = std::move(m_HLTVServers);
	m_HLTVServers.clear();
	for (const auto& wrapper : servers)
		wrapper->BeginShutdown();
	for (const auto& wrapper : servers) {
		if (std::exchange(wrapper->m_AnnouncedRecorder, false))
			g_pSTVForwards.CallOnRecorderAvailabilityChanged(wrapper->GetInstanceNumber(), false);
	}
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IHLTVServer *hltvserver)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->IsActive() && wrapper->GetHLTVServer() == hltvserver)
			return wrapper;
	}
	return nullptr;
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IServer *server)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->IsActive() && wrapper->GetBaseServer() == server)
			return wrapper;
	}
	return nullptr;
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IDemoRecorder *demorecorder)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->IsActive() && wrapper->GetDemoRecorder() != nullptr && wrapper->GetDemoRecorder() == demorecorder)
			return wrapper;
	}
	return nullptr;
}

int HLTVServerWrapperManager::GetInstanceNumber(IHLTVServer *hltvserver)
{
#if SOURCE_ENGINE == SE_CSGO
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		if (hltvserver == hltvdirector->GetHLTVServer(i))
			return i;
	}

	// We should have found it in the above loop :S
	smutils->LogError(myself, "Failed to find IHLTVServer instance in director.");
	return -1;
#else
	return 0;
#endif
}

IDemoRecorder *HLTVServerWrapperManager::GetDemoRecorderPtr(IHLTVServer *hltv)
{
	static int offset = -1;
	if (offset == -1)
	{
		void *addr;
		if (!g_pGameConf->GetAddress("CHLTVServer::m_DemoRecorder", &addr))
		{
			smutils->LogError(myself, "Failed to get CHLTVServer::m_DemoRecorder offset.");
			return nullptr;
		}

		*(int **)&offset = (int *)addr;

		// See if we have to subtract something from the offset.
		int baseOffset = 0;
		if (g_pGameConf->GetOffset("CHLTVDemoRecorder_BaseOffset", &baseOffset))
		{
			offset -= baseOffset;
		}
	}

	if (hltv)
	{
		IServer *baseServer = hltv->GetBaseServer();
#ifndef WIN32
	return (IDemoRecorder *)((intptr_t)baseServer + offset);
#else
#if SOURCE_ENGINE == SE_CSGO
		return (IDemoRecorder *)((intptr_t)hltv + offset);
#else
		return (IDemoRecorder *)((intptr_t)baseServer + offset);
#endif // SOURCE_ENGINE == SE_CSGO
#endif // !WIN32
	}
	else
	{
		return nullptr;
	}
}

bool HLTVServerWrapperManager::HasShutdownOffset()
{
	return m_bHasShutdownOffset;
}

bool HLTVServerWrapperManager::HasClientPrintfOffset()
{
	return m_bHasClientPrintfOffset;
}

#if SOURCE_ENGINE != SE_CSGO
bool HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg(INetMessage &msg, bool bForceReliable, bool bVoice)
{
	if (printInvocations.empty() || !printInvocations.back().wrapper->IsActive())
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	IClient *pClient = printInvocations.back().client;
	if (!pClient)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	// Let the message flow through the intended path like CGameClient::SendNetMsg wants to.
	bool bRetSent = pClient->SendNetMsg(msg, bForceReliable);

	// It's important to supercede, because there is no original function to call.
	// (the "vtable" was empty before hooking it)
	// See FakeNetChan variable at the top.
	RETURN_META_VALUE(MRES_SUPERCEDE, bRetSent);
}
#endif

HLTVServerWrapperManager g_HLTVServers;
