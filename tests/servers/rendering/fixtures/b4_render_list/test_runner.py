#!/usr/bin/env python3
"""Test the fixture runner's marker validation."""

import unittest

from run import invalid_markers


class TestMarkers(unittest.TestCase):
    def setUp(self):
        self.lines = [
            "B4_LIST_REAL_MATERIALS=1",
            "B4_LIST_COLOR_ATOMIC=1",
            "B4_LIST_SHADOW_APPEND=1",
            "B4_LIST_RECOVERY=1",
            "B4_LIST_ORDINARY=1",
            "B4_LIST_FIRST_MISMATCH=1",
            "B4_LIST_RETIRED=1",
            "B4_LIST_EMPTY=1",
            "B4_LIST_ALL=1",
            "B4_LIST_LOAD=0",
            "B4_LIST_UNLOAD=0",
        ]

    def test_valid_markers(self):
        self.assertEqual(invalid_markers(self.lines), {})

    def test_conflicting_markers_are_not_accepted(self):
        self.lines.extend(["B4_LIST_COLOR_ATOMIC=0", "B4_LIST_LOAD=1"])
        self.assertEqual(set(invalid_markers(self.lines)), {"B4_LIST_COLOR_ATOMIC", "B4_LIST_LOAD"})

    def test_same_value_duplicate_and_missing_are_not_accepted(self):
        self.lines.append("B4_LIST_EMPTY=1")
        self.lines.remove("B4_LIST_UNLOAD=0")
        self.assertEqual(set(invalid_markers(self.lines)), {"B4_LIST_EMPTY", "B4_LIST_UNLOAD"})


if __name__ == "__main__":
    unittest.main()
