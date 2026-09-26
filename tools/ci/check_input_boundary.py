#!/usr/bin/env python3
"""HOUSE-02991: device polling stays in IInputSource implementations."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = {".cpp", ".hpp", ".h", ".inl"}
INPUT_SOURCES = {
    Path("src/player/KeyboardMouseSource.cpp"),
    Path("src/player/TouchSource.cpp"),
}
DEVICE_CALL = re.compile(
    r"\b(?:Keyboard|Mouse|TouchPanel)\s*::\s*"
    r"(?:GetState|SetPosition|ReadGesture|GetCapabilities)\s*\("
)
DEVICE_HEADER = re.compile(
    r"^\s*#\s*include\s*[<\"]Microsoft/Xna/Framework/Input/"
    r"(?:Keyboard|Mouse|Touch/TouchPanel)\.hpp[>\"]"
)


def violations(source: str) -> list[int]:
    return [
        line
        for line, text in enumerate(source.splitlines(), 1)
        if DEVICE_CALL.search(text) or DEVICE_HEADER.search(text)
    ]


def main() -> int:
    assert violations("auto keys = Keyboard::GetState();") == [1]
    assert violations("Mouse::SetPosition(1, 2);\nTouchPanel::GetState();") == [1, 2]
    assert violations('#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"') == [1]
    assert not violations("MouseState state; // no device poll")

    failures: list[str] = []
    checked = 0
    for directory in (ROOT / "src", ROOT / "include"):
        for path in directory.rglob("*"):
            if path.suffix not in SUFFIXES or not path.is_file():
                continue
            relative = path.relative_to(ROOT)
            if relative in INPUT_SOURCES:
                continue
            checked += 1
            for line in violations(path.read_text(encoding="utf-8")):
                failures.append(f"{relative}:{line}: direct XNA device call outside an input source")

    if failures:
        print("\n".join(failures))
        return 1
    print(f"check_input_boundary: {checked} runtime files clean; device reads confined to input sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
