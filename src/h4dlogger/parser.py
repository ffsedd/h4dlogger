from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional

import pandas as pd


@dataclass(frozen=True, slots=True)
class SensorLog:
    path: Path

    def rows(
        self, start_ts: Optional[int] = None, end_ts: Optional[int] = None
    ) -> Iterable[tuple[int, str, str, str, float]]:
        """
        Stream parsed rows from the log file (CSV-style: topic,ts,value)
        with optional timestamp filtering.

        Yields
        ------
        tuple[int, str, str, str, float]
            (timestamp, device, sensor, metric, value)
        """
        if not Path(self.path).is_file():
            raise FileNotFoundError(f"File not found: {self.path}")
        with Path(self.path).open() as f:
            for line in f:
                parts = line.strip().split(",", 2)
                if len(parts) != 3:
                    continue

                topic, ts_raw, payload = parts

                try:
                    ts = int(ts_raw)
                    value = float(payload)
                except ValueError:
                    continue

                if start_ts is not None and ts < start_ts:
                    continue
                if end_ts is not None and ts > end_ts:
                    continue

                t = topic.split("/")
                if len(t) < 3:
                    continue  # malformed topic

                device = t[0]
                sensor = t[1]
                metric = t[2]

                yield ts, device, sensor, metric, value

    def parse(self, sort: bool = True) -> pd.DataFrame:
        """
        Parse the log file into a DataFrame.

        Returns
        -------
        pd.DataFrame
            Columns: ts (UTC datetime), sensor, unit, value
        """
        df = pd.DataFrame(self.rows(), columns=["ts", "device", "sensor", "metric", "value"])
        if df.empty:
            return df

        df["ts"] = pd.to_datetime(df["ts"], unit="s", utc=True)

        if sort:
            df = df.sort_values(["device", "metric", "sensor"]).reset_index(drop=True)

        return df


if __name__ == "__main__":
    s = SensorLog(Path("/home/m/mnt/dlogger/logs/kitchen_2026-03-28.log"))
    df = s.parse()
    print(df)
    print(df.describe())
    print(df[df.metric == "co2_grad"])
