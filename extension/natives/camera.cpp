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
#include "core/hltvdirectorwrapper.h"
#include "core/hltvserverwrapper.h"
#include "natives/handlers.h"
#include "natives/pawn_boundary.h"

namespace pawn = sourcetv::pawn;

// native SourceTV_GetViewEntity();
cell_t Native_GetViewEntity(IPluginContext *pContext, const cell_t *params)
{
	return hltvdirector->GetPVSEntity();
}

// native SourceTV_GetViewOrigin();
cell_t Native_GetViewOrigin(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_ForceFixedCameraShot(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_ForceChaseCameraShot(IPluginContext *pContext, const cell_t *params)
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
