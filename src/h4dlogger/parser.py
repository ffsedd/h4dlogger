from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import pandas as pd


@dataclass(frozen=True, slots=True)
class SensorLog:
    path: Path

    def rows(self) -> Iterable[tuple[int, str, str, float]]:
        with self.path.open() as f:
            for line in f:
                parts = line.split(maxsplit=2)
                if len(parts) != 3:
                    continue

                ts_raw, topic, payload = parts
                ts = int(ts_raw)

                if topic.endswith("/sample"):
                    sensor = topic.removesuffix("/sample")
                    try:
                        t, h = payload.split(",")
                        yield ts, sensor, "temp", float(t)
                        yield ts, sensor, "hum", float(h)
                    except ValueError:
                        continue

                else:
                    try:
                        sensor, unit = topic.rsplit("/", 1)
                        yield ts, sensor, unit, float(payload)
                    except ValueError:
                        continue

    def parse(self) -> pd.DataFrame:
        df = pd.DataFrame(self.rows(), columns=["ts", "sensor", "unit", "value"])

        if df.empty:
            return df

        df["ts"] = pd.to_datetime(df["ts"], unit="s", utc=True)
        return df
