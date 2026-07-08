#!/usr/bin/env python3
"""Parse `pio run -t checkprogsize` output into a per-env size sidecar.

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


LINE_RE = re.compile(
    r"^(RAM|Flash):.*used\s+(\d+)\s+bytes\s+from\s+(\d+)\s+bytes",
    re.MULTILINE,
)


def parse(text: str) -> dict[str, dict[str, int] | None]:
    out: dict[str, dict[str, int] | None] = {"ram": None, "flash": None}
    for m in LINE_RE.finditer(text):
        kind = m.group(1).lower()
        out[kind] = {"used_bytes": int(m.group(2)), "total_bytes": int(m.group(3))}
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pio-env", required=True)
    ap.add_argument("--board-name", required=True)
    ap.add_argument("--sha", required=True)
    ap.add_argument("--out", required=True, help="Path to write sidecar JSON")
    args = ap.parse_args()

    text = sys.stdin.read()

    parsed = parse(text)
    if parsed["flash"] is None and parsed["ram"] is None:
        # Neither line found — checkprogsize probably didn't run. Fail loudly
        # so a broken producer surfaces at PR time instead of silently
        # emitting empty sidecars.
        print("parse_size: no Flash: or RAM: lines found on stdin", file=sys.stderr)
        return 2

    doc = {
        "schema_version": 1,
        "pio_env": args.pio_env,
        "board_name": args.board_name,
        "sha": args.sha,
        "flash": parsed["flash"],
        "ram": parsed["ram"],
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
