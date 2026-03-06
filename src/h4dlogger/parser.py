from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import pandas as pd


@dataclass(frozen=True, slots=True)
class SensorLog:
    """
    Parser for line-based sensor log files.

    Expected log line format:

        <timestamp> <topic> <payload>

    Example:
        1700000000 sensors/device1/temp/temp_mean 21.5

    Topic structure must contain four parts:

        prefix/device/sensor/metric

    Example:
        sensors/device1/temp/temp_mean

    The parser extracts:
        ts         Unix timestamp (seconds)
        sensor_id  "device/sensor"
        metric     metric name
        value      float value
    """

    path: Path

    def rows(self) -> Iterable[tuple[int, str, str, float]]:
        """
        Stream parsed rows from the log file.

        Yields
        ------
        tuple
            (timestamp, sensor_id, metric, value)

        Invalid lines are skipped if:
        - the line does not have exactly 3 whitespace-separated parts
        - timestamp is not an integer
        - payload is not a float
        - topic does not contain exactly four segments
        """

        with self.path.open() as f:
            for line in f:
                # Split only first two spaces so payload may contain spaces
                parts = line.split(maxsplit=2)
                if len(parts) != 3:
                    continue

                ts_raw, topic, payload = parts

                # Validate timestamp and numeric payload
                try:
                    ts = int(ts_raw)
                    value = float(payload)
                except ValueError:
                    continue

                # Parse topic structure
                t = topic.split("/")

                if len(t) != 4:
                    continue

                _, device, sensor, metric = t

                # Compose sensor identifier
                sensor_id = f"{device}/{sensor}"

                yield ts, sensor_id, metric, value

    def parse(self) -> pd.DataFrame:
        """
        Parse the log file into a pandas DataFrame.

        Returns
        -------
        DataFrame
            Columns:
            - ts (datetime64[ns, UTC])
            - sensor (str)
            - unit (str)
            - value (float)

        Empty files or files without valid lines return an empty DataFrame.
        """

        df = pd.DataFrame(self.rows(), columns=["ts", "sensor", "unit", "value"])

        if df.empty:
            return df

        # Convert unix timestamp to timezone-aware datetime
        df["ts"] = pd.to_datetime(df["ts"], unit="s", utc=True)

        return df