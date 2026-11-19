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

#include "extension.h"
#include "natives/handlers.h"
#include "natives/pawn_boundary.h"
#include "core/hltvclientwrapper.h"
#include "core/hltvserverwrapper.h"

namespace pawn = sourcetv::pawn;

extern const sp_nativeinfo_t sourcetv_natives[];

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
