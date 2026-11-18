// SPDX-License-Identifier: GPL-3.0-only
#ifndef SOURCETV_RECORDING_HOOKS_H
#define SOURCETV_RECORDING_HOOKS_H

#include "recording_sessions.h"

#include <array>
#include <cstddef>

namespace sourcetv::recording {

template <typename Handle, std::size_t Count, typename Remove>
void RemoveHooks(std::array<Handle, Count>& hooks, Remove remove)
{
	for (auto it = hooks.rbegin(); it != hooks.rend(); ++it) {
		if (*it)
			remove(*it);
		*it = {};
	}
}

// Commit the observation capability only after every required hook is live.
template <typename Handle, std::size_t Count, typename Add, typename Remove>
bool InstallHooks(std::array<Handle, Count>& hooks, Add add, Remove remove)
{
	for (std::size_t i = 0; i < Count; ++i) {
		hooks[i] = add(i);
		if (!hooks[i]) {
			RemoveHooks(hooks, remove);
			return false;
		}
	}
	return true;
}

inline bool RecorderAvailable(const Sessions& sessions, const void* recorder)
{
	return sessions.Current(recorder).lifetime != 0;
}

template <typename Install>
bool RegisterRecorder(Sessions& sessions, const void* recorder, Install install)
{
	if (RecorderAvailable(sessions, recorder))
		return true;
	if (!recorder || !install())
		return false;
	sessions.Attach(recorder);
	return true;
}

template <typename Detour>
Detour* EnableRecordingHook(Detour* hook)
{
	if (!hook)
		return nullptr;
	hook->EnableDetour();
	if (hook->IsEnabled())
		return hook;
	hook->Destroy();
	return nullptr;
}

} // namespace sourcetv::recording
#endif
