"""Pre-build script: stage web_data/ui/ as gzipped LittleFS image contents.

Registered via `extra_scripts = pre:Software/src/lib/build_webfs.py` in
platformio.ini. Runs before every build. Walks web_data/ui/**, compresses each
.html/.css/.js/.svg with the `zopfli` binary (max compression, --i10000),
and writes the result to web_data/ui_staged/*.gz. platformio.ini's
`board_build.filesystem_dir = web_data/ui_staged` then packs that directory
into the LittleFS image at `pio run -t buildfs`.

The staged tree is always regenerated from scratch (no mtime shortcut). zopfli
is a required build dependency; a missing binary is a hard error.

Sibling to Software/src/lib/update_ota_html_gzip.py (the ElegantOTA HTML
regenerator). That script converts to a C++ byte array; this one produces
loose files for LittleFS.
"""
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821  (PlatformIO injects `Import` at exec time)

ROOT = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
SRC = ROOT / "web_data" / "ui"
STAGE = ROOT / "web_data" / "ui_staged"
EXTS = {".html", ".css", ".js", ".svg"}

if shutil.which("zopfli") is None:
    sys.stderr.write(
        "build_webfs.py: `zopfli` binary not found on PATH.\n"
        "  Install: `choco install zopfli` (Windows), `brew install zopfli`\n"
        "  (macOS), `apt install zopfli` (Debian/Ubuntu), or build from\n"
        "  https://github.com/google/zopfli.\n"
    )
    sys.exit(1)

if not SRC.is_dir():
    sys.stderr.write(f"build_webfs.py: source dir {SRC} does not exist.\n")
    sys.exit(1)

# Wipe the staging tree so removed source files don't linger as stale .gz.
if STAGE.exists():
    shutil.rmtree(STAGE)
STAGE.mkdir(parents=True)

# version.txt: short git sha of the working tree, or "unknown" outside git.
try:
    sha = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True
    ).strip()
except (subprocess.CalledProcessError, FileNotFoundError):
    sha = "unknown"
(STAGE / "version.txt").write_text(sha + "\n")

total_in = 0
total_out = 0
sizes = []  # (relative_path, gz_bytes)

for path in sorted(SRC.rglob("*")):
    if not path.is_file() or path.suffix.lower() not in EXTS:
        continue
    rel = path.relative_to(SRC)
    out = STAGE / rel.with_name(rel.name + ".gz")
    out.parent.mkdir(parents=True, exist_ok=True)
    raw = path.read_bytes()
    # `zopfli -c --i10000 <file>` writes the gzip stream to stdout.
    gz = subprocess.check_output(
        ["zopfli", "-c", "--i10000", "--gzip", str(path)]
    )
    out.write_bytes(gz)
    total_in += len(raw)
    total_out += len(gz)
    sizes.append((str(rel).replace("\\", "/"), len(gz)))

# Sort largest-first for the summary.
sizes.sort(key=lambda t: t[1], reverse=True)
BUDGET_HINT = 128 * 1024  # min_spiffs partition size; informational only.

print("build_webfs.py: staged web_data/ui/ -> web_data/ui_staged/")
print(f"  {len(sizes)} files, {total_in} B raw -> {total_out} B gz "
      f"(ratio {total_in / max(total_out, 1):.2f}x)")
for name, size in sizes[:3]:
    print(f"    {size:>7} B  {name}.gz")
free = BUDGET_HINT - total_out
print(f"  ~{free // 1024} KB free vs 128 KB min_spiffs partition "
      f"(informational).")
