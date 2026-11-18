// SPDX-License-Identifier: GPL-3.0-only
#include "recording_path.h"

#include <climits>
#include <cstdio>

int main()
{
	const struct {
		const char *description;
		const char *path;
		bool allowed;
	} cases[] = {
		{"ordinary demo", "demos/match.dem", true},
		{"suffix handled by caller", "demos/match", true},
		{"single backslash accepted by legacy predicate", R"(demos\match)", true},
		{"empty", "", false},
		{"double backslash", R"(demos\\match)", false},
		{"colon", "C:match", false},
		{"dot pair", "demos/../match", false},
		{"line feed", "demos/\nmatch", false},
		{"carriage return", "demos/\rmatch", false},
	};
	for (const auto &test : cases) {
		if (IsValidPath(test.path) != test.allowed) {
			std::fprintf(stderr, "recording_path: %s\n", test.description);
			return 1;
		}
	}
	std::printf("recording_path: %zu cases passed (%zu-bit)\n",
		sizeof(cases) / sizeof(cases[0]), sizeof(void *) * CHAR_BIT);
}
