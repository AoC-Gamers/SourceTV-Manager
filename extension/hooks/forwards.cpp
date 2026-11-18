/**
* vim: set ts=4 :
* =============================================================================
* SourceMod SourceTV Manager Extension
* Copyright (C) 2004-2016 AlliedModders LLC.  All rights reserved.
* =============================================================================
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License, version 3.0, as published by the
* Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*
* As a special exception, AlliedModders LLC gives you permission to link the
* code of this program (as well as its derivative works) to "Half-Life 2," the
* "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
* by the Valve Corporation.  You must obey the GNU General Public License in
* all respects for all other code used.  Additionally, AlliedModders LLC grants
* this exception to all derivative works.  AlliedModders LLC defines further
* exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
* or <http://www.sourcemod.net/license.php>.
*
* Version: $Id$
*/

#include "core/recording_hooks.h"
#include "extension.h"
#include "core/local_hltv_broadcast.h"
#include "hooks/forwards.h"
#include "hooks/manual_recall.h"
#include <climits>
#include "core/hltvserverwrapper.h"
#include "core/hook_context.h"
#include "hooks/commonhooks.h"

CForwardManager g_pSTVForwards;

// Only windows always uses the vtable for these. Linux does direct calls, so we use detours there.
#ifdef WIN32
SH_DECL_HOOK2_void(IDemoRecorder, StartRecording, SH_NOATTRIB, 0, const char *, bool)
#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0, CGameInfo const *)
#else
SH_DECL_HOOK0_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0)
#endif // SOURCE_ENGINE == SE_CSGO
#endif // !WIN32

#if SOURCE_ENGINE == SE_CSGO
SH_DECL_MANUALHOOK13(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, const netadr_t &, int, int, int, const char *, const char *, const char *, int, CUtlVector<NetMsg_SplitPlayerConnect *> &, bool, CrossPlayPlatform_t, const unsigned char *, int);
SH_DECL_MANUALHOOK1_void_vafmt(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &);
SH_DECL_HOOK1_void(IClient, Disconnect, SH_NOATTRIB, 0, const char *);
#ifndef WIN32
SH_DECL_MANUALHOOK1_void(CBaseClient_Disconnect, 0, 0, 0, const char *);
#endif // !WIN32

#elif SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
SH_DECL_MANUALHOOK10(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, ConnectAddress, int, int, int, const char *, const char *, const char *, int, ConnectPlayers &, bool);
SH_DECL_MANUALHOOK1_void_vafmt(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &);
SH_DECL_HOOK0_void_vafmt(IClient, Disconnect, SH_NOATTRIB, 0);
#ifndef WIN32
SH_DECL_MANUALHOOK0_void_vafmt(CBaseClient_Disconnect, 0, 0, 0);
#endif // !WIN32

#else
SH_DECL_MANUALHOOK9(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, netadr_t &, int, int, int, int, const char *, const char *, const char *, int);
SH_DECL_MANUALHOOK3_void(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &, int, const char *);
SH_DECL_HOOK0_void_vafmt(IClient, Disconnect, SH_NOATTRIB, 0);
#ifndef WIN32
SH_DECL_MANUALHOOK0_void_vafmt(CBaseClient_Disconnect, 0, 0, 0);
#endif // !WIN32

#endif
SH_DECL_MANUALHOOK0_void(CBaseClient_ActivatePlayer, 0, 0, 0);

SH_DECL_MANUALHOOK1(CHLTVServer_GetChallengeType, 0, 0, 0, int, const netadr_t &);

void CForwardManager::Init()
{
	if (m_Initialized)
		return;

	m_Initialized = true;
	plsys->AddPluginsListener(this);
	int offset = -1;
	if (!g_pGameConf->GetOffset("CHLTVServer::ConnectClient", &offset) || offset < 0)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::ConnectClient offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_ConnectClient, offset, 0, 0);
		m_bHasClientConnectOffset = true;
	}

	if (!g_pGameConf->GetOffset("CHLTVServer::RejectConnection", &offset) || offset < 0)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::RejectConnection offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_RejectConnection, offset, 0, 0);
		m_bHasRejectConnectionOffset = true;
	}

	if (!g_pGameConf->GetOffset("CHLTVServer::GetChallengeType", &offset) || offset < 0)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::GetChallengeType offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_GetChallengeType, offset, 0, 0);
		m_bHasGetChallengeTypeOffset = true;
	}

	if (!g_pGameConf->GetOffset("CBaseClient::ActivatePlayer", &offset) || offset < 0)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::ActivatePlayer offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_ActivatePlayer, offset, 0, 0);
		m_bHasActivatePlayerOffset = true;
	}

#ifndef WIN32
	if (!g_pGameConf->GetOffset("CBaseClient::Disconnect", &offset) || offset < 0)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::Disconnect offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_Disconnect, offset, 0, 0);
		m_bHasDisconnectOffset = true;
	}
#endif

	g_pSTVCommonHooks.Init();

	m_StartRecordingFwd = forwards->CreateForward("SourceTV_OnStartRecording", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_StopRecordingFwd = forwards->CreateForward("SourceTV_OnStopRecording", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_Cell);
	m_RecordingFileStartedFwd = forwards->CreateForward("SourceTV_OnRecordingFileStarted", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_Cell);
	m_RecordingFileClosedFwd = forwards->CreateForward("SourceTV_OnRecordingFileClosed", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_Cell);
	m_RecorderAvailabilityChangedFwd = forwards->CreateForward("SourceTV_OnRecorderAvailabilityChanged", ET_Ignore, 2, NULL, Param_Cell, Param_Cell);
	m_SpectatorPreConnectFwd = forwards->CreateForward("SourceTV_OnSpectatorPreConnect", ET_LowEvent, 4, NULL, Param_String, Param_String, Param_String, Param_String);
	m_SpectatorConnectedFwd = forwards->CreateForward("SourceTV_OnSpectatorConnected", ET_Ignore, 1, NULL, Param_Cell);
	m_SpectatorDisconnectFwd = forwards->CreateForward("SourceTV_OnSpectatorDisconnect", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_SpectatorDisconnectedFwd = forwards->CreateForward("SourceTV_OnSpectatorDisconnected", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_SpectatorPutInServerFwd = forwards->CreateForward("SourceTV_OnSpectatorPutInServer", ET_Ignore, 1, NULL, Param_Cell);
	m_SpectatorChatMessageFwd = forwards->CreateForward("SourceTV_OnSpectatorChatMessage", ET_Hook, 3, NULL, Param_Cell, Param_String, Param_String);
	m_SpectatorChatMessagePostFwd = forwards->CreateForward("SourceTV_OnSpectatorChatMessage_Post", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_String);

	m_ServerStartFwd = forwards->CreateForward("SourceTV_OnServerStart", ET_Ignore, 1, NULL, Param_Cell);
	m_ServerShutdownFwd = forwards->CreateForward("SourceTV_OnServerShutdown", ET_Ignore, 1, NULL, Param_Cell);
}

void CForwardManager::Shutdown()
{
	if (!m_Initialized)
		return;

	m_Initialized = false;
	RemoveBroadcastLocalChatDetour();
#ifndef WIN32
	sourcetv::recording::RemoveHooks(m_RecordingDetours, [](CDetour *hook) { hook->Destroy(); });
#else
	for (auto& entry : m_RecorderHooks)
		sourcetv::recording::RemoveHooks(entry.second, [](int id) { SH_REMOVE_HOOK_ID(id); });
	m_RecorderHooks.clear();
#endif
	m_RecordingSessions.Reset();
	plsys->RemovePluginsListener(this);
	forwards->ReleaseForward(m_StartRecordingFwd);
	forwards->ReleaseForward(m_StopRecordingFwd);
	forwards->ReleaseForward(m_RecordingFileStartedFwd);
	forwards->ReleaseForward(m_RecordingFileClosedFwd);
	forwards->ReleaseForward(m_RecorderAvailabilityChangedFwd);
	forwards->ReleaseForward(m_SpectatorPreConnectFwd);
	forwards->ReleaseForward(m_SpectatorConnectedFwd);
	forwards->ReleaseForward(m_SpectatorDisconnectFwd);
	forwards->ReleaseForward(m_SpectatorDisconnectedFwd);
	forwards->ReleaseForward(m_SpectatorPutInServerFwd);
	forwards->ReleaseForward(m_SpectatorChatMessageFwd);
	forwards->ReleaseForward(m_SpectatorChatMessagePostFwd);

	forwards->ReleaseForward(m_ServerStartFwd);
	forwards->ReleaseForward(m_ServerShutdownFwd);
}

void CForwardManager::OnPluginUnloaded(IPlugin *plugin)
{
	if (plugin)
		m_RecordingSessions.Revoke({plugin, plugin->GetSerial()});
}

void CForwardManager::HookRecorder(IDemoRecorder *recorder)
{
	if (!sourcetv::recording::RegisterRecorder(m_RecordingSessions, recorder, [&] {
#ifdef WIN32
		std::array<int, 4> hooks{};
		const bool installed = sourcetv::recording::InstallHooks(hooks, [&](std::size_t index) {
			switch (index) {
			case 0:
				return SH_ADD_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Pre), false);
			case 1:
				return SH_ADD_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), true);
			case 2:
				return SH_ADD_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording), false);
			default:
				return SH_ADD_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording_Post), true);
			}
		}, [](int id) { SH_REMOVE_HOOK_ID(id); });
		if (installed)
			m_RecorderHooks.emplace(recorder, hooks);
		return installed;
#else
		return InstallRecordingDetours();
#endif
	}))
		smutils->LogError(myself, "SourceTV recorder unavailable: could not install all recording observers.");
}

void CForwardManager::UnhookRecorder(IDemoRecorder *recorder)
{
	m_RecordingSessions.Forget(recorder);
#ifdef WIN32
	const auto entry = m_RecorderHooks.find(recorder);
	if (entry != m_RecorderHooks.end()) {
		sourcetv::recording::RemoveHooks(entry->second, [](int id) { SH_REMOVE_HOOK_ID(id); });
		m_RecorderHooks.erase(entry);
	}
#endif
}

void CForwardManager::HookServer(HLTVServerWrapper *wrapper)
{
	IServer *server = wrapper->GetBaseServer();
	if (m_bHasClientConnectOffset)
		SH_ADD_MANUALHOOK(CHLTVServer_ConnectClient, server, SH_MEMBER(this, &CForwardManager::OnSpectatorConnect), false);
	
	if (m_bHasGetChallengeTypeOffset)
		SH_ADD_MANUALHOOK(CHLTVServer_GetChallengeType, server, SH_MEMBER(this, &CForwardManager::OnGetChallengeType), false);

	// Hook all already connected clients as well for late loading
	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient *client = server->GetClient(i);
		if (client && client->IsConnected())
		{
			HookClient(wrapper, client);
			// Ip and password unknown :(
			// Could add more gamedata to fetch it if people really lateload the extension and expect it to work :B
			wrapper->GetClient(i + 1)->Initialize("", "", client);
		}
	}
}

void CForwardManager::UnhookServer(HLTVServerWrapper *wrapper)
{
	IServer *server = wrapper->GetBaseServer();
	if (m_bHasClientConnectOffset)
		SH_REMOVE_MANUALHOOK(CHLTVServer_ConnectClient, server, SH_MEMBER(this, &CForwardManager::OnSpectatorConnect), false);

	if (m_bHasGetChallengeTypeOffset)
		SH_REMOVE_MANUALHOOK(CHLTVServer_GetChallengeType, server, SH_MEMBER(this, &CForwardManager::OnGetChallengeType), false);

	// Unhook exactly the receivers registered for this server.
	for (auto it = m_HookedClients.begin(); it != m_HookedClients.end();) {
		const auto current = it++;
		if (current->second == wrapper)
			UnhookClient(current->first);
	}
}

void CForwardManager::HookClient(HLTVServerWrapper *owner, IClient *client)
{
	if (!m_HookedClients.emplace(client, owner).second)
		return;

	// Hook ExecuteStringCommand for chat messages
	g_pSTVCommonHooks.AddSpectatorHook(this, client);

	void *pGameClient = (void *)((intptr_t)client - 4);
	if (m_bHasActivatePlayerOffset)
		SH_ADD_MANUALHOOK(CBaseClient_ActivatePlayer, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorPutInServer), true);
	
	// Linux' engine uses the CGameClient vtable internally, but we're using the IClient vtable to kick players.
	// Need to hook both to catch all cases >.<
	SH_ADD_HOOK(IClient, Disconnect, client, SH_MEMBER(this, &CForwardManager::IClient_OnSpectatorDisconnect), false);
#ifndef WIN32
	if (m_bHasDisconnectOffset)
		SH_ADD_MANUALHOOK(CBaseClient_Disconnect, pGameClient, SH_MEMBER(this, &CForwardManager::BaseClient_OnSpectatorDisconnect), false);
#endif
}

void CForwardManager::UnhookClient(IClient *client)
{
	if (!m_HookedClients.erase(client))
		return;

	m_Commands.Forget(client);

	// Remove ExecuteStringCommand hook
	g_pSTVCommonHooks.RemoveSpectatorHook(this, client);

	void *pGameClient = (void *)((intptr_t)client - 4);
	if (m_bHasActivatePlayerOffset)
		SH_REMOVE_MANUALHOOK(CBaseClient_ActivatePlayer, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorPutInServer), true);
	
	SH_REMOVE_HOOK(IClient, Disconnect, client, SH_MEMBER(this, &CForwardManager::IClient_OnSpectatorDisconnect), false);
#ifndef WIN32
	if (m_bHasDisconnectOffset)
		SH_REMOVE_MANUALHOOK(CBaseClient_Disconnect, pGameClient, SH_MEMBER(this, &CForwardManager::BaseClient_OnSpectatorDisconnect), false);
#endif
}

void CForwardManager::CallOnServerStart(IHLTVServer *server)
{
	m_ServerStartFwd->PushCell(g_HLTVServers.GetInstanceNumber(server));
	m_ServerStartFwd->Execute();
}

void CForwardManager::CallOnServerShutdown(IHLTVServer *server)
{
	m_ServerShutdownFwd->PushCell(g_HLTVServers.GetInstanceNumber(server));
	m_ServerShutdownFwd->Execute();
}

#if SOURCE_ENGINE == SE_CSGO
static bool ExtractPlayerName(CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, char *name, int maxlen)
{
	if (!name || maxlen <= 0)
		return false;

	for (int i = 0; i < pSplitPlayerConnectVector.Count(); i++)
	{
		NetMsg_SplitPlayerConnect *split = pSplitPlayerConnectVector[i];
		if (!split->has_convars())
			continue;

		const CMsg_CVars cvars = split->convars();
		for (int c = 0; c < cvars.cvars_size(); c++)
		{
			const CMsg_CVars_CVar cvar = cvars.cvars(c);
			if (!cvar.has_name() || !cvar.has_value())
				continue;

			if (!strcmp(cvar.name().c_str(), "name"))
			{
				// V_strncpy always terminates the destination when maxlen is positive.
				V_strncpy(name, cvar.value().c_str(), maxlen);
				return true;
			}
		}
	}
	return false;
}
#endif

// Mimic Connect extension https://forums.alliedmods.net/showthread.php?t=162489
// Thanks asherkin!
#if SOURCE_ENGINE == SE_CSGO
// CHLTVServer::ConnectClient(ns_address const&, int, int, int, char const*, char const*, char const*, int, CUtlVector<CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true> *, CUtlMemory<CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true> *, int>> &, bool, CrossPlayPlatform_t, unsigned char const*, int)
IClient *CForwardManager::OnSpectatorConnect(const netadr_t & address, int nProtocol, int iChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie, CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, bool bUnknown, CrossPlayPlatform_t platform, const unsigned char *pUnknown, int iUnknown)
#elif SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
IClient *CForwardManager::OnSpectatorConnect(ConnectAddress address, int nProtocol, int iChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie, ConnectPlayers &pSplitPlayerConnectVector, bool bUnknown)
#else
IClient *CForwardManager::OnSpectatorConnect(netadr_t & address, int nProtocol, int iChallenge, int iClientChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie)
#endif
{
	if (META_RESULT_STATUS >= MRES_SUPERCEDE || !pCookie || cbCookie < static_cast<int>(sizeof(uint64)))
		RETURN_META_VALUE(MRES_IGNORED, nullptr);

#if SOURCE_ENGINE == SE_CSGO
	// CS:GO doesn't send the player name in pchName, but only in the client info convars.
	// Try to extract the name from the protobuf msg.
	char playerName[MAX_PLAYER_NAME_LENGTH];
	if (ExtractPlayerName(pSplitPlayerConnectVector, playerName, sizeof(playerName)))
		pchName = playerName;
#endif

	IServer *server = META_IFACEPTR(IServer);
	const auto wrapper = g_HLTVServers.Pin(g_HLTVServers.GetWrapper(server));
	if (!wrapper)
		RETURN_META_VALUE(MRES_IGNORED, nullptr);

	char ipString[16];
	V_snprintf(ipString, sizeof(ipString), "%u.%u.%u.%u", address.ip[0], address.ip[1], address.ip[2], address.ip[3]);
	sourcetv::ConnectionBuffers buffers(pchPassword);

	m_SpectatorPreConnectFwd->PushString(pchName ? pchName : "");
	m_SpectatorPreConnectFwd->PushStringEx(buffers.password.data(), buffers.password.size(), SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorPreConnectFwd->PushString(ipString);
	m_SpectatorPreConnectFwd->PushStringEx(buffers.rejection.data(), buffers.rejection.size(), SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);

	cell_t retVal = 1;
	m_SpectatorPreConnectFwd->Execute(&retVal);
	buffers.Terminate();
	if (!wrapper->IsActive())
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);

	if (retVal == 0)
	{
		if (m_bHasRejectConnectionOffset)
		{
#if SOURCE_ENGINE == SE_LEFT4DEAD2
			const auto rejection = buffers.L4D2RejectionText();
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, "%s", rejection.data());
#elif SOURCE_ENGINE == SE_CSGO || SOURCE_ENGINE == SE_LEFT4DEAD
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, "%s", buffers.rejection.data());
#else
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, iClientChallenge, buffers.rejection.data());
#endif
		}
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);
	}

	// Continue through other pre-hooks before the engine call.
#if SOURCE_ENGINE == SE_CSGO
	IClient *client = SOURCETV_RECALL_MANUAL(CHLTVServer_ConnectClient, address, nProtocol, iChallenge, nAuthProtocol, pchName, buffers.password.data(), pCookie, cbCookie, pSplitPlayerConnectVector, bUnknown, platform, pUnknown, iUnknown);
#elif SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
	IClient *client = SOURCETV_RECALL_MANUAL(CHLTVServer_ConnectClient, address, nProtocol, iChallenge, nAuthProtocol, pchName, buffers.password.data(), pCookie, cbCookie, pSplitPlayerConnectVector, bUnknown);
#else
	IClient *client = SOURCETV_RECALL_MANUAL(CHLTVServer_ConnectClient, address, nProtocol, iChallenge, iClientChallenge, nAuthProtocol, pchName, buffers.password.data(), pCookie, cbCookie);
#endif

	if (!client || !wrapper->IsActive())
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);

	HookClient(wrapper.get(), client);
	const int clientIndex = client->GetPlayerSlot() + 1;
	wrapper->GetClient(clientIndex)->Initialize(ipString, buffers.password.data(), client);

	m_SpectatorConnectedFwd->PushCell(clientIndex);
	m_SpectatorConnectedFwd->Execute();

	const auto hooked = m_HookedClients.find(client);
	RETURN_META_VALUE(MRES_SUPERCEDE,
		wrapper->IsActive() && hooked != m_HookedClients.end() && hooked->second == wrapper.get() ? client : nullptr);
}

// Force steam authentication
// Thanks GoD-Tony :)
int CForwardManager::OnGetChallengeType(const netadr_t &address)
{
	if (!tv_force_steamauth.GetBool())
		RETURN_META_VALUE(MRES_IGNORED, k_EAuthProtocolHashedCDKey);

	RETURN_META_VALUE(MRES_SUPERCEDE, k_EAuthProtocolSteam);
}

void CForwardManager::BaseClient_OnSpectatorDisconnect(const char *reason)
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META(MRES_IGNORED);

	IClient *client = (IClient *)((intptr_t)pGameClient + 4);
	HandleSpectatorDisconnect(client, reason);

	RETURN_META(MRES_SUPERCEDE);
}

void CForwardManager::IClient_OnSpectatorDisconnect(const char *reason)
{
	IClient *client = META_IFACEPTR(IClient);
	if (!client)
		RETURN_META(MRES_IGNORED);

	HandleSpectatorDisconnect(client, reason);

	RETURN_META(MRES_SUPERCEDE);
}

void CForwardManager::HandleSpectatorDisconnect(IClient *client, const char *reason)
{
	const auto hooked = m_HookedClients.find(client);
	if (hooked == m_HookedClients.end())
		return;
	const auto owner = g_HLTVServers.Pin(hooked->second);
	if (!owner || !owner->IsActive())
		return;
	UnhookClient(client);

	char disconnectReason[255]{};
	V_strncpy(disconnectReason, reason ? reason : "", sizeof(disconnectReason));
	int clientIndex = client->GetPlayerSlot() + 1;

	m_SpectatorDisconnectFwd->PushCell(clientIndex);
	m_SpectatorDisconnectFwd->PushStringEx(disconnectReason, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorDisconnectFwd->Execute();
	disconnectReason[sizeof(disconnectReason) - 1] = '\0';
	if (!owner->IsActive() || m_HookedClients.count(client))
		return;

	IServer *server = owner->GetBaseServer();
	const int slot = clientIndex - 1;
	if (slot < 0 || slot >= server->GetClientCount() || server->GetClient(slot) != client)
		return;

	// A listener may already have disconnected this spectator.
	if (client->IsConnected()) {
#if SOURCE_ENGINE == SE_CSGO
		SH_CALL(client, &IClient::Disconnect)(disconnectReason);
#else
		SH_CALL(client, &IClient::Disconnect)("%s", disconnectReason);
#endif
	}

	if (!owner->IsActive() || m_HookedClients.count(client))
		return;

	m_SpectatorDisconnectedFwd->PushCell(clientIndex);
	m_SpectatorDisconnectedFwd->PushString(disconnectReason);
	m_SpectatorDisconnectedFwd->Execute();
}

void CForwardManager::OnSpectatorPutInServer()
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META(MRES_IGNORED);

	// The IClient vtable is + 4 from the CBaseClient vtable due to multiple inheritance.
	IClient *client = (IClient *)((intptr_t)pGameClient + 4);

	m_SpectatorPutInServerFwd->PushCell(client->GetPlayerSlot() + 1);
	m_SpectatorPutInServerFwd->Execute();

	RETURN_META(MRES_IGNORED);
}

bool CForwardManager::BaseClient_OnSpectatorExecuteStringCommand(const char *s)
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META_VALUE(MRES_IGNORED, true);

	IClient *client = (IClient *)((intptr_t)pGameClient + 4);
	HandleSpectatorExecuteStringCommand(client, s);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool CForwardManager::IClient_OnSpectatorExecuteStringCommand(const char *s)
{
	IClient *client = META_IFACEPTR(IClient);
	if (!client)
		RETURN_META_VALUE(MRES_IGNORED, true);

	HandleSpectatorExecuteStringCommand(client, s);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void CForwardManager::HandleSpectatorExecuteStringCommand(IClient *client, const char *s)
{
	const auto hooked = m_HookedClients.find(client);
	if (hooked == m_HookedClients.end())
		return;

	std::optional<sourcetv::ChatMessage> chat;
	CCommand args;
	if (s && s[0] && args.Tokenize(s) && args.ArgC() > 1 && !Q_stricmp(args[0], "say")) {
		chat = sourcetv::ChatMessage{client->GetPlayerSlot() + 1, client->GetClientName(), args[1]};
	}
	m_Commands.Begin(hooked->second, client, std::move(chat));
}

bool CForwardManager::IClient_OnSpectatorExecuteStringCommand_Post(const char *s)
{
	m_Commands.End(META_IFACEPTR(IClient));
	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool CForwardManager::BaseClient_OnSpectatorExecuteStringCommand_Post(const char *s)
{
	void *gameClient = META_IFACEPTR(void);
	if (gameClient)
		m_Commands.End(reinterpret_cast<IClient*>(reinterpret_cast<intptr_t>(gameClient) + 4));
	RETURN_META_VALUE(MRES_IGNORED, true);
}

DETOUR_DECL_MEMBER2(DetourHLTVServer_BroadcastLocalChat, void, const char *, chat, const char *, chatgroup)
{
	// Check on every call so plugins loaded or unloaded at runtime are reflected
	// without keeping a listener-count cache that could become stale.
	if (!g_pSTVForwards.HasSpectatorChatListeners())
	{
		DETOUR_MEMBER_CALL(DetourHLTVServer_BroadcastLocalChat)(chat, chatgroup);
		return;
	}

	const auto& binding = HltvServerBaseBinding();
	IServer* server = binding ? static_cast<IServer*>(binding->ServerBase(this)) : nullptr;
	const auto wrapper = g_HLTVServers.Pin(g_HLTVServers.GetWrapper(server));
	if (!wrapper) {
		DETOUR_MEMBER_CALL(DetourHLTVServer_BroadcastLocalChat)(chat, chatgroup);
		return;
	}
	const auto command = g_pSTVForwards.CurrentChat(wrapper.get());
	const int clientIndex = command ? command->index : 0;

	char chatBuffer[256], groupBuffer[256];
	ke::SafeStrcpy(chatBuffer, sizeof(chatBuffer), command ? command->text.c_str() : chat);
	ke::SafeStrcpy(groupBuffer, sizeof(groupBuffer), chatgroup);

	if (g_pSTVForwards.CallOnSpectatorChatMessage(clientIndex, chatBuffer, sizeof(chatBuffer), groupBuffer, sizeof(groupBuffer)))
		return;
	if (!wrapper->IsActive())
		return;

	chatBuffer[sizeof(chatBuffer) - 1] = '\0';
	groupBuffer[sizeof(groupBuffer) - 1] = '\0';
	char messageBuffer[1024];
	chat = chatBuffer;
	if (command) {
		ke::SafeSprintf(messageBuffer, sizeof(messageBuffer), "%s : %s", command->name.c_str(), chatBuffer);
		chat = messageBuffer;
	}

	DETOUR_MEMBER_CALL(DetourHLTVServer_BroadcastLocalChat)(chat, groupBuffer);

	if (wrapper->IsActive())
		g_pSTVForwards.CallOnSpectatorChatMessage_Post(clientIndex, chatBuffer, groupBuffer);
}

void CForwardManager::CreateBroadcastLocalChatDetour()
{
	if (m_bBroadcastLocalChatDetoured || !HltvServerBaseBinding())
		return;

	m_DBroadcastLocalChat = DETOUR_CREATE_MEMBER(DetourHLTVServer_BroadcastLocalChat, "CHLTVServer::BroadcastLocalChat");

	if (m_DBroadcastLocalChat != nullptr)
	{
		m_DBroadcastLocalChat->EnableDetour();
		m_bBroadcastLocalChatDetoured = true;
		return;
	}
	smutils->LogError(myself, "CHLTVServer::BroadcastLocalChat detour could not be initialized.");
}

void CForwardManager::RemoveBroadcastLocalChatDetour()
{
	if (m_DBroadcastLocalChat != nullptr)
	{
		m_DBroadcastLocalChat->Destroy();
		m_DBroadcastLocalChat = nullptr;
	}
	m_bBroadcastLocalChatDetoured = false;
}

bool CForwardManager::CallOnSpectatorChatMessage(int clientIndex, char *msg, int msglen, char *chatgroup, int grouplen)
{
	m_SpectatorChatMessageFwd->PushCell(clientIndex);
	m_SpectatorChatMessageFwd->PushStringEx(msg, msglen, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorChatMessageFwd->PushStringEx(chatgroup, grouplen, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);

	cell_t res = Pl_Continue;
	m_SpectatorChatMessageFwd->Execute(&res);
	if (res >= Pl_Handled)
		return true;
	return false;
}

void CForwardManager::CallOnSpectatorChatMessage_Post(int clientIndex, const char *msg, const char *chatgroup)
{
	m_SpectatorChatMessagePostFwd->PushCell(clientIndex);
	m_SpectatorChatMessagePostFwd->PushString(msg);
	m_SpectatorChatMessagePostFwd->PushString(chatgroup);
	m_SpectatorChatMessagePostFwd->Execute();
}

bool CForwardManager::HasSpectatorChatListeners()
{
	return m_Initialized && (m_SpectatorChatMessageFwd->GetFunctionCount() > 0 ||
		m_SpectatorChatMessagePostFwd->GetFunctionCount() > 0);
}

// These two hooks are actually only hooked on windows.
void CForwardManager::OnStartRecording_Post(const char *filename, bool bContinuously)
{
	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	CallOnStartRecording(recorder, filename, bContinuously);
	RETURN_META(MRES_IGNORED);
}

void CForwardManager::OnStartRecording_Pre(const char *filename, bool bContinuously)
{
	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	CaptureRecordingStartState(recorder);
	RETURN_META(MRES_IGNORED);
}

#if SOURCE_ENGINE == SE_CSGO
void CForwardManager::OnStopRecording(CGameInfo const *info)
#else
void CForwardManager::OnStopRecording()
#endif
{
	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	if (!CallOnStopRecording(recorder))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

#if SOURCE_ENGINE == SE_CSGO
void CForwardManager::OnStopRecording_Post(CGameInfo const *info)
#else
void CForwardManager::OnStopRecording_Post()
#endif
{
	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	CallOnRecordingFileClosed(recorder);
	RETURN_META(MRES_IGNORED);
}

namespace recording = sourcetv::recording;

static recording::Snapshot CaptureRecorder(IDemoRecorder *recorder, bool withTick)
{
	recording::Snapshot result;
	result.active = recorder->IsRecording();
	if (auto* wrapper = g_HLTVServers.GetWrapper(recorder)) {
		result.instance = wrapper->GetInstanceNumber();
		if (result.active) {
			const char* filename = wrapper->GetDemoFileName();
			if (filename)
				result.filename = filename;
		}
	}
	if (withTick && result.active)
		result.tick = recorder->GetRecordingTick();
	return result;
}

void CForwardManager::CallOnStartRecording(IDemoRecorder *recorder, const char *filename, bool bContinuously)
{
	if (!m_RecordingSessions.Current(recorder).lifetime)
		return;
	const auto frame = m_RecordingSessions.EndStart(recorder);
	recording::Snapshot effective;
	if (frame)
		effective = CaptureRecorder(recorder, false);
	if (effective.filename.empty() && filename)
		effective.filename = filename;
	const auto revision = m_RecordingSessions.Current(recorder);
	const bool newFile = frame && frame->revision == revision && effective.active &&
		(!frame->before.active || stricmp(frame->before.filename.c_str(), effective.filename.c_str()) != 0);
	if (newFile)
		m_RecordingSessions.ConfirmNewFile(recorder, revision);

	if (m_StartRecordingFwd->GetFunctionCount() > 0)
	{
		m_StartRecordingFwd->PushCell(effective.instance);
		m_StartRecordingFwd->PushString(effective.filename.c_str());
		m_StartRecordingFwd->Execute();
	}

	if (newFile && m_RecordingSessions.Current(recorder) == revision && recorder->IsRecording()
		&& m_RecordingFileStartedFwd->GetFunctionCount() > 0)
	{
		m_RecordingFileStartedFwd->PushCell(effective.instance);
		m_RecordingFileStartedFwd->PushString(effective.filename.c_str());
		m_RecordingFileStartedFwd->PushCell(bContinuously ? 1 : 0);
		m_RecordingFileStartedFwd->Execute();
	}
}

void CForwardManager::CaptureRecordingStartState(IDemoRecorder *recorder)
{
	if (m_RecordingSessions.Current(recorder).lifetime)
		m_RecordingSessions.BeginStart(recorder, CaptureRecorder(recorder, false));
}

bool CForwardManager::CallOnStopRecording(IDemoRecorder *recorder)
{
	const auto before = m_RecordingSessions.Current(recorder);
	if (!before.lifetime)
		return true;
	const auto snapshot = CaptureRecorder(recorder, true);
	m_RecordingSessions.BeginClose(recorder, snapshot);

	if (snapshot.active && m_StopRecordingFwd->GetFunctionCount() > 0)
	{
		m_StopRecordingFwd->PushCell(snapshot.instance);
		m_StopRecordingFwd->PushString(snapshot.filename.c_str());
		m_StopRecordingFwd->PushCell(snapshot.tick);
		m_StopRecordingFwd->Execute();
	}
	return m_RecordingSessions.CloseStillCurrent(recorder, before);
}

void CForwardManager::CallOnRecordingFileClosed(IDemoRecorder *recorder)
{
	if (!m_RecordingSessions.HasPendingClose(recorder))
		return;
	const bool active = recorder->IsRecording();
	auto* wrapper = active ? g_HLTVServers.GetWrapper(recorder) : nullptr;
	const char* filename = wrapper ? wrapper->GetDemoFileName() : nullptr;
	auto closed = m_RecordingSessions.EndClose(recorder, active, filename,
		[](const char* left, const char* right) { return stricmp(left, right) == 0; });
	if (!closed || m_RecordingFileClosedFwd->GetFunctionCount() == 0)
		return;

	m_RecordingFileClosedFwd->PushCell(closed->instance);
	m_RecordingFileClosedFwd->PushString(closed->filename.c_str());
	m_RecordingFileClosedFwd->PushCell(closed->tick);
	m_RecordingFileClosedFwd->Execute();
}

void CForwardManager::CallOnRecorderAvailabilityChanged(int instance, bool available)
{
	if (!m_Initialized || m_RecorderAvailabilityChangedFwd->GetFunctionCount() == 0)
		return;
	m_RecorderAvailabilityChangedFwd->PushCell(instance);
	m_RecorderAvailabilityChangedFwd->PushCell(available ? 1 : 0);
	m_RecorderAvailabilityChangedFwd->Execute();
}

// Only need to detour these on Linux. Windows always uses the vtable.
#ifndef WIN32
DETOUR_DECL_MEMBER2(DetourHLTVStartRecording, void, const char *, filename, bool, bContinuously)
{
	IDemoRecorder *recorder = (IDemoRecorder *)this;
	g_pSTVForwards.CaptureRecordingStartState(recorder);
	// Call the original first.
	DETOUR_MEMBER_CALL(DetourHLTVStartRecording)(filename, bContinuously);
	
	g_pSTVForwards.CallOnStartRecording(recorder, filename, bContinuously);
}

#if SOURCE_ENGINE == SE_CSGO
DETOUR_DECL_MEMBER1(DetourHLTVStopRecording, void, CGameInfo const *, info)
#else
DETOUR_DECL_MEMBER0(DetourHLTVStopRecording, void)
#endif
{
	IDemoRecorder *recorder = (IDemoRecorder *)this;
	if (g_pSTVForwards.CallOnStopRecording(recorder))
	{
#if SOURCE_ENGINE == SE_CSGO
		DETOUR_MEMBER_CALL(DetourHLTVStopRecording)(info);
#else
		DETOUR_MEMBER_CALL(DetourHLTVStopRecording)();
#endif
	}
	g_pSTVForwards.CallOnRecordingFileClosed(recorder);
}

bool CForwardManager::InstallRecordingDetours()
{
	if (m_RecordingDetours[0])
		return true;

	return sourcetv::recording::InstallHooks(m_RecordingDetours, [](std::size_t index) {
		CDetour *hook;
		if (index == 0) {
			hook = DETOUR_CREATE_MEMBER(DetourHLTVStartRecording, "CHLTVDemoRecorder::StartRecording")
		} else {
			hook = DETOUR_CREATE_MEMBER(DetourHLTVStopRecording, "CHLTVDemoRecorder::StopRecording")
		}
		return sourcetv::recording::EnableRecordingHook(hook);
	}, [](CDetour *hook) { hook->Destroy(); });
}
#endif
