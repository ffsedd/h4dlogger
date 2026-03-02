from __future__ import annotations

from pathlib import Path

from .loader import load_logs
from .metrics import add_metrics
from .plot import plot_dashboard


def main() -> None:

    log_dir = Path.home() / "mnt" / "dlogger" / "logs"

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