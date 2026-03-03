from __future__ import annotations

import argparse
from pathlib import Path

from .loader import load_logs
from .metrics import add_metrics
from .plot import plot_dashboard


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument(
        "--log-dir",
        type=Path,
        default=Path.home() / "mnt" / "dlogger" / "logs",
    )
    return p.parse_args()


def main() -> None:
    args = parse_args()
    log_dir: Path = args.log_dir

    paths = sorted(log_dir.glob("*.log"))
    print(f"found {len(paths)} log files")

    df = load_logs(paths)
    print(f"loaded {len(df)} rows")

    if df.empty:
        print("no data")
        return

    df = add_metrics(df)

    plot_dashboard(
        df,
        roll_window="60min",
        resample=None,
    )


if __name__ == "__main__":
    main()
