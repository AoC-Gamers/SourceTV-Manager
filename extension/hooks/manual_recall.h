// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <sourcehook.h>

// Continue the current pre-hook chain with updated arguments. This is the
// call portion of RETURN_META_VALUE_MNEWPARAMS, so the caller can consume the
// result before it returns MRES_SUPERCEDE to the outer invocation.
#define SOURCETV_RECALL_MANUAL(hook, ...) ([&] { \
	SET_META_RESULT(MRES_IGNORED); \
	SH_GLOB_SHPTR->DoRecall(); \
	auto* receiver = static_cast<SourceHook::EmptyClass*>(SH_GLOB_SHPTR->GetIfacePtr()); \
	return (receiver->*(__SoureceHook_FHM_GetRecallMFP##hook(receiver)))(__VA_ARGS__); \
}())
