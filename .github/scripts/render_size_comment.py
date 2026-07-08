#!/usr/bin/env python3
"""Render a firmware-size PR comment from base-side and PR-side size sidecars.

Consumed by .github/workflows/measure-firmware-size.yml. Each side is a
directory (potentially with subdirectories from `merge-multiple: true`)
containing one `<pio_env>.size.json` per board built by
`.github/workflows/compile-common-image.yml`.

Rows = union of pio_envs found in either side, sorted by PR-side flash
fill percentage descending so the boards closest to their partition limit
surface first.
"""
import argparse
import json
import pathlib
import sys


STICKY_MARKER = "<!-- firmware-size-report -->"
WARN_FILL_THRESHOLD = 0.90  # append ⚠️ when PR fill > 90% AND flash grew


def load_sidecars(root: str) -> dict[str, dict]:
    """Return {pio_env: sidecar_dict} for every *.size.json under root."""
    out: dict[str, dict] = {}
    p = pathlib.Path(root)
    if not p.exists():
        return out
    for f in p.rglob("*.size.json"):
        try:
            with f.open(encoding="utf-8") as fh:
                doc = json.load(fh)
        except (OSError, json.JSONDecodeError):
            continue
        env = doc.get("pio_env")
        if env:
            out[env] = doc
    return out


def short(sha: str) -> str:
    return (sha or "")[:7] or "?"


def fmt_bytes(n: int | None) -> str:
    if n is None:
        return "—"
    return f"{n:,}"


def fmt_delta(delta: int | None) -> str:
    if delta is None:
        return ""
    if delta == 0:
        return "0"
    return f"{delta:+,}"


def fmt_pct(used: int | None, total: int | None) -> str:
    if used is None or not total:
        return "—"
    return f"{(used / total) * 100:.2f}%"


def sort_key(env: str, pr: dict | None) -> tuple[float, str]:
    """Sort by PR-side flash fill % desc; envs without PR data go last."""
    if pr and pr.get("flash"):
        fl = pr["flash"]
        if fl.get("total_bytes"):
            return (-(fl["used_bytes"] / fl["total_bytes"]), env)
    return (float("inf"), env)


def render_row(env: str, base: dict | None, pr: dict | None) -> str:
    board = (pr or base or {}).get("board_name", env)

    if pr is None:
        # PR-side sidecar missing → build failed on PR head for this env.
        # Leave the PR % column blank; we can't report a percentage we don't have.
        base_flash = (base or {}).get("flash") or {}
        return (
            f"| {board} | `{env}` | {fmt_bytes(base_flash.get('used_bytes'))} "
            f"| build failed | {fmt_bytes(base_flash.get('total_bytes'))} "
            f"| — | — | — | — |"
        )

    pr_flash = pr.get("flash") or {}
    pr_used = pr_flash.get("used_bytes")
    pr_total = pr_flash.get("total_bytes")
    pr_pct_str = fmt_pct(pr_used, pr_total)

    if base is None:
        # No base-side sidecar — new board, or base build failed for this env.
        delta_cell = "(new)"
        base_used_cell = "—"
    else:
        base_flash = base.get("flash") or {}
        base_used = base_flash.get("used_bytes")
        base_used_cell = fmt_bytes(base_used)
        if pr_used is not None and base_used is not None:
            delta = pr_used - base_used
            delta_cell = fmt_delta(delta)
            # ⚠️ when the board is already tight (>90% full) and it grew.
            if delta > 0 and pr_total and (pr_used / pr_total) > WARN_FILL_THRESHOLD:
                delta_cell += " ⚠️"
        else:
            delta_cell = "—"

    # RAM columns (base used, pr used, delta) — no fill % for RAM; the
    # heap allocations dominate runtime RAM anyway, so absolute deltas are
    # what reviewers care about.
    pr_ram = pr.get("ram") or {}
    pr_ram_used = pr_ram.get("used_bytes")
    if base and base.get("ram") and pr_ram_used is not None:
        base_ram_used = base["ram"].get("used_bytes")
        if base_ram_used is not None:
            ram_delta = fmt_delta(pr_ram_used - base_ram_used)
        else:
            ram_delta = "—"
    else:
        ram_delta = "—" if base else "(new)"
    ram_pr_cell = fmt_bytes(pr_ram_used)

    return (
        f"| {board} | `{env}` | {base_used_cell} | {fmt_bytes(pr_used)} "
        f"| {fmt_bytes(pr_total)} | {pr_pct_str} | {delta_cell} "
        f"| {ram_pr_cell} | {ram_delta} |"
    )


def render(
    base: dict[str, dict],
    pr: dict[str, dict],
    base_branch: str,
    base_sha: str,
    head_sha: str,
    no_baseline: bool,
) -> str:
    envs = sorted(set(base) | set(pr), key=lambda e: sort_key(e, pr.get(e)))

    lines: list[str] = [
        STICKY_MARKER,
        "## Firmware size report",
        "",
    ]

    if no_baseline:
        lines.append(
            f"> No successful compile run found on base branch `{base_branch}` yet; "
            "showing absolute sizes only. Deltas will appear once `main` has been built."
        )
        lines.append("")
    else:
        lines.append(
            f"Base: `{short(base_sha)}` on `{base_branch}` · This PR: `{short(head_sha)}`"
        )
        lines.append("")

    lines.append(
        "| Board | Env | Base flash | PR flash | Max | PR % | Δ flash | PR RAM | Δ RAM |"
    )
    lines.append("| - | - | -: | -: | -: | -: | -: | -: | -: |")

    for env in envs:
        lines.append(render_row(env, base.get(env), pr.get(env)))

    lines.append("")
    lines.append(
        "⚠️ = board already >90% full and flash grew. "
        "Sorted by PR flash fill % (tightest first)."
    )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pr-dir", required=True)
    ap.add_argument("--base-dir", required=True)
    ap.add_argument("--base-branch", required=True)
    ap.add_argument("--base-sha", required=True)
    ap.add_argument("--head-sha", required=True)
    ap.add_argument("--no-baseline", default="false")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    no_baseline = args.no_baseline.lower() == "true"
    base = {} if no_baseline else load_sidecars(args.base_dir)
    pr = load_sidecars(args.pr_dir)

    if not pr and not base:
        # Nothing to say. Emit a minimal note so the sticky comment
        # updates in place instead of leaving stale numbers.
        body = (
            f"{STICKY_MARKER}\n"
            "## Firmware size report\n\n"
            "> No size sidecars found in either the PR build or the base build.\n"
        )
    else:
        body = render(base, pr, args.base_branch, args.base_sha, args.head_sha, no_baseline)

    with open(args.out, "w", encoding="utf-8") as f:
        f.write(body)
    return 0


if __name__ == "__main__":
    sys.exit(main())
