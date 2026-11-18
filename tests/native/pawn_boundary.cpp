// SPDX-License-Identifier: GPL-3.0-only
#include "pawn_boundary.h"

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace pawn = sourcetv::pawn;

static void Check(bool condition, const char *description)
{
	if (!condition) {
		std::fprintf(stderr, "pawn_boundary: %s\n", description);
		std::exit(1);
	}
}

struct TestContext
{
	std::array<cell_t, 12> memory{};
	bool failWrite = false;
	unsigned writes = 0;

	bool IsAddressValid(cell_t address) const
	{
		return (address >= 0 && address < 20) || (address >= 32 && address < 48);
	}

	int LocalToPhysAddr(cell_t address, cell_t** output)
	{
		if (!IsAddressValid(address))
			return SP_ERROR_INVALID_ADDRESS;
		if (output)
			*output = reinterpret_cast<cell_t*>(reinterpret_cast<char*>(memory.data()) + address);
		return SP_ERROR_NONE;
	}

	int LocalToString(cell_t address, char** output)
	{
		if (!IsAddressValid(address))
			return SP_ERROR_INVALID_ADDRESS;
		*output = reinterpret_cast<char*>(memory.data()) + address;
		return SP_ERROR_NONE;
	}

	int StringToLocalUTF8(cell_t address, size_t capacity, const char *value, size_t*)
	{
		++writes;
		if (failWrite)
			return SP_ERROR_INVALID_ADDRESS;
		auto *output = reinterpret_cast<char*>(memory.data()) + address;
		std::snprintf(output, capacity, "%s", value);
		return SP_ERROR_NONE;
	}
};

int main()
{
	TestContext context;
	context.memory.fill(0x41414141);
	cell_t *cells = nullptr;
	Check(pawn::ResolveCells(context, 8, cells, 3) && cells == &context.memory[2], "valid vector");
	Check(!pawn::ResolveCells(context, 12, cells, 3) && cells == &context.memory[2], "vector crosses gap");
	Check(!pawn::ResolveCells(context, 40, cells, 3), "vector exceeds VM");
	Check(!pawn::ResolveCells(context, 1, cells), "unaligned cell");
	Check(!pawn::ResolveCells(context, -1, cells), "negative cell");
	Check(!pawn::ResolveCells(context, INT_MAX - 3, cells, 3), "cell range arithmetic overflow");

	const char *input = nullptr;
	Check(!pawn::ReadString(context, 0, input) && !input, "unterminated string reaches the gap");

	char *output = nullptr;
	Check(!pawn::ResolveBuffer(context, 16, 20, output) && !output, "buffer spans heap/stack gap");
	Check(!pawn::ResolveBuffer(context, 17, 4, output), "last byte outside heap");
	Check(!pawn::ResolveBuffer(context, 0, 0, output), "zero capacity");
	Check(!pawn::ResolveBuffer(context, 0, -1, output), "negative capacity");
	Check(!pawn::ResolveBuffer(context, INT_MAX, 2, output), "buffer range arithmetic overflow");
	Check(context.memory[4] == 0x41414141 && context.writes == 0, "validation leaves canaries intact");
	Check(pawn::ResolveBuffer(context, 17, 3, output), "unaligned buffer ending exactly at heap boundary");
	Check(pawn::WriteString(context, 17, 3, output, "") && context.writes == 0 && !*output,
		"empty output does not call the UTF-8 writer");
	Check(pawn::WriteString(context, 17, 1, output, "a") && context.writes == 0 && !*output,
		"one-byte output is always terminated");
	Check(pawn::WriteString(context, 17, 3, output, "abc") && !std::strcmp(output, "ab"),
		"nonempty output delegates with the requested capacity");
	context.failWrite = true;
	Check(!pawn::WriteString(context, 17, 3, output, "bad"), "VM writer error propagated");

	Check(pawn::ReadString(context, 17, input) && !std::strcmp(input, "ab"), "valid input string");

	pawn::BroadcastTarget target;
	Check(pawn::ReadBroadcastTarget(0, target) && target == pawn::BroadcastTarget::Everyone, "broadcast everyone");
	Check(pawn::ReadBroadcastTarget(1, target) && target == pawn::BroadcastTarget::OnlyLocal, "broadcast only local");
	Check(!pawn::ReadBroadcastTarget(-1, target) && target == pawn::BroadcastTarget::OnlyLocal, "invalid negative enum");
	Check(!pawn::ReadBroadcastTarget(2, target), "unknown broadcast enum");

	const cell_t negativeZero = INT_MIN;
	Check(pawn::FloatCell(pawn::ReadFloat(negativeZero)) == negativeZero, "negative-zero bits");
	Check(pawn::FloatCell(pawn::ReadFloat(0x7fc01234)) == 0x7fc01234, "NaN payload bits");
	Check(!pawn::FitsInt(pawn::ReadFloat(0x7f800000)) && !pawn::FitsInt(2147483648.0f),
		"invalid camera coordinates");
	int next = -1;
	Check(pawn::NextThinkTick(0.6f, 0.1f, 50, next) && next == 56, "camera duration uses legacy rounding");
	Check(!pawn::NextThinkTick(0.6f, 0.1f, INT_MAX, next) && next == 56, "tick addition overflow");
	Check(!pawn::NextThinkTick(pawn::ReadFloat(0x7fc01234), 0.1f, 50, next), "NaN duration");
	Check(!pawn::NextThinkTick(4.0f, 0.0f, 50, next), "invalid tick interval");
	std::printf("pawn_boundary: checks passed (%zu-bit)\n", sizeof(void*) * CHAR_BIT);
}
