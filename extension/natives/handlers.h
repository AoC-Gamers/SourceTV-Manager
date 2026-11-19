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

#pragma once

#include "smsdk_ext.h"

cell_t Native_BroadcastScreenMessage(IPluginContext *pContext, const cell_t *params);
cell_t Native_BroadcastConsoleMessage(IPluginContext *pContext, const cell_t *params);
cell_t Native_BroadcastChatMessage(IPluginContext *pContext, const cell_t *params);
cell_t Native_PrintToDemoConsole(IPluginContext *pContext, const cell_t *params);
cell_t Native_PrintToChat(IPluginContext *pContext, const cell_t *params);
cell_t Native_PrintToConsole(IPluginContext *pContext, const cell_t *params);
cell_t Native_SetClientTVTitle(IPluginContext *pContext, const cell_t *params);

cell_t Native_GetViewEntity(IPluginContext *pContext, const cell_t *params);
cell_t Native_GetViewOrigin(IPluginContext *pContext, const cell_t *params);
cell_t Native_ForceFixedCameraShot(IPluginContext *pContext, const cell_t *params);
cell_t Native_ForceChaseCameraShot(IPluginContext *pContext, const cell_t *params);

cell_t Native_IsRecording(IPluginContext *pContext, const cell_t *params);
cell_t Native_StartRecording(IPluginContext *pContext, const cell_t *params);
cell_t Native_StartRecordingEx(IPluginContext *pContext, const cell_t *params);
cell_t Native_StopOwnedRecording(IPluginContext *pContext, const cell_t *params);
cell_t Native_GetRecordingSnapshot(IPluginContext *pContext, const cell_t *params);
cell_t Native_StopRecording(IPluginContext *pContext, const cell_t *params);
cell_t Native_GetDemoFileName(IPluginContext *pContext, const cell_t *params);
cell_t Native_GetRecordingTick(IPluginContext *pContext, const cell_t *params);
