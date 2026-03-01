from __future__ import annotations

import numpy as np
import pandas as pd


def dew_point(temp: pd.Series, rh: pd.Series) -> pd.Series:
    a = 17.62
    b = 243.12

    gamma = (a * temp / (b + temp)) + np.log(rh / 100)
    return (b * gamma) / (a - gamma)


def absolute_humidity(temp: pd.Series, rh: pd.Series) -> pd.Series:
    return 6.112 * np.exp((17.67 * temp) / (temp + 243.5)) * rh * 2.1674 / (273.15 + temp)


def add_metrics(df: pd.DataFrame) -> pd.DataFrame:
    if "temp" not in df.columns.levels[0]:
        return df

    for sensor in df["temp"].columns:
        if sensor not in df["hum"].columns:
            continue

        t = df["temp"][sensor]
        h = df["hum"][sensor]

        df[("dew", sensor)] = dew_point(t, h)
        df[("abs_hum", sensor)] = absolute_humidity(t, h)

    return df
