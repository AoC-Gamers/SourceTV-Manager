// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sourcetv {

struct ConnectionBuffers
{
	std::array<char, 255> password{};
	std::array<char, 255> rejection{};

	explicit ConnectionBuffers(const char* original)
	{
		if (original) {
			for (std::size_t i = 0; i + 1 < password.size() && original[i]; ++i)
				password[i] = original[i];
		}
	}

	void Terminate()
	{
		password.back() = '\0';
		rejection.back() = '\0';
	}
};

struct ChatMessage
{
	int index;
	std::string name;
	std::string text;
};

class SpectatorCommands
{
public:
	void Begin(const void* server, const void* client, std::optional<ChatMessage> chat)
	{
		m_Commands.push_back({server, client, std::move(chat)});
	}

	void End(const void* client)
	{
		for (auto it = m_Commands.end(); it != m_Commands.begin();) {
			if ((--it)->client == client) {
				m_Commands.erase(it);
				return;
			}
		}
	}

	void Forget(const void* client)
	{
		m_Commands.erase(std::remove_if(m_Commands.begin(), m_Commands.end(),
			[&](const Command& command) { return command.client == client; }), m_Commands.end());
	}

	std::optional<ChatMessage> Current(const void* server) const
	{
		for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it) {
			if (it->server == server)
				return it->chat;
		}
		return std::nullopt;
	}

private:
	struct Command
	{
		const void* server;
		const void* client;
		std::optional<ChatMessage> chat;
	};

	std::vector<Command> m_Commands;
};

class PointerOverrides
{
public:
	void Push(const void* owner, void** slot, void* value, const void* invocation = nullptr)
	{
		m_Frames.push_back({owner, invocation, slot, slot ? *slot : nullptr});
		if (slot)
			*slot = value;
	}

	void Pop(const void* owner, const void* invocation = nullptr)
	{
		for (std::size_t i = m_Frames.size(); i > 0; --i) {
			const auto& frame = m_Frames[i - 1];
			if (frame.owner == owner && frame.invocation == invocation) {
				Remove(i - 1);
				return;
			}
		}
	}

	void Clear(const void* owner)
	{
		for (std::size_t i = m_Frames.size(); i > 0; --i) {
			if (m_Frames[i - 1].owner == owner)
				Remove(i - 1);
		}
	}

private:
	struct Frame
	{
		const void* owner;
		const void* invocation;
		void** slot;
		void* previous;
	};

	void Remove(std::size_t index)
	{
		const auto frame = m_Frames[index];
		if (frame.slot) {
			// If an inner command still owns this slot, pass the original value
			// to it and leave its active override in place.
			auto inner = std::find_if(m_Frames.begin() + index + 1, m_Frames.end(),
				[&](const Frame& next) { return next.slot == frame.slot; });
			if (inner != m_Frames.end())
				inner->previous = frame.previous;
			else
				*frame.slot = frame.previous;
		}
		m_Frames.erase(m_Frames.begin() + index);
	}

	std::vector<Frame> m_Frames;
};

} // namespace sourcetv
