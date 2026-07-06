#!/usr/bin/env python3
"""
publish_release.py — Build and publish a firmware release to GitHub.

Usage:
  python publish_release.py              # interactive
  python publish_release.py patch        # auto-bump patch (1.0.0 → 1.0.1)
  python publish_release.py minor        # auto-bump minor (1.0.0 → 1.1.0)
  python publish_release.py major        # auto-bump major (1.0.0 → 2.0.0)
  python publish_release.py 1.2.3        # explicit version

Steps:
  1. Determine new version
  2. Create git tag vX.Y.Z
  3. Build firmware via PlatformIO (get_version.py reads the tag)
  4. Create GitHub release and upload firmware.bin
"""

import subprocess, sys, re, os

PLATFORMIO_ENV = "DISPLAY"
FIRMWARE_BIN   = f".pio/build/{PLATFORMIO_ENV}/firmware.bin"
LITTLEFS_BIN   = f".pio/build/{PLATFORMIO_ENV}/littlefs.bin"
GITHUB_REPO    = "HB9IIU/ESP32_S3_INTEGRATED_DISPLAY_SAT_TRACKER"


def run(cmd):
    print(f"\n$ {cmd}")
    result = subprocess.run(cmd, shell=True)
    if result.returncode != 0:
        print(f"\nERROR: command exited with code {result.returncode}")
        sys.exit(1)


def get_output(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True).stdout.strip()


def current_version():
    v = get_output("git describe --tags --abbrev=0 2>/dev/null")
    if v.startswith(('v', 'V')):
        v = v[1:]
    return v or "0.0.0"


def bump_version(ver, bump):
    m = re.match(r'^(\d+)\.(\d+)\.(\d+)', ver)
    if not m:
        print(f"Cannot parse current version: {ver}")
        sys.exit(1)
    ma, mi, pa = int(m[1]), int(m[2]), int(m[3])
    if bump == "major": return f"{ma+1}.0.0"
    if bump == "minor": return f"{ma}.{mi+1}.0"
    return f"{ma}.{mi}.{pa+1}"  # patch


def main():
    cur = current_version()
    print(f"Current version: v{cur}")

    arg = sys.argv[1] if len(sys.argv) > 1 else None

    if arg in ("major", "minor", "patch"):
        new = bump_version(cur, arg)
    elif arg:
        new = arg.lstrip('vV')
    else:
        print("Bump type? [patch/minor/major] or enter version (e.g. 1.2.0):")
        inp = input("> ").strip()
        if inp in ("major", "minor", "patch"):
            new = bump_version(cur, inp)
        else:
            new = inp.lstrip('vV')

    if not re.match(r'^\d+\.\d+\.\d+$', new):
        print(f"Invalid version: '{new}'  (expected X.Y.Z)")
        sys.exit(1)

    tag = f"v{new}"
    print(f"\nReleasing {tag}  (was v{cur})")

    ans = input("\nProceed? [y/N] ").strip().lower()
    if ans != 'y':
        print("Aborted.")
        sys.exit(0)

    # Warn on dirty tree but don't block
    dirty = get_output("git status --porcelain")
    if dirty:
        print(f"\nWarning: uncommitted changes detected:\n{dirty}")
        ans = input("Tag anyway? [y/N] ").strip().lower()
        if ans != 'y':
            sys.exit(0)

    # Tag first so get_version.py picks up the new version during build
    run(f'git tag {tag} -m "Release {tag}"')

    # Build firmware (get_version.py reads git describe → outputs X.Y.Z as FIRMWARE_VERSION)
    run(f"pio run -e {PLATFORMIO_ENV}")

    # Build LittleFS image
    run(f"pio run -e {PLATFORMIO_ENV} -t buildfs")

    for path in (FIRMWARE_BIN, LITTLEFS_BIN):
        if not os.path.exists(path):
            print(f"\nERROR: {path} not found — build may have failed")
            sys.exit(1)

    fw_size = os.path.getsize(FIRMWARE_BIN)
    fs_size = os.path.getsize(LITTLEFS_BIN)
    print(f"\nFirmware : {FIRMWARE_BIN}  ({fw_size:,} bytes / {fw_size/1024:.1f} KB)")
    print(f"Filesystem: {LITTLEFS_BIN}  ({fs_size:,} bytes / {fs_size/1024:.1f} KB)")

    # Push tag
    run("git push origin --tags")

    # Create GitHub release and upload both binaries
    run(
        f'gh release create {tag} "{FIRMWARE_BIN}" "{LITTLEFS_BIN}" '
        f'--repo {GITHUB_REPO} '
        f'--title "{tag}" '
        f'--notes "Satellite Tracker firmware release {tag}"'
    )

    print(f"\nDone!  https://github.com/{GITHUB_REPO}/releases/tag/{tag}")


if __name__ == "__main__":
    main()
