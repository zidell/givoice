#!/usr/bin/env python3
"""Run the installed desktop app independently of the invoking terminal/agent."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import time
import uuid


ROOT = Path(__file__).resolve().parents[2]
DEST = Path(os.environ.get("GIVOICE_INSTALL_PREFIX", Path.home() / ".local"))
BINARY = (DEST / "bin/givoice").absolute()


def instances():
    found = []
    for proc in Path("/proc").iterdir():
        if not proc.name.isdigit():
            continue
        try:
            if proc.stat().st_uid != os.getuid():
                continue
            exe = os.readlink(proc / "exe").removesuffix(" (deleted)")
            args = (proc / "cmdline").read_bytes().split(b"\0")
            if exe == str(BINARY) and b"--overlay" not in args:
                found.append(int(proc.name))
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
    return found


def require_idle():
    result = subprocess.run([
        "gdbus", "call", "--session", "--dest", "com.videostew.givoice",
        "--object-path", "/com/videostew/givoice/Escape",
        "--method", "com.videostew.givoice.Escape.GetState",
    ], capture_output=True, text=True, timeout=5)
    if result.returncode or result.stdout.strip() != "(false,)":
        raise RuntimeError("Givoice is busy or its idle state cannot be verified; left running")


def wait_until(check, message):
    for _ in range(100):
        if check():
            return
        time.sleep(0.1)
    raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--restart", action="store_true", help="restart only when idle")
    mode.add_argument("--build", action="store_true",
                      help="build and test, install, then restart only when idle")
    args = parser.parse_args()
    # Verify the independent launcher before touching a running app.
    subprocess.run(["systemctl", "--user", "show-environment"],
                   check=True, stdout=subprocess.DEVNULL)
    if args.build:
        subprocess.run(["bash", str(ROOT / "native/linux/build.sh"), "--test"], check=True)
    current = instances()
    if current and not (args.restart or args.build):
        print(f"Givoice already running: {current}")
        return
    if current:
        require_idle()
    if args.build:
        subprocess.run(["bash", str(ROOT / "native/linux/install.sh")], check=True)
    if not BINARY.is_file():
        raise RuntimeError(f"Installed app missing: {BINARY}; use --build first")
    if current:
        # Installation must succeed before asking the old app to exit.
        require_idle()
        for pid in current:
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        wait_until(lambda: not instances(), "Graceful shutdown timed out; no second app started")
    unit = "givoice-local-" + uuid.uuid4().hex
    subprocess.run([
        "systemd-run", "--user", "--collect", "--unit=" + unit,
        "--service-type=exec", "--property=ExitType=cgroup", "--", str(BINARY),
    ], check=True)
    # ExitType=cgroup also keeps the tray's delayed restart child alive after
    # its original main process exits. No login unit is installed or enabled.
    def ready():
        result = subprocess.run([
            "gdbus", "call", "--session", "--dest", "com.videostew.givoice",
            "--object-path", "/com/videostew/givoice/Escape",
            "--method", "com.videostew.givoice.Escape.GetState",
        ], capture_output=True, timeout=5)
        return bool(instances()) and result.returncode == 0
    wait_until(ready, f"Givoice did not become ready; inspect journalctl --user -u {unit}")
    print(f"Givoice running independently: unit={unit}, pids={instances()}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
