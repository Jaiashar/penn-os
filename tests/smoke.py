"""Exercise PennFAT persistence and the OS's real interactive process API."""
from pathlib import Path
import re
import subprocess
import tempfile
from terminal import Terminal

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="penn-os-ci-") as work:
    work = Path(work)
    payload = bytes(range(256)) * 16
    (work / "input.bin").write_bytes(payload)
    commands = "\n".join([
        "mkfs smoke.fat 1 0", "mount smoke.fat", "touch smoke.txt",
        "cp -h input.bin stored", "ls", "umount", "mount smoke.fat",
        "cp stored -h output.bin", "umount", "",
    ])
    result = subprocess.run([str(ROOT / "bin/pennfat")], input=commands,
                            text=True, capture_output=True, cwd=work, timeout=15, check=True)
    print(result.stdout, result.stderr, flush=True)
    assert "smoke.txt" in result.stdout + result.stderr
    assert (work / "output.bin").read_bytes() == payload, "PennFAT persistence mismatch"
    terminal = Terminal([str(ROOT / "bin/penn-os"), "smoke.fat", "smoke.log"], str(work), "$ ")
    try:
        terminal.expect_prompt()
        output = terminal.command("echo CI_OS_OK")
        assert re.search(r"(?m)^CI_OS_OK\s*$", output), output
        output = terminal.command("ps")
        assert all(column in output for column in ("PID", "PPID", "PRI", "STAT", "CMD")), output
        assert re.search(r"(?m)^\d+\s+\d+\s+-?\d+\s+\w\s+shell\s*$", output), output
        terminal.command("sleep 5 &")
        output = terminal.command("jobs")
        assert "sleep 5" in output and "running" in output.lower(), output
        terminal.finish(b"logout\n")
    finally:
        terminal.close()
    assert (work / "smoke.log").stat().st_size > 0, "Missing scheduler log"
print("PASS: PennFAT multi-block round trip across remount, OS boot, echo, ps, background jobs, logout")
