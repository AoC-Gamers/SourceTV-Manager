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
#include "core/hltvclientwrapper.h"
#include "core/hltvserverwrapper.h"
#include "core/local_hltv_broadcast.h"
#include "natives/handlers.h"
#include "natives/natives.h"
#include "natives/pawn_boundary.h"
#include "natives/print_recipient.h"

namespace pawn = sourcetv::pawn;

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

void ShutdownNativeCalls()
{
	g_bHasBroadcastPrintfOffset = false;
	g_bHasClientFireGameEventOffset = false;
	ShutdownLocalHltvBroadcast();
}

// native bool:SourceTV_BroadcastScreenMessage(bool:bLocalOnly, const String:format[], any:...);
cell_t Native_BroadcastScreenMessage(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_BroadcastConsoleMessage(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_BroadcastChatMessage(IPluginContext *pContext, const cell_t *params)
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

// native bool:SourceTV_PrintToDemoConsole(const String:format[], any:...);
static bool FormatPrint(IPluginContext *context, const cell_t *params, unsigned int index,
	char (&buffer)[1024])
{
	DetectExceptions handler(context);
	smutils->FormatString(buffer, sizeof(buffer), context, params, index);
	return !handler.HasException();
}

cell_t Native_PrintToDemoConsole(IPluginContext *pContext, const cell_t *params)
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

cell_t Native_PrintToChat(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_PrintToConsole(IPluginContext *pContext, const cell_t *params)
{
	PrintToSpectator(pContext, params, [](IClient *client, const char *buffer) {
		client->ClientPrintf("%s\n", buffer);
	});
	return 0;
}


// native SourceTV_SetClientTVTitle(client, const String:format[], any:...);
cell_t Native_SetClientTVTitle(IPluginContext *pContext, const cell_t *params)
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
