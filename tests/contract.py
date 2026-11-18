# SPDX-License-Identifier: GPL-3.0-only
"""Check the SourcePawn API and its registration against the frozen baseline."""

import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = Path(__file__).with_name("fixtures") / "public-contract.json"


def inventory(read):
    include = read("addons/sourcemod/scripting/include/sourcetvmanager.inc")
    declarations = re.sub(r"/\*.*?\*/|//[^\n]*", "", include, flags=re.S)
    entry = read("extension/extension.cpp")
    natives = read("extension/natives/natives.cpp")
    forwards = read("extension/hooks/forwards.cpp")
    return {
        "declarations": [" ".join(match.split()) for match in
                         re.findall(r"^\s*((?:native|forward)\b[^;]+;)", declarations, re.M)],
        "enums": {name: [" ".join(member.split()) for member in body.split(",") if member.strip()]
                  for name, body in re.findall(r"\benum\s+(\w+)\s*\{([^}]+)\}", declarations)},
        "shot_length": re.findall(r"^#define\s+DEF_SHOT_LENGTH\s+([^\n]+)", declarations, re.M),
        "extension_file": re.findall(r'\bfile\s*=\s*"([^"]+)"', declarations),
        "optional_natives": re.findall(r'MarkNativeAsOptional\("([^"]+)"\)', declarations),
        "registered_natives": re.findall(r'\{\s*"(SourceTV_\w+)"\s*,\s*\w+\s*\}', natives),
        "registered_forwards": [" ".join(match.split()) for match in
                               re.findall(r'CreateForward\(("SourceTV_[^;]+)\);', forwards)],
        "libraries": re.findall(r'RegisterLibrary\([^,]+,\s*"([^"]+)"', entry),
        "cvars": [list(declaration) for declaration in
                  re.findall(r'\bConVar\s+\w+\("([^"]+)",\s*"([^"]+)"', entry)],
    }


class PublicContractTest(unittest.TestCase):
    def test_frozen_public_api(self):
        expected = json.loads(FIXTURE.read_text(encoding="utf-8"))["contract"]
        self.assertEqual(inventory(lambda name: (ROOT / name).read_text(encoding="utf-8")), expected)


if __name__ == "__main__":
    unittest.main()
