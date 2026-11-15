#!/usr/bin/env python3
"""Run only real pending-VA transfer case2; preserves the original case0/1 runner."""

from run_native import main

if __name__ == "__main__":
    raise SystemExit(main(case_indices=(2,)))
