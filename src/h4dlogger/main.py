from __future__ import annotations

from pathlib import Path

from .loader import load_logs
from .metrics import add_metrics
from .plot import plot


def main() -> None:

    log_dir = Path("/mnt/openwrt/logs/")
    paths = log_dir.glob("all_*.log")

    df = load_logs(paths)

    if df.empty:
        print("no data")
        return

    df = add_metrics(df)

    plot(
        df,
        roll_window="60min",
        resample=None,
    )


if __name__ == "__main__":
    main()
