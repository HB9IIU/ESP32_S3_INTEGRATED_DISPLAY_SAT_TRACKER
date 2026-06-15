Import("env")
import subprocess, datetime

def get_version():
    try:
        v = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty=+"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
        return v if v else None
    except Exception:
        return None

version = get_version() or datetime.datetime.utcnow().strftime("build-%Y%m%d")
print(f"[version] FIRMWARE_VERSION = {version}")
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", '\\"' + version + '\\"')])
