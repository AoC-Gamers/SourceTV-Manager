// SPDX-License-Identifier: GPL-3.0-only
#include "hook_context.h"

#include <cassert>
#include <cstdio>
#include <cstring>

int main()
{
	sourcetv::ConnectionBuffers outer("outer");
	assert(outer.rejection[0] == '\0');
	{
		sourcetv::ConnectionBuffers inner(nullptr);
		assert(inner.password[0] == '\0' && inner.rejection[0] == '\0');
		inner.password.fill('x');
		inner.rejection.fill('%');
		inner.Terminate();
		assert(std::strlen(inner.password.data()) == 254);
		assert(std::strlen(inner.rejection.data()) == 254);
		assert(inner.L4D2RejectionText().data() == std::string(508, '%'));
	}
	assert(std::strcmp(outer.password.data(), "outer") == 0);
	assert(outer.rejection[0] == '\0');

	assert(outer.L4D2RejectionText()[0] == '\0');
	const char reason[] = "SourceTV: prueba al 100% completada.";
	std::memcpy(outer.rejection.data(), reason, sizeof(reason));
	const auto encoded = outer.L4D2RejectionText();
	assert(std::strcmp(encoded.data(), "SourceTV: prueba al 100%% completada.") == 0);
	assert(std::strcmp(outer.rejection.data(), reason) == 0);

	int firstServer, secondServer, author, guest;
	sourcetv::SpectatorCommands commands;
	commands.Begin(&firstServer, &author, sourcetv::ChatMessage{1, "Author", "outer"});
	const auto captured = commands.Current(&firstServer);
	assert(captured && captured->text == "outer");
	commands.Begin(&firstServer, &author, std::nullopt);
	assert(!commands.Current(&firstServer));
	commands.Begin(&secondServer, &guest, sourcetv::ChatMessage{2, "Guest", "inner"});
	assert(commands.Current(&secondServer)->index == 2);
	commands.End(&author);
	assert(commands.Current(&firstServer)->name == "Author");
	commands.Forget(&guest);
	assert(!commands.Current(&secondServer));
	commands.End(&author);
	assert(!commands.Current(&firstServer) && captured->name == "Author");

	int original, first, second, invocation;
	void* slot = &original;
	sourcetv::PointerOverrides overrides;
	overrides.Push(&firstServer, &slot, &first);
	overrides.Push(&firstServer, nullptr, nullptr);
	overrides.Pop(&firstServer);
	assert(slot == &first);
	overrides.Push(&secondServer, &slot, &second, &invocation);
	overrides.Clear(&firstServer);
	assert(slot == &second);
	overrides.Pop(&secondServer, &invocation);
	assert(slot == &original);
	overrides.Push(&firstServer, &slot, &first);
	overrides.Push(&firstServer, &slot, &second, &invocation);
	overrides.Pop(&firstServer, &invocation);
	assert(slot == &first);
	overrides.Clear(&firstServer);
	overrides.Clear(&firstServer);
	assert(slot == &original);

	std::puts("hook_context: passed");
}
