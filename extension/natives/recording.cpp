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
#include "core/hltvserverwrapper.h"
#include "hooks/forwards.h"
#include "natives/handlers.h"
#include "natives/pawn_boundary.h"
#include "natives/recording_path.h"

namespace pawn = sourcetv::pawn;
namespace recording = sourcetv::recording;

// native bool:SourceTV_IsRecording();
cell_t Native_IsRecording(IPluginContext *pContext, const cell_t *params)
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

cell_t Native_StartRecording(IPluginContext *pContext, const cell_t *params)
{
	const char *filename;
	if (!pawn::ReadString(*pContext, params[1], filename))
		return 0;

	auto port = SelectedRecorder();
	char path[PLATFORM_MAX_PATH];
	return recording::Start(g_pSTVForwards.RecordingSessions(), port, port.recorder, {}, false,
		false, filename, path, sizeof(path)).result == recording::StartResult::Started;
}

cell_t Native_StartRecordingEx(IPluginContext *pContext, const cell_t *params)
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

cell_t Native_StopOwnedRecording(IPluginContext *pContext, const cell_t *params)
{
	auto port = SelectedRecorder();
	return recording::Stop(g_pSTVForwards.RecordingSessions(), port, port.recorder, Caller(pContext), params[1], true);
}

cell_t Native_GetRecordingSnapshot(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_StopRecording(IPluginContext *pContext, const cell_t *params)
{
	auto port = SelectedRecorder();
	return recording::Stop(g_pSTVForwards.RecordingSessions(), port, port.recorder, Caller(pContext), 0, false);
}

// native bool:SourceTV_GetDemoFileName(String:sFilename[], maxlen);
cell_t Native_GetDemoFileName(IPluginContext *pContext, const cell_t *params)
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
cell_t Native_GetRecordingTick(IPluginContext *pContext, const cell_t *params)
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
