// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <sp_vm_api.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace sourcetv::pawn {

enum class BroadcastTarget : cell_t { Everyone = 0, OnlyLocal = 1 };

inline bool ReadBroadcastTarget(cell_t input, BroadcastTarget& target)
{
	if (input != static_cast<cell_t>(BroadcastTarget::Everyone)
		&& input != static_cast<cell_t>(BroadcastTarget::OnlyLocal))
		return false;
	target = static_cast<BroadcastTarget>(input);
	return true;
}

inline float ReadFloat(cell_t cell)
{
	static_assert(sizeof(float) == sizeof(cell_t), "Pawn cells must fit a float");
	float value;
	std::memcpy(&value, &cell, sizeof(value));
	return value;
}

inline cell_t FloatCell(float value)
{
	cell_t cell;
	std::memcpy(&cell, &value, sizeof(cell));
	return cell;
}

inline bool FitsInt(float value)
{
	// Compare in double so INT_MAX is not rounded up to 2^31.
	return std::isfinite(value) && double(value) >= (std::numeric_limits<int>::min)()
		&& double(value) <= (std::numeric_limits<int>::max)();
}

inline bool NextThinkTick(float duration, float interval, int tick, int& next)
{
	if (!std::isfinite(duration) || !std::isfinite(interval) || interval <= 0)
		return false;

	const float delta = 0.5f + duration / interval;
	if (!FitsInt(delta))
		return false;

	const int64_t target = int64_t(tick) + static_cast<int>(delta);
	if (target < (std::numeric_limits<int>::min)() || target > (std::numeric_limits<int>::max)())
		return false;

	next = static_cast<int>(target);
	return true;
}

inline bool ContainsAddressRange(cell_t start, size_t bytes)
{
	return start >= 0 && bytes && bytes - 1 <= size_t((std::numeric_limits<cell_t>::max)() - start);
}

template <class Context>
bool ResolveCells(Context& context, cell_t address, cell_t*& destination, size_t count = 1)
{
	if (address % sizeof(cell_t) || count > (std::numeric_limits<cell_t>::max)() / sizeof(cell_t)
		|| !ContainsAddressRange(address, count * sizeof(cell_t)))
		return false;

	cell_t* first = nullptr;
	if (context.LocalToPhysAddr(address, &first) != SP_ERROR_NONE || !first)
		return false;

	// LocalToPhysAddr validates a single byte address in the pinned VM.
	for (size_t offset = sizeof(cell_t) - 1; offset < count * sizeof(cell_t); offset += sizeof(cell_t)) {
		if (context.LocalToPhysAddr(address + static_cast<cell_t>(offset), nullptr) != SP_ERROR_NONE)
			return false;
	}
	destination = first;
	return true;
}

template <class Context>
bool ReadString(Context& context, cell_t address, const char*& value)
{
	if (address < 0)
		return false;
	char* first = nullptr;
	for (int64_t offset = address; offset <= (std::numeric_limits<cell_t>::max)(); ++offset) {
		char* byte = nullptr;
		if (context.LocalToString(static_cast<cell_t>(offset), &byte) != SP_ERROR_NONE || !byte)
			return false;
		if (!first)
			first = byte;
		if (!*byte) {
			value = first;
			return true;
		}
	}
	return false;
}

template <class Context>
bool ResolveBuffer(Context& context, cell_t address, cell_t capacity, char*& destination)
{
	if (capacity <= 0 || !ContainsAddressRange(address, static_cast<size_t>(capacity)))
		return false;
	char* first = nullptr;
	if (context.LocalToString(address, &first) != SP_ERROR_NONE || !first)
		return false;

	const cell_t last = address + (capacity - 1);
	// In the pinned VM the heap and stack boundaries are cell aligned.
	// Check each crossed cell and the final byte to reject heap/stack gaps.
	for (int64_t cursor = int64_t(address) + sizeof(cell_t) - address % sizeof(cell_t);
	     cursor < last; cursor += sizeof(cell_t)) {
		char* byte = nullptr;
		if (context.LocalToString(static_cast<cell_t>(cursor), &byte) != SP_ERROR_NONE || !byte)
			return false;
	}
	char* end = nullptr;
	if (context.LocalToString(last, &end) != SP_ERROR_NONE || !end)
		return false;

	destination = first;
	return true;
}

template <class Context>
bool WriteString(Context& context, cell_t address, cell_t capacity, char* destination, const char* value)
{
	// The pinned UTF-8 writer can inspect dest[-1] when copying zero bytes.
	if (capacity == 1 || !*value) {
		*destination = '\0';
		return true;
	}
	const size_t bytes = static_cast<size_t>(capacity);
	if (std::strlen(value) >= bytes) {
		// The VM scans backwards from a truncated continuation byte. A prefix
		// made only of continuation bytes has no leading byte inside the output.
		const char *tail = value + bytes - 2;
		while (tail != value && (static_cast<unsigned char>(*tail) & 0xc0) == 0x80)
			--tail;
		if (tail == value && (static_cast<unsigned char>(*tail) & 0xc0) == 0x80) {
			*destination = '\0';
			return true;
		}
	}
	return context.StringToLocalUTF8(address, bytes, value, nullptr) == SP_ERROR_NONE;
}

} // namespace sourcetv::pawn
