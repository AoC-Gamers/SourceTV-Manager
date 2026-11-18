// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <optional>

namespace sourcetv::abi {

class HltvServerBase
{
public:
	static std::optional<HltvServerBase> FromOffset(int bytes)
	{
		if (bytes < 0)
			return std::nullopt;
		return HltvServerBase(bytes);
	}

	void* CompleteServer(void* serverBase) const
	{
		return serverBase ? static_cast<std::byte*>(serverBase) - m_Bytes : nullptr;
	}

	void* ServerBase(void* completeServer) const
	{
		return completeServer ? static_cast<std::byte*>(completeServer) + m_Bytes : nullptr;
	}

private:
	explicit HltvServerBase(int bytes) : m_Bytes(bytes) {}
	int m_Bytes;
};

inline auto LocalEventArguments(void* receiver, void* event, bool reliable)
{
	// BinTools expects a packed argument buffer, beginning with the receiver.
	std::array<unsigned char, 2 * sizeof(void*) + sizeof(bool)> arguments{};
	std::memcpy(arguments.data(), &receiver, sizeof(receiver));
	std::memcpy(arguments.data() + sizeof(receiver), &event, sizeof(event));
	std::memcpy(arguments.data() + sizeof(receiver) + sizeof(event), &reliable, sizeof(reliable));
	return arguments;
}

} // namespace sourcetv::abi
