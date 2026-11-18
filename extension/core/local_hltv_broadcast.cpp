// SPDX-License-Identifier: GPL-3.0-only
#include "extension.h"
#include "core/local_hltv_broadcast.h"

namespace {

std::optional<sourcetv::abi::HltvServerBase> serverBase;
ICallWrapper* localBroadcast = nullptr;

} // namespace

const std::optional<sourcetv::abi::HltvServerBase>& HltvServerBaseBinding()
{
	return serverBase;
}

void SetupLocalHltvBroadcast()
{
	int offset;
	if (g_pGameConf->GetOffset("IServer_from_CHLTVServer", &offset))
		serverBase = sourcetv::abi::HltvServerBase::FromOffset(offset);
#if SOURCE_ENGINE != SE_LEFT4DEAD2
	// The legacy engines are outside the L4D2 ABI profile.
	if (!serverBase)
		serverBase = sourcetv::abi::HltvServerBase::FromOffset(8);
#endif
	if (!serverBase)
		smutils->LogError(myself, "Missing or invalid IServer_from_CHLTVServer; local events and chat hooks are unavailable.");
}

void ShutdownLocalHltvBroadcast()
{
	if (localBroadcast) {
		localBroadcast->Destroy();
		localBroadcast = nullptr;
	}
	serverBase.reset();
}

bool BroadcastEventLocal(IHLTVServer* server, IGameEvent* event, bool reliable)
{
	if (!server || !event || !bintools)
		return false;

#if SOURCE_ENGINE == SE_LEFT4DEAD2
	const auto& eventBase = serverBase;
#else
	// Legacy event calls used an independent adjustment from the chat detour.
	const auto eventBase = sourcetv::abi::HltvServerBase::FromOffset(8);
#endif
	if (!eventBase)
		return false;

	void* receiver = eventBase->CompleteServer(server->GetBaseServer());
	if (!receiver)
		return false;

	if (!localBroadcast) {
		void* address = nullptr;
		if (!g_pGameConf->GetMemSig("CHLTVServer::BroadcastEventLocal", &address) || !address) {
			smutils->LogError(myself, "Failed to resolve CHLTVServer::BroadcastEventLocal.");
			return false;
		}

		PassInfo pass[2]{};
		pass[0].flags = PASSFLAG_BYVAL;
		pass[0].type = PassType_Basic;
		pass[0].size = sizeof(IGameEvent*);
		pass[1].flags = PASSFLAG_BYVAL;
		pass[1].type = PassType_Basic;
		pass[1].size = sizeof(bool);
		localBroadcast = bintools->CreateCall(address, CallConv_ThisCall, nullptr, pass, 2);
		if (!localBroadcast)
			return false;
	}

	auto arguments = sourcetv::abi::LocalEventArguments(receiver, event, reliable);
	localBroadcast->Execute(arguments.data(), nullptr);
	return true;
}
