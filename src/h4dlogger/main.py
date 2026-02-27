from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates


@dataclass(frozen=True)
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

        df = pd.DataFrame(rows, columns=["ts", "temperature", "humidity"])

        df["ts"] = pd.to_datetime(df["ts"], unit="s")

        # Average identical timestamps (each batch → one datapoint)
        df = df.groupby("ts", as_index=True).mean().sort_index()

        return df


def plot(df: pd.DataFrame) -> None:
    fig, ax1 = plt.subplots()

    ax1.scatter(df.index, df["temperature"], s=1, c="red", alpha=0.3)
    ax1.plot(df.index, df["temperature"].rolling(10).mean(), c="red")
    ax1.set_ylabel("Temperature (°C)")

    ax2 = ax1.twinx()
    ax2.scatter(df.index, df["humidity"], s=1, c="blue", alpha=0.3)
    ax2.plot(df.index, df["humidity"].rolling(10).mean(), c="blue")
    ax2.set_ylabel("Humidity (%)")

    ax1.set_xlabel("Time")

    # Omit seconds in display
    ax1.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
    fig.autofmt_xdate()

    ax1.grid(alpha=0.3)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    data_dir = Path("/home/m/openwrt/logs/")
    log_path = data_dir / "all_2026-02-27.log"

    log = SensorLog(log_path)
    df = log.parse_batches()
    plot(df)
