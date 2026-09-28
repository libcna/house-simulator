#!/usr/bin/env python3
"""Focused fail-closed regressions for the strict-XNA compiler gate."""

from __future__ import annotations

import contextlib
import io
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_xna_strict as gate


class StrictCompilerGateTests(unittest.TestCase):
    entry = {"file": "src/fixture.cpp", "command": "c++ -c src/fixture.cpp -o fixture.o"}

    def invoke(self, returncode: int, stderr: str = "") -> tuple[int, str, str]:
        output, errors = io.StringIO(), io.StringIO()
        result = subprocess.CompletedProcess([], returncode, stdout="", stderr=stderr)
        with (
            mock.patch.object(sys, "argv", ["check_xna_strict.py", "--jobs", "1"]),
            mock.patch.object(gate, "load_entries", return_value=[self.entry]),
            mock.patch.object(gate.subprocess, "run", return_value=result),
            contextlib.redirect_stdout(output),
            contextlib.redirect_stderr(errors),
        ):
            status = gate.main()
        return status, output.getvalue(), errors.getvalue()

    def test_clean_compile_passes(self) -> None:
        status, output, errors = self.invoke(0)
        self.assertEqual(status, 0)
        self.assertIn("1 translation unit(s) clean", output)
        self.assertEqual(errors, "")

    def test_syntax_failure_without_deprecation_matches_fails(self) -> None:
        status, output, errors = self.invoke(1, "upstream.hpp:313: error: expected unqualified-id before using")
        self.assertEqual(status, 1)
        self.assertNotIn("clean", output)
        self.assertIn("src/fixture.cpp: compiler exit 1", errors)
        self.assertIn("upstream.hpp:313: error:", errors)

    def test_failure_with_empty_stderr_fails(self) -> None:
        status, output, errors = self.invoke(2)
        self.assertEqual(status, 1)
        self.assertNotIn("clean", output)
        self.assertIn("compiler produced no stderr", errors)

    def test_killed_compiler_fails(self) -> None:
        status, output, errors = self.invoke(-9)
        self.assertEqual(status, 1)
        self.assertNotIn("clean", output)
        self.assertIn("compiler exit -9", errors)

    def test_forbidden_overload_still_fails(self) -> None:
        status, output, errors = self.invoke(
            0, "src/fixture.cpp:7:2: warning: 'Thing::Extension()' is deprecated: CNAEXT: not XNA\n"
        )
        self.assertEqual(status, 1)
        self.assertNotIn("clean", output)
        self.assertIn("selects Thing::Extension()", errors)

    def test_destructor_exemption_still_passes(self) -> None:
        status, output, errors = self.invoke(
            0, "src/fixture.cpp:7:2: warning: 'Thing::~Thing()' is deprecated: CNAEXT: not XNA\n"
        )
        self.assertEqual(status, 0)
        self.assertIn("1 destructor hit(s) exempt", output)
        self.assertEqual(errors, "")

    def test_destructor_warning_cannot_hide_compile_failure(self) -> None:
        status, output, errors = self.invoke(
            1, "src/fixture.cpp:7:2: warning: 'Thing::~Thing()' is deprecated: CNAEXT: not XNA\n"
        )
        self.assertEqual(status, 1)
        self.assertNotIn("clean", output)
        self.assertIn("compiler exit 1", errors)


if __name__ == "__main__":
    unittest.main()
