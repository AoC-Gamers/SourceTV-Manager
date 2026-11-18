// SPDX-License-Identifier: GPL-3.0-only
#ifndef SOURCETV_PRINT_RECIPIENT_H
#define SOURCETV_PRINT_RECIPIENT_H

namespace sourcetv {

template <class Server>
auto ClientAt(Server* server, int slot) -> decltype(server->GetClient(slot))
{
	if (!server || slot < 0 || slot >= server->GetClientCount())
		return nullptr;
	return server->GetClient(slot);
}

template <class FindRelay, class Translator, class Format>
bool FormatForRelay(FindRelay findRelay, Translator& translator, int target,
	unsigned int language, Format format)
{
	auto* relay = findRelay();
	if (!relay)
		return false;

	struct TranslationScope {
		FindRelay& findRelay;
		Translator& translator;
		decltype(relay) relayPlayer;
		unsigned int serial;
		unsigned int language;
		int target;

		~TranslationScope()
		{
			auto* current = findRelay();
			if (current == relayPlayer && current->GetSerial() == serial)
				current->SetLanguageId(language);
			translator.SetGlobalTarget(target);
		}
	} scope{findRelay, translator, relay, relay->GetSerial(), relay->GetLanguageId(),
		translator.SetGlobalTarget(target)};
	relay->SetLanguageId(language);
	return format();
}

template <class FindClient, class Format, class Emit>
bool PrintToRecipient(FindClient findClient, Format format, Emit emit)
{
	auto* initial = findClient();
	if (!initial || !initial->IsConnected())
		return false;
	const int user = initial->GetUserID();
	if (!format())
		return false;

	auto* current = findClient();
	if (current != initial || !current->IsConnected() || current->GetUserID() != user)
		return false;
	emit(current);
	return true;
}

} // namespace sourcetv
#endif
