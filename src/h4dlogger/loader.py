from __future__ import annotations

from pathlib import Path
from typing import Iterable

import pandas as pd

from .parser import SensorLog


def load_logs(paths: Iterable[Path]) -> pd.DataFrame:
    dfs = []

    for p in sorted(paths):
        df = SensorLog(p).parse(sort=False)
        if not df.empty:
            dfs.append(df)

    if not dfs:
        return pd.DataFrame()

    df = pd.concat(dfs)

    # Remove duplicate rows
    df = df.drop_duplicates()

    # Filter only base sensors (omit _min, _max, _grad)
    df = df[~df["metric"].str.endswith(("_min", "_max"))]

    # Pivot to MultiIndex: (metric/unit, sensor_id)
    df = df.pivot_table(
        index="ts", columns=["metric", "sensor"], values="value", aggfunc="mean"
    ).sort_index()

    # Name the levels for plotting
    df.columns.names = ["metric", "sensor"]

    print(f"Loaded data with shape {df.shape} and columns: {df.columns.tolist()}")
    return df


if __name__ == "__main__":
    df = load_logs([Path("/home/m/mnt/dlogger/logs/kitchen_2026-03-28.log")])
    print(df)
