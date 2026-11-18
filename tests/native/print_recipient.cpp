// SPDX-License-Identifier: GPL-3.0-only
#include "print_recipient.h"

#include <cassert>
#include <cstdio>

struct Player {
	unsigned int serial = 1;
	unsigned int language = 3;
	bool IsConnected() const { return true; }
	int GetUserID() const { return serial; }
	unsigned int GetSerial() const { return serial; }
	unsigned int GetLanguageId() const { return language; }
	void SetLanguageId(unsigned int value) { language = value; }
};

struct Translator {
	int target = 9;
	int SetGlobalTarget(int value)
	{
		const int previous = target;
		target = value;
		return previous;
	}
};

struct Server {
	Player player;
	int reads = 0;
	int GetClientCount() const { return 1; }
	Player* GetClient(int slot)
	{
		assert(slot == 0);
		++reads;
		return &player;
	}
};

int main()
{
	Server server;
	assert(!sourcetv::ClientAt(&server, -1));
	assert(!sourcetv::ClientAt(&server, 1));
	assert(server.reads == 0);
	assert(sourcetv::ClientAt(&server, 0) == &server.player);

	Player relay;
	Player* currentRelay = nullptr;
	auto findRelay = [&] { return currentRelay; };
	Translator translator;

	assert(!sourcetv::FormatForRelay(findRelay, translator, 1, 4, [] {
		assert(false && "format with no relay");
		return true;
	}));

	currentRelay = &relay;
	assert(sourcetv::FormatForRelay(findRelay, translator, 1, 4, [&] {
		assert(relay.language == 4 && translator.target == 1);
		assert(!sourcetv::FormatForRelay(findRelay, translator, 2, 5, [&] {
			assert(relay.language == 5 && translator.target == 2);
			return false;
		}));
		assert(relay.language == 4 && translator.target == 1);
		return true;
	}));
	assert(relay.language == 3 && translator.target == 9);

	assert(!sourcetv::FormatForRelay(findRelay, translator, 1, 4, [&] {
		++relay.serial;
		relay.language = 7;
		return false;
	}));
	assert(relay.language == 7 && translator.target == 9);

	auto findClient = [&] { return sourcetv::ClientAt(&server, 0); };
	int sends = 0;
	auto emit = [&](Player* recipient) {
		assert(recipient == &server.player);
		++sends;
	};
	assert(!sourcetv::PrintToRecipient(findClient, [] { return false; }, emit));
	assert(!sourcetv::PrintToRecipient(findClient, [&] {
		++server.player.serial;
		return true;
	}, emit));
	assert(sourcetv::PrintToRecipient(findClient, [] { return true; }, emit));
	assert(sends == 1);
	std::printf("print_recipient: checks passed (%zu-bit)\n", sizeof(void*) * 8);
}
