#!/usr/bin/env python3
"""Decode the fixed 4096 x 768-byte native drain diagnostic ring after a run."""
import argparse
from pathlib import Path
import re
import sys

SLOT_BYTES = 768
SLOT_COUNT = 4096


def decode(data):
    if len(data) != SLOT_BYTES * SLOT_COUNT:
        raise ValueError(f"expected {SLOT_BYTES * SLOT_COUNT} bytes, got {len(data)}")
    records = []
    invalid = []
    for index in range(SLOT_COUNT):
        slot = data[index * SLOT_BYTES : (index + 1) * SLOT_BYTES]
        if not any(slot):
            continue
        line = slot.split(b"\0", 1)[0]
        match = re.fullmatch(rb"(.+) checksum=([0-9a-f]{16})\n", line)
        if not match:
            invalid.append(index)
            continue
        payload, expected = match.groups()
        checksum = 14695981039346656037
        for value in payload:
            checksum = ((checksum ^ value) * 1099511628211) & ((1 << 64) - 1)
        seq = re.search(rb"\bseq=(\d+)\b", payload)
        if checksum != int(expected, 16) or not seq:
            invalid.append(index)
            continue
        sequence = int(seq.group(1))
        if sequence < 1 or (sequence - 1) % SLOT_COUNT != index:
            invalid.append(index)
            continue
        records.append((sequence, line.decode("ascii").rstrip("\n")))
    records.sort()
    return records, invalid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ring", type=Path)
    parser.add_argument("--output", type=Path, help="Write sorted records to this file instead of stdout")
    args = parser.parse_args()
    try:
        records, invalid = decode(args.ring.read_bytes())
    except (OSError, ValueError) as exc:
        parser.exit(2, f"diagnostic ring error: {exc}\n")
    text = "".join(line + "\n" for _, line in records)
    if args.output:
        args.output.write_text(text, encoding="ascii")
    else:
        sys.stdout.write(text)
    span = f"{records[0][0]}..{records[-1][0]}" if records else "empty"
    print(f"valid={len(records)} seq={span} invalid_or_torn_slots={invalid}", file=sys.stderr)
    print("The ring retains only the latest window. Missing earlier span entries may have been overwritten; correlate same-thread graph nesting and pair IDs.", file=sys.stderr)


if __name__ == "__main__":
    main()
