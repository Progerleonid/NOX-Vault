#!/usr/bin/env python3
"""Fail a release when its tag and the two client version sources disagree."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_release_version.py TAG REPOSITORY_ROOT", file=sys.stderr)
        return 2
    tag = sys.argv[1]
    root = Path(sys.argv[2])
    match = re.fullmatch(r"v(\d+\.\d+\.\d+)", tag)
    if not match:
        print(f"release tag must have the form vX.Y.Z, got {tag!r}", file=sys.stderr)
        return 1
    expected = match.group(1)
    cmake = (root / "client" / "CMakeLists.txt").read_text(encoding="utf-8")
    cmake_match = re.search(r"project\(nox_vault_client VERSION (\d+\.\d+\.\d+)", cmake)
    manifest = json.loads((root / "client" / "vcpkg.json").read_text(encoding="utf-8"))
    versions = {"tag": expected, "CMake": cmake_match.group(1) if cmake_match else None,
                "vcpkg": manifest.get("version-string")}
    if len(set(versions.values())) != 1:
        print("release versions disagree: " + ", ".join(f"{k}={v}" for k, v in versions.items()), file=sys.stderr)
        return 1
    print(expected)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
