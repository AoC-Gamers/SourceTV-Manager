// SPDX-License-Identifier: GPL-3.0-only
// Extracted from SourceTV Manager (C) 2004-2016 AlliedModders LLC. See LICENSE and NOTICE.md.
#pragma once

#include <cstring>

// Checks in COM_IsValidPath in the engine
static inline bool IsValidPath(const char *path)
{
	return std::strlen(path) > 0 && !std::strstr(path, "\\\\") && !std::strstr(path, ":") && !std::strstr(path, "..") && !std::strstr(path, "\n") && !std::strstr(path, "\r");
}
