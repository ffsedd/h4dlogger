from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


@dataclass(frozen=True)
class Config:
    mount: Path = Path.home() / "mnt" / "dlogger"
    remote: str = "root@10.11.12.1:/mnt/data"
    project_dir: Path = Path.home() / "dev" / "h4dlogger"
    sshfs_opts: str = "reconnect,ServerAliveInterval=15,ServerAliveCountMax=3"
    timeout_s: int = 10


def _run(
    cmd: Sequence[str],
    *,
    cwd: Path | None = None,
    timeout: int | None = None,
    check: bool = True,
) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        cwd=cwd,
        timeout=timeout,
        check=check,
    )


def _is_mounted(path: Path) -> bool:
    result = _run(["mountpoint", "-q", str(path)], check=False)
    return result.returncode == 0


def _unmount(path: Path) -> None:
    # lazy unmount to recover from stale sshfs
    _run(["fusermount", "-u", "-z", str(path)], check=False)


def _mount(cfg: Config) -> None:
    cfg.mount.mkdir(parents=True, exist_ok=True)

    if _is_mounted(cfg.mount):
        return

    print("mounting logger...")

    try:
        _run(
            [
                "sshfs",
                "-o",
                cfg.sshfs_opts,
                cfg.remote,
                str(cfg.mount),
            ],
            timeout=cfg.timeout_s,
        )
    except subprocess.TimeoutExpired:
        # try to recover from stale mount
        print("sshfs timeout, attempting recovery...")
        _unmount(cfg.mount)
        _run(
            [
                "sshfs",
                "-o",
                cfg.sshfs_opts,
                cfg.remote,
                str(cfg.mount),
            ],
            timeout=cfg.timeout_s,
        )


def dlogger(cfg: Config = Config()) -> None:
    _mount(cfg)

    _run(
        ["uv", "run", "python", "-m", "h4dlogger.main"],
        cwd=cfg.project_dir,
    )
