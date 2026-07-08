#!/usr/bin/env python3
"""Parse `pio run -t checkprogsize` output into a per-env size report.

Consumed by .github/workflows/measure-firmware-size.yml to produce a
firmware-size PR comment without rebuilding.

checkprogsize output looks like:

    RAM:   [==        ]  13.9% (used 45678 bytes from 327680 bytes)
    Flash: [======    ]  62.8% (used 1234567 bytes from 1966080 bytes)
"""
import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass


@dataclass
class Section:
    used_bytes: int
    total_bytes: int


@dataclass
class Size:
    ram: Section | None
    flash: Section | None


@dataclass
class BoardSize:
    schema_version: int
    pio_env: str
    board_name: str
    sha: str
    flash: Section | None
    ram: Section | None


RAM_RE = re.compile(r"^RAM:.*used\s+(\d+)\s+bytes\s+from\s+(\d+)\s+bytes", re.MULTILINE)
FLASH_RE = re.compile(r"^Flash:.*used\s+(\d+)\s+bytes\s+from\s+(\d+)\s+bytes", re.MULTILINE)


def _match(regex: re.Pattern[str], text: str) -> Section | None:
    m = regex.search(text)
    if m is None:
        return None
    return Section(used_bytes=int(m.group(1)), total_bytes=int(m.group(2)))


def parse(text: str) -> Size:
    return Size(ram=_match(RAM_RE, text), flash=_match(FLASH_RE, text))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pio-env", required=True)
    ap.add_argument("--board-name", required=True)
    ap.add_argument("--sha", required=True)
    ap.add_argument("--out", required=True, help="Path to write size report JSON")
    args = ap.parse_args()

    sizes = parse(sys.stdin.read())
    if sizes.ram is None and sizes.flash is None:
        # Neither line found — checkprogsize probably didn't run. Fail loudly
        # so a broken producer surfaces at PR time instead of silently
        # emitting empty size reports.
        print("parse_size: no Flash: or RAM: lines found on stdin", file=sys.stderr)
        return 2

    doc = BoardSize(
        schema_version=1,
        pio_env=args.pio_env,
        board_name=args.board_name,
        sha=args.sha,
        flash=sizes.flash,
        ram=sizes.ram,
    )
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(asdict(doc), f, indent=2)
        f.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
