from __future__ import annotations

from pathlib import Path
from typing import Iterable

import pandas as pd

from .parser import SensorLog


def load_logs(paths: Iterable[Path]) -> pd.DataFrame:
    dfs = []

    for p in sorted(paths):
        df = SensorLog(p).parse()
        if not df.empty:
            dfs.append(df)

    if not dfs:
        return pd.DataFrame()

    df = pd.concat(dfs)

    df = (
        df.pivot_table(
            index="ts",
            columns=["unit", "sensor"],
            values="value",
            aggfunc="mean",
        )
        .sort_index()
    )

    return df
