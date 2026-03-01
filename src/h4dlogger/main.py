from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates


@dataclass(frozen=True, slots=True)
class SensorLog:
    path: Path

    def parse_batches(self) -> pd.DataFrame:
        rows: list[tuple[int, float, float]] = []

        with self.path.open() as f:
            for line in f:
                parts = line.strip().split(maxsplit=2)
                if len(parts) != 3:
                    continue

                ts_raw, topic, payload = parts
                if not topic.endswith("/sample"):
                    continue

                try:
                    temp_str, hum_str = payload.split(",")
                    rows.append((int(ts_raw), float(temp_str), float(hum_str)))
                except ValueError:
                    continue

        if not rows:
            return pd.DataFrame(columns=["temperature", "humidity"])

        df = pd.DataFrame(rows, columns=["ts", "temperature", "humidity"])
        df["ts"] = pd.to_datetime(df["ts"], unit="s")

        return (
            df.groupby("ts", as_index=True)
              .mean()
              .sort_index()
        )


def load_logs(paths: Iterable[Path]) -> pd.DataFrame:
    dfs: list[pd.DataFrame] = []

    for path in sorted(paths):
        df = SensorLog(path).parse_batches()
        if not df.empty:
            dfs.append(df)

    if not dfs:
        return pd.DataFrame(columns=["temperature", "humidity"])

    return pd.concat(dfs).sort_index()


def plot(df: pd.DataFrame) -> None:
    fig, ax1 = plt.subplots()

    temp_color = "red"
    hum_color = "blue"

    # Temperature
    ax1.scatter(df.index, df["temperature"], s=1, c=temp_color, alpha=0.3)
    ax1.plot(df.index, df["temperature"].rolling(60).mean(), c=temp_color)
    ax1.set_ylabel("Temperature (°C)", color=temp_color)
    ax1.tick_params(axis="y", labelcolor=temp_color)

    # Humidity
    ax2 = ax1.twinx()
    ax2.scatter(df.index, df["humidity"], s=1, c=hum_color, alpha=0.3)
    ax2.plot(df.index, df["humidity"].rolling(60).mean(), c=hum_color)
    ax2.set_ylabel("Humidity (%)", color=hum_color)
    ax2.tick_params(axis="y", labelcolor=hum_color)

    # X axis
    ax1.set_xlabel("Time (UTC)")
    ax1.xaxis.set_major_formatter(mdates.DateFormatter("%Y-%m-%d %H:%M"))

    ax1.grid(alpha=0.3)
    fig.autofmt_xdate()
    plt.tight_layout()
    plt.show()


def main():
    data_dir = Path("/mnt/openwrt/logs/")
    paths = data_dir.glob("all_*.log")  # daily logs
    print(paths)
    df = load_logs(paths)
    print(df.tail(30))
    plot(df)


if __name__ == "__main__":
    main()
