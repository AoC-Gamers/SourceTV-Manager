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
#include "core/recording_hooks.h"
#include "hooks/forwards.h"

// Only windows always uses the vtable for these. Linux does direct calls, so we use detours there.
#ifdef WIN32
SH_DECL_HOOK2_void(IDemoRecorder, StartRecording, SH_NOATTRIB, 0, const char *, bool)
#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0, CGameInfo const *)
#else
SH_DECL_HOOK0_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0)
#endif // SOURCE_ENGINE == SE_CSGO
#endif // !WIN32

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

void CForwardManager::ShutdownRecordingHooks()
{
#ifndef WIN32
	sourcetv::recording::RemoveHooks(m_RecordingDetours, [](CDetour *hook) { hook->Destroy(); });
#else
	for (auto& entry : m_RecorderHooks)
		sourcetv::recording::RemoveHooks(entry.second, [](int id) { SH_REMOVE_HOOK_ID(id); });
	m_RecorderHooks.clear();
#endif
	m_RecordingSessions.Reset();
}

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
