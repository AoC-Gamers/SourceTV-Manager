// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "core/hltv_server_binding.h"

class IHLTVServer;
class IGameEvent;

const std::optional<sourcetv::abi::HltvServerBase>& HltvServerBaseBinding();
void SetupLocalHltvBroadcast();
void ShutdownLocalHltvBroadcast();
bool BroadcastEventLocal(IHLTVServer* server, IGameEvent* event, bool reliable);
