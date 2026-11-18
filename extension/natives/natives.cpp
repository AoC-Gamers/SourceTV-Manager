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

#include "print_recipient.h"
#include "extension.h"
#include "core/local_hltv_broadcast.h"
#include "natives/natives.h"
#include "natives/recording_path.h"
#include "natives/pawn_boundary.h"
#include "core/hltvserverwrapper.h"
#include "hooks/forwards.h"

namespace pawn = sourcetv::pawn;

extern const sp_nativeinfo_t sourcetv_natives[];

namespace recording = sourcetv::recording;

// SourceMod's formatter assumes the format address already resolves.
static bool HasFormatString(IPluginContext& context, const cell_t *params, cell_t index)
{
	const char *format;
	return params[0] >= index && pawn::ReadString(context, params[index], format);
}

// Print to client consoles
SH_DECL_MANUALHOOK0_void_vafmt(CBaseServer_BroadcastPrintf, 0, 0, 0);
// For print to chat
SH_DECL_MANUALHOOK1_void(CBaseClient_FireGameEvent, 0, 0, 0, IGameEvent *);

bool g_bHasBroadcastPrintfOffset = false;
bool g_bHasClientFireGameEventOffset = false;

void SetupNativeCalls()
{
	int offset = -1;
	if (!g_pGameConf->GetOffset("CBaseServer::BroadcastPrintf", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseServer::BroadcastPrintf offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseServer_BroadcastPrintf, offset, 0, 0);
		g_bHasBroadcastPrintfOffset = true;
	}

	offset = -1;
	if (!g_pGameConf->GetOffset("CBaseClient::FireGameEvent", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::FireGameEvent offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_FireGameEvent, offset, 0, 0);
		g_bHasClientFireGameEventOffset = true;
	}
}

// native SourceTV_GetServerInstanceCount();
static cell_t Native_GetServerInstanceCount(IPluginContext *pContext, const cell_t *params)
{
#if SOURCE_ENGINE == SE_CSGO
	return hltvdirector->GetHLTVServerCount();
#else
	return hltvdirector->GetHLTVServer() ? 1 : 0;
#endif
}

// native SourceTV_SelectServerInstance();
static cell_t Native_SelectServerInstance(IPluginContext *pContext, const cell_t *params)
{
#if SOURCE_ENGINE == SE_CSGO
	if (params[1] < 0 || params[1] >= hltvdirector->GetHLTVServerCount())
	{
		pContext->ReportError("Invalid HLTV server instance number (%d).", params[1]);
		return 0;
	}
	g_STVManager.SelectSourceTVServer(hltvdirector->GetHLTVServer(params[1]));
#else
	if (params[1] != 0 || !hltvdirector->GetHLTVServer())
	{
		pContext->ReportError("Invalid HLTV server instance number (%d).", params[1]);
		return 0;
	}
	// There only is one hltv server.
	g_STVManager.SelectSourceTVServer(hltvdirector->GetHLTVServer());
#endif

	return 0;
}

// native SourceTV_GetSelectedServerInstance();
static cell_t Native_GetSelectedServerInstance(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetInstanceNumber();
}

// native SourceTV_IsActive();
static cell_t Native_IsActive(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	return hltvserver->GetBaseServer()->IsActive();
}

// native SourceTV_IsMasterProxy();
static cell_t Native_IsMasterProxy(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	return hltvserver->GetHLTVServer()->IsMasterProxy();
}

// native bool:SourceTV_GetServerIP(String:ip[], maxlen);
static cell_t Native_GetServerIP(IPluginContext *pContext, const cell_t *params)
{
	char *destination;
	if (!pawn::ResolveBuffer(*pContext, params[1], params[2], destination))
		return 0;

	if (hltvserver == nullptr)
		return 0;

	const netadr_t *adr = hltvserver->GetHLTVServer()->GetRelayAddress();

	char buf[16];
	V_snprintf(buf, sizeof(buf), "%d.%d.%d.%d", adr->ip[0], adr->ip[1], adr->ip[2], adr->ip[3]);
	return pawn::WriteString(*pContext, params[1], params[2], destination, buf);
}

// native SourceTV_GetServerPort();
static cell_t Native_GetServerPort(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

#if SOURCE_ENGINE == SE_TF2 || SOURCE_ENGINE == SE_HL2DM
	return hltvserver->GetBaseServer()->GetLocalUDPPort();
#else
	return hltvserver->GetBaseServer()->GetUDPPort();
#endif
}

// native SourceTV_GetBotIndex();
static cell_t Native_GetBotIndex(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	return hltvserver->GetHLTVServer()->GetHLTVSlot() + 1;
}

// native bool:SourceTV_GetLocalStats(&proxies, &slots, &specs);
static cell_t Native_GetLocalStats(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t *plProxies, *plSlots, *plSpecs;
	if (!pawn::ResolveCells(*pContext, params[1], plProxies)
		|| !pawn::ResolveCells(*pContext, params[2], plSlots)
		|| !pawn::ResolveCells(*pContext, params[3], plSpecs))
		return 0;

	int proxies, slots, specs;
	hltvserver->GetHLTVServer()->GetLocalStats(proxies, slots, specs);

	*plProxies = proxies;
	*plSlots = slots;
	*plSpecs = specs;
	return 1;
}

// native bool:SourceTV_GetGlobalStats(&proxies, &slots, &specs);
static cell_t Native_GetGlobalStats(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t *plProxies, *plSlots, *plSpecs;
	if (!pawn::ResolveCells(*pContext, params[1], plProxies)
		|| !pawn::ResolveCells(*pContext, params[2], plSlots)
		|| !pawn::ResolveCells(*pContext, params[3], plSpecs))
		return 0;

	int proxies, slots, specs;
	hltvserver->GetHLTVServer()->GetGlobalStats(proxies, slots, specs);

	*plProxies = proxies;
	*plSlots = slots;
	*plSpecs = specs;
	return 1;
}

// native SourceTV_GetBroadcastTick();
static cell_t Native_GetBroadcastTick(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvdirector->GetDirectorTick();
}

// native Float:SourceTV_GetDelay();
static cell_t Native_GetDelay(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return pawn::FloatCell(hltvdirector->GetDelay());
}

void ShutdownNativeCalls()
{
	g_bHasBroadcastPrintfOffset = false;
	g_bHasClientFireGameEventOffset = false;
	ShutdownLocalHltvBroadcast();
}

// native bool:SourceTV_BroadcastScreenMessage(bool:bLocalOnly, const String:format[], any:...);
static cell_t Native_BroadcastScreenMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;
	if (!HasFormatString(*pContext, params, 2))
		return 0;

	pawn::BroadcastTarget target;
	if (!pawn::ReadBroadcastTarget(params[1], target))
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 2);
		if (eh.HasException())
			return 0;
	}

	IGameEvent *msg = gameevents->CreateEvent("hltv_message", true);
	if (!msg)
		return 0;

#if SOURCE_ENGINE == SE_CSGO
	wchar_t wBuffer[1024];
	V_UTF8ToUnicode(buffer, wBuffer, sizeof(wBuffer));
	msg->SetWString("text", wBuffer);
#else
	msg->SetString("text", buffer);
#endif

	int ret = 1;
	bool bLocalOnly = target == pawn::BroadcastTarget::OnlyLocal;
	if (!bLocalOnly)
		hltvserver->GetHLTVServer()->BroadcastEvent(msg);
	else
		ret = BroadcastEventLocal(hltvserver->GetHLTVServer(), msg, false);

	gameevents->FreeEvent(msg);

	return ret;
}

// native bool:SourceTV_BroadcastConsoleMessage(const String:format[], any:...);
static cell_t Native_BroadcastConsoleMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;
	if (!HasFormatString(*pContext, params, 1))
		return 0;

	if (!g_bHasBroadcastPrintfOffset)
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 1);
		if (eh.HasException())
			return 0;
	}

	SH_MCALL(hltvserver->GetBaseServer(), CBaseServer_BroadcastPrintf)("%s\n", buffer);

	return 1;
}

// native bool:SourceTV_BroadcastChatMessage(bool:bLocalOnly, const String:format[], any:...);
static cell_t Native_BroadcastChatMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;
	if (!HasFormatString(*pContext, params, 2))
		return 0;

	pawn::BroadcastTarget target;
	if (!pawn::ReadBroadcastTarget(params[1], target))
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 2);
		if (eh.HasException())
			return 0;
	}

	IGameEvent *msg = gameevents->CreateEvent("hltv_chat", true);
	if (!msg)
		return 0;

#if SOURCE_ENGINE == SE_CSGO
	wchar_t wBuffer[1024];
	V_UTF8ToUnicode(buffer, wBuffer, sizeof(wBuffer));
	msg->SetWString("text", wBuffer);
#else
	msg->SetString("text", buffer);
#endif

	int ret = 1;
	bool bLocalOnly = target == pawn::BroadcastTarget::OnlyLocal;
	if (!bLocalOnly)
		hltvserver->GetHLTVServer()->BroadcastEvent(msg);
	else
		ret = BroadcastEventLocal(hltvserver->GetHLTVServer(), msg, false);

	gameevents->FreeEvent(msg);

	return ret;
}

// native SourceTV_GetViewEntity();
static cell_t Native_GetViewEntity(IPluginContext *pContext, const cell_t *params)
{
	return hltvdirector->GetPVSEntity();
}

// native SourceTV_GetViewOrigin();
static cell_t Native_GetViewOrigin(IPluginContext *pContext, const cell_t *params)
{
	cell_t *output;
	if (!pawn::ResolveCells(*pContext, params[1], output, 3))
		return 0;

	const Vector origin = hltvdirector->GetPVSOrigin();
	output[0] = pawn::FloatCell(origin.x);
	output[1] = pawn::FloatCell(origin.y);
	output[2] = pawn::FloatCell(origin.z);
	return 0;
}

// native bool:SourceTV_ForceFixedCameraShot(Float:pos[], Float:angle[], iTarget, Float:fov, Float:fDuration);
static cell_t Native_ForceFixedCameraShot(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t *pos, *angle;
	if (!pawn::ResolveCells(*pContext, params[1], pos, 3)
		|| !pawn::ResolveCells(*pContext, params[2], angle, 3))
		return 0;

	const Vector vPos(pawn::ReadFloat(pos[0]), pawn::ReadFloat(pos[1]), pawn::ReadFloat(pos[2]));
	const float theta = pawn::ReadFloat(angle[0]);
	const float phi = pawn::ReadFloat(angle[1]);
	const float fov = pawn::ReadFloat(params[4]);
	const float duration = pawn::ReadFloat(params[5]);
	if (!pawn::FitsInt(vPos.x) || !pawn::FitsInt(vPos.y) || !pawn::FitsInt(vPos.z)
		|| !pawn::FitsInt(theta) || !pawn::FitsInt(phi) || !std::isfinite(fov)
		|| !std::isfinite(duration))
		return 0;

	// Validate entities.
	if (params[3] != 0 && !gamehelpers->ReferenceToEntity(params[3]))
	{
		pContext->ReportError("Invalid target (%d - %d)", gamehelpers->ReferenceToIndex(params[3]), params[3]);
		return 0;
	}

	int nextTick;
	if (!pawn::NextThinkTick(duration, gpGlobals->interval_per_tick, hltvdirector->GetDirectorTick(), nextTick))
		return 0;

	// Update director state like it would do itself.
	g_HLTVDirectorWrapper.SetPVSEntity(0);
	g_HLTVDirectorWrapper.SetPVSOrigin(vPos);

	IGameEvent *shot = gameevents->CreateEvent("hltv_fixed", true);
	if (!shot)
		return 0;
	
	shot->SetInt("posx", vPos.x);
	shot->SetInt("posy", vPos.y);
	shot->SetInt("posz", vPos.z);
	shot->SetInt("theta", theta);
	shot->SetInt("phi", phi);
	shot->SetInt("target", params[3] ? gamehelpers->ReferenceToIndex(params[3]) : DIRECTOR_NO_TARGET);
	shot->SetFloat("fov", fov);

	hltvserver->GetHLTVServer()->BroadcastEvent(shot);
	gameevents->FreeEvent(shot);

	// Prevent auto director from changing shots until we allow it to again.
	if (pawn::NextThinkTick(duration, gpGlobals->interval_per_tick, hltvdirector->GetDirectorTick(), nextTick))
		g_HLTVDirectorWrapper.SetNextThinkTick(nextTick);

	return 1;
}

// native bool:SourceTV_ForceChaseCameraShot(iTarget1, iTarget2, distance, phi, theta, bool:bInEye, Float:fDuration);
static cell_t Native_ForceChaseCameraShot(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	const float duration = pawn::ReadFloat(params[7]);
	if (!std::isfinite(duration))
		return 0;

	// Validate entities.
	if (!gamehelpers->ReferenceToEntity(params[1]))
	{
		pContext->ReportError("Invalid target1 (%d - %d)", gamehelpers->ReferenceToIndex(params[1]), params[1]);
		return 0;
	}

	if (params[2] != 0 && !gamehelpers->ReferenceToEntity(params[2]))
	{
		pContext->ReportError("Invalid target2 (%d - %d)", gamehelpers->ReferenceToIndex(params[2]), params[2]);
		return 0;
	}

	int nextTick;
	if (!pawn::NextThinkTick(duration, gpGlobals->interval_per_tick, hltvdirector->GetDirectorTick(), nextTick))
		return 0;

	IGameEvent *shot = gameevents->CreateEvent("hltv_chase", true);
	if (!shot)
		return 0;

	shot->SetInt("target1", gamehelpers->ReferenceToIndex(params[1]));
	shot->SetInt("target2", params[2] ? gamehelpers->ReferenceToIndex(params[2]) : DIRECTOR_NO_TARGET);
	shot->SetInt("distance", params[3]);
	shot->SetInt("phi", params[4]); // hi/low
	shot->SetInt("theta", params[5]); // left/right
	shot->SetInt("ineye", params[6] ? 1 : 0);

	// Update director state
	g_HLTVDirectorWrapper.SetPVSEntity(gamehelpers->ReferenceToIndex(params[1]));

	hltvserver->GetHLTVServer()->BroadcastEvent(shot);
	gameevents->FreeEvent(shot);

	// Prevent auto director from changing shots until we allow it to again.
	if (pawn::NextThinkTick(duration, gpGlobals->interval_per_tick, hltvdirector->GetDirectorTick(), nextTick))
		g_HLTVDirectorWrapper.SetNextThinkTick(nextTick);

	return 1;
}

// native bool:SourceTV_IsRecording();
static cell_t Native_IsRecording(IPluginContext *pContext, const cell_t *params)
{
	if (!hltvserver || !hltvserver->GetDemoRecorder())
		return 0;
	return hltvserver->GetDemoRecorder()->IsRecording();
}

namespace {

struct RecorderPort {
	HLTVServerWrapper* wrapper;
	IDemoRecorder* recorder;

	bool IsActive() const { return wrapper->GetBaseServer()->IsActive(); }
	bool IsMaster() const { return wrapper->GetHLTVServer()->IsMasterProxy(); }
	bool IsRecording() const { return recorder->IsRecording(); }
	const char* FileName() const { return wrapper->GetDemoFileName(); }
	int Instance() const { return wrapper->GetInstanceNumber(); }
	int Tick() const { return recorder->GetRecordingTick(); }
	bool Equal(const char* left, const char* right) const { return stricmp(left, right) == 0; }

	bool HasDemoSuffix(const char* path) const
	{
		const char* ext = libsys->GetFileExtension(path);
		return ext && Equal(ext, "dem");
	}

	bool CanUsePath(const char* path) const
	{
#if SOURCE_ENGINE == SE_CSGO
		if (hltvdirector->GetHLTVServerCount() > 1) {
			for (int i = 0; i < hltvdirector->GetHLTVServerCount(); ++i) {
				auto* server = hltvdirector->GetHLTVServer(i);
				if (server->IsRecording() && Equal(path, server->GetRecordingDemoFilename()))
					return false;
			}
		}
#endif
		return true;
	}

	void Start(const char* path, bool continuous) const { recorder->StartRecording(path, continuous); }
	void Stop() const
	{
#if SOURCE_ENGINE == SE_CSGO
		recorder->StopRecording(NULL);
#else
		recorder->StopRecording();
#endif
	}
};

RecorderPort SelectedRecorder()
{
	auto* wrapper = hltvserver;
	return {wrapper, wrapper ? wrapper->GetDemoRecorder() : nullptr};
}

recording::Plugin Caller(IPluginContext* context)
{
	IPlugin* plugin = plsys->FindPluginByContext(context->GetContext());
	return {plugin, plugin ? plugin->GetSerial() : 0};
}

} // namespace

static cell_t Native_StartRecording(IPluginContext *pContext, const cell_t *params)
{
	const char *filename;
	if (!pawn::ReadString(*pContext, params[1], filename))
		return 0;

	auto port = SelectedRecorder();
	char path[PLATFORM_MAX_PATH];
	return recording::Start(g_pSTVForwards.RecordingSessions(), port, port.recorder, {}, false,
		false, filename, path, sizeof(path)).result == recording::StartResult::Started;
}

static cell_t Native_StartRecordingEx(IPluginContext *pContext, const cell_t *params)
{
	cell_t *ownerTokenOutput;
	const char *filename;
	if (!pawn::ResolveCells(*pContext, params[3], ownerTokenOutput)
		|| !pawn::ReadString(*pContext, params[1], filename))
		return static_cast<cell_t>(recording::StartResult::InvalidPath);
	*ownerTokenOutput = 0;

	auto port = SelectedRecorder();
	char path[PLATFORM_MAX_PATH];
	const auto result = recording::Start(g_pSTVForwards.RecordingSessions(), port, port.recorder,
		Caller(pContext), true, params[2] != 0, filename, path, sizeof(path));
	*ownerTokenOutput = result.token;
	return static_cast<cell_t>(result.result);
}

static cell_t Native_StopOwnedRecording(IPluginContext *pContext, const cell_t *params)
{
	auto port = SelectedRecorder();
	return recording::Stop(g_pSTVForwards.RecordingSessions(), port, port.recorder, Caller(pContext), params[1], true);
}

static cell_t Native_GetRecordingSnapshot(IPluginContext *pContext, const cell_t *params)
{
	cell_t *recording, *ownerToken, *instance, *tick;
	const cell_t capacity = params[5];
	char *filename = nullptr;
	if (!pawn::ResolveCells(*pContext, params[1], recording)
		|| !pawn::ResolveCells(*pContext, params[2], ownerToken)
		|| !pawn::ResolveCells(*pContext, params[3], instance)
		|| !pawn::ResolveCells(*pContext, params[6], tick)
		|| capacity < 0
		|| (capacity > 0 && !pawn::ResolveBuffer(*pContext, params[4], capacity, filename)))
		return 0;

	auto port = SelectedRecorder();
	const auto status = recording::ReadStatus(g_pSTVForwards.RecordingSessions(), port, port.recorder,
		Caller(pContext), filename != nullptr);
	if (filename && !pawn::WriteString(*pContext, params[4], capacity, filename, status.current.filename.c_str()))
		return 0;

	*recording = 0;
	*ownerToken = 0;
	*instance = -1;
	*tick = -1;
	if (!status.available)
		return 0;
	*instance = status.current.instance;
	if (status.current.active)
	{
		*recording = 1;
		if (status.token != 0)
			*ownerToken = status.token;
		*tick = status.current.tick;
	}
	return 1;
}

// native bool:SourceTV_StopRecording();
static cell_t Native_StopRecording(IPluginContext *pContext, const cell_t *params)
{
	auto port = SelectedRecorder();
	return recording::Stop(g_pSTVForwards.RecordingSessions(), port, port.recorder, Caller(pContext), 0, false);
}

// native bool:SourceTV_GetDemoFileName(String:sFilename[], maxlen);
static cell_t Native_GetDemoFileName(IPluginContext *pContext, const cell_t *params)
{
	char *destination;
	if (!pawn::ResolveBuffer(*pContext, params[1], params[2], destination))
		return 0;

	if (hltvserver == nullptr)
		return 0;

	// Failed to find the IDemoRecorder instance
	if (!hltvserver->GetDemoRecorder())
		return 0;

	// Not recording
	if (!hltvserver->GetDemoRecorder()->IsRecording())
		return 0;

	const char *pDemoFile = hltvserver->GetDemoFileName();
	if (!pDemoFile)
		return 0;

	return pawn::WriteString(*pContext, params[1], params[2], destination, pDemoFile);
}

// native SourceTV_GetRecordingTick();
static cell_t Native_GetRecordingTick(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	// Failed to find the IDemoRecorder instance
	if (!hltvserver->GetDemoRecorder())
		return -1;

	if (!hltvserver->GetDemoRecorder()->IsRecording())
		return -1;

	return hltvserver->GetDemoRecorder()->GetRecordingTick();
}

// native bool:SourceTV_PrintToDemoConsole(const String:format[], any:...);
static bool FormatPrint(IPluginContext *context, const cell_t *params, unsigned int index,
	char (&buffer)[1024])
{
	DetectExceptions handler(context);
	smutils->FormatString(buffer, sizeof(buffer), context, params, index);
	return !handler.HasException();
}

static cell_t Native_PrintToDemoConsole(IPluginContext *pContext, const cell_t *params)
{
	const auto server = g_HLTVServers.Pin(hltvserver);
	if (!server || !HasFormatString(*pContext, params, 1))
		return 0;

	const int slot = server->GetHLTVServer()->GetHLTVSlot();
	char buffer[1024];
	return sourcetv::PrintToRecipient([&] {
		return server->IsActive() ? sourcetv::ClientAt(iserver, slot) : nullptr;
	}, [&] {
		return FormatPrint(pContext, params, 1, buffer);
	}, [&](IClient *client) {
		client->ClientPrintf("%s\n", buffer);
	});
}

// native SourceTV_GetSpectatorCount();
static cell_t Native_GetSpectatorCount(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetBaseServer()->GetNumClients();
}

// native SourceTV_GetMaxClients();
static cell_t Native_GetMaxClients(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetBaseServer()->GetMaxClients();
}

// native SourceTV_GetClientCount();
static cell_t Native_GetClientCount(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetBaseServer()->GetClientCount();
}

// native bool:SourceTV_IsClientConnected(client);
static cell_t Native_IsClientConnected(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	return pClient && pClient->IsConnected();
}

// native bool:SourceTV_IsClientProxy(client);
static cell_t Native_IsClientProxy(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	return pClient->BaseClient()->IsHLTV();
}

// native SourceTV_GetClientName(client, String:name[], maxlen);
static cell_t Native_GetClientName(IPluginContext *pContext, const cell_t *params)
{
	char *destination;
	if (!pawn::ResolveBuffer(*pContext, params[2], params[3], destination))
		return 0;

	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}
	
	pawn::WriteString(*pContext, params[2], params[3], destination, pClient->Name());
	return 0;
}

// native SourceTV_GetClientIP(client, String:ip[], maxlen);
static cell_t Native_GetClientIP(IPluginContext *pContext, const cell_t *params)
{
	char *destination;
	if (!pawn::ResolveBuffer(*pContext, params[2], params[3], destination))
		return 0;

	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	pawn::WriteString(*pContext, params[2], params[3], destination, pClient->Ip());
	return 0;
}

// native SourceTV_GetClientPassword(client, String:password[], maxlen);
static cell_t Native_GetClientPassword(IPluginContext *pContext, const cell_t *params)
{
	char *destination;
	if (!pawn::ResolveBuffer(*pContext, params[2], params[3], destination))
		return 0;

	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	pawn::WriteString(*pContext, params[2], params[3], destination, pClient->Password());
	return 0;
}

// native SourceTV_KickClient(client, const String:sReason[]);
static cell_t Native_KickClient(IPluginContext *pContext, const cell_t *params)
{
	const char *pReason;
	if (!pawn::ReadString(*pContext, params[2], pReason))
		return 0;

	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	pClient->Kick(pReason);
	return 0;
}

// native SourceTV_PrintToChat(client, const String:format[], any:...);
template <class Emit>
static void PrintToSpectator(IPluginContext *context, const cell_t *params, Emit emit)
{
	const auto server = g_HLTVServers.Pin(hltvserver);
	if (!server || !HasFormatString(*context, params, 2))
		return;

	const cell_t client = params[1];
	if (client < 1 || client > server->GetBaseServer()->GetClientCount()) {
		context->ReportError("Invalid spectator client index %d.", client);
		return;
	}
	auto* spectator = server->FindClient(client);
	if (!spectator || !spectator->IsConnected()) {
		context->ReportError("Client %d is not connected.", client);
		return;
	}

	char buffer[1024];
	sourcetv::PrintToRecipient([&] {
		if (!server->IsActive() || client > server->GetBaseServer()->GetClientCount())
			return static_cast<IClient*>(nullptr);
		auto* current = server->FindClient(client);
		return current ? current->BaseClient() : nullptr;
	}, [&] {
		auto format = [&] { return FormatPrint(context, params, 2, buffer); };
#if SOURCE_ENGINE != SE_CSGO
		const int slot = server->GetHLTVServer()->GetHLTVSlot();
		return sourcetv::FormatForRelay([&]() -> IGamePlayer* {
			if (!sourcetv::ClientAt(iserver, slot))
				return nullptr;
			auto* relay = playerhelpers->GetGamePlayer(slot + 1);
			return relay && relay->IsConnected() ? relay : nullptr;
		}, *translator, slot + 1, spectator->GetLanguageId(), format);
#else
		return format();
#endif
	}, [&](IClient *recipient) { emit(recipient, buffer); });
}

static cell_t Native_PrintToChat(IPluginContext *pContext, const cell_t *params)
{
	PrintToSpectator(pContext, params, [](IClient *client, const char *buffer) {
		IGameEvent *msg = gameevents->CreateEvent("hltv_chat", true);
		if (!msg)
			return;

#if SOURCE_ENGINE == SE_CSGO
		wchar_t wBuffer[1024];
		V_UTF8ToUnicode(buffer, wBuffer, sizeof(wBuffer));
		msg->SetWString("text", wBuffer);
#else
		msg->SetString("text", buffer);
#endif
		if (g_bHasClientFireGameEventOffset) {
			// The IClient vtable is +4 from the CBaseClient vtable.
			void *gameClient = (void *)((intptr_t)client - 4);
			SH_MCALL(gameClient, CBaseClient_FireGameEvent)(msg);
		}
		gameevents->FreeEvent(msg);
	});
	return 0;
}

// native SourceTV_PrintToConsole(client, const String:format[], any:...);
static cell_t Native_PrintToConsole(IPluginContext *pContext, const cell_t *params)
{
	PrintToSpectator(pContext, params, [](IClient *client, const char *buffer) {
		client->ClientPrintf("%s\n", buffer);
	});
	return 0;
}


// native SourceTV_SetClientTVTitle(client, const String:format[], any:...);
static cell_t Native_SetClientTVTitle(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;
	if (!HasFormatString(*pContext, params, 2))
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = hltvserver->FindClient(client);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 2);
		if (eh.HasException())
			return 0;
	}

	IGameEvent *msg = gameevents->CreateEvent("hltv_title", true);
	if (!msg)
		return 0;

#if SOURCE_ENGINE == SE_CSGO
	wchar_t wBuffer[1024];
	V_UTF8ToUnicode(buffer, wBuffer, sizeof(wBuffer));
	msg->SetWString("text", wBuffer);
#else
	msg->SetString("text", buffer);
#endif

	if (g_bHasClientFireGameEventOffset)
	{
		void *pGameClient = pClient->BaseClient();
		// The IClient vtable is +4 from the CBaseClient vtable due to multiple inheritance.
		pGameClient = (void *)((intptr_t)pGameClient - 4);
		SH_MCALL(pGameClient, CBaseClient_FireGameEvent)(msg);
	}

	gameevents->FreeEvent(msg);

	return 0;
}

const sp_nativeinfo_t sourcetv_natives[] =
{
	{ "SourceTV_GetServerInstanceCount", Native_GetServerInstanceCount },
	{ "SourceTV_SelectServerInstance", Native_SelectServerInstance },
	{ "SourceTV_GetSelectedServerInstance", Native_GetSelectedServerInstance },
	{ "SourceTV_IsActive", Native_IsActive },
	{ "SourceTV_IsMasterProxy", Native_IsMasterProxy },
	{ "SourceTV_GetServerIP", Native_GetServerIP },
	{ "SourceTV_GetServerPort", Native_GetServerPort },
	{ "SourceTV_GetBotIndex", Native_GetBotIndex },
	{ "SourceTV_GetLocalStats", Native_GetLocalStats },
	{ "SourceTV_GetGlobalStats", Native_GetGlobalStats },
	{ "SourceTV_GetBroadcastTick", Native_GetBroadcastTick },
	{ "SourceTV_GetDelay", Native_GetDelay },
	{ "SourceTV_BroadcastScreenMessage", Native_BroadcastScreenMessage },
	{ "SourceTV_BroadcastConsoleMessage", Native_BroadcastConsoleMessage },
	{ "SourceTV_BroadcastChatMessage", Native_BroadcastChatMessage },
	{ "SourceTV_GetViewEntity", Native_GetViewEntity },
	{ "SourceTV_GetViewOrigin", Native_GetViewOrigin },
	{ "SourceTV_ForceFixedCameraShot", Native_ForceFixedCameraShot },
	{ "SourceTV_ForceChaseCameraShot", Native_ForceChaseCameraShot },
	{ "SourceTV_StartRecording", Native_StartRecording },
	{ "SourceTV_StartRecordingEx", Native_StartRecordingEx },
	{ "SourceTV_StopOwnedRecording", Native_StopOwnedRecording },
	{ "SourceTV_GetRecordingSnapshot", Native_GetRecordingSnapshot },
	{ "SourceTV_StopRecording", Native_StopRecording },
	{ "SourceTV_IsRecording", Native_IsRecording },
	{ "SourceTV_GetDemoFileName", Native_GetDemoFileName },
	{ "SourceTV_GetRecordingTick", Native_GetRecordingTick },
	{ "SourceTV_PrintToDemoConsole", Native_PrintToDemoConsole },
	{ "SourceTV_GetSpectatorCount", Native_GetSpectatorCount },
	{ "SourceTV_GetMaxClients", Native_GetMaxClients },
	{ "SourceTV_GetClientCount", Native_GetClientCount },
	{ "SourceTV_IsClientConnected", Native_IsClientConnected },
	{ "SourceTV_IsClientProxy", Native_IsClientProxy },
	{ "SourceTV_GetClientName", Native_GetClientName },
	{ "SourceTV_GetClientIP", Native_GetClientIP },
	{ "SourceTV_GetClientPassword", Native_GetClientPassword },
	{ "SourceTV_KickClient", Native_KickClient },
	{ "SourceTV_PrintToChat", Native_PrintToChat },
	{ "SourceTV_PrintToConsole", Native_PrintToConsole },
	{ "SourceTV_SetClientTVTitle", Native_SetClientTVTitle },
	{ NULL, NULL },
};
