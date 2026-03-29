from __future__ import annotations

import numpy as np
import pandas as pd

pd.set_option("display.max_rows", 500)
pd.set_option("display.max_columns", 500)
pd.set_option("display.width", 1000)


def dew_point(temp: pd.Series, rh: pd.Series) -> pd.Series:
    a = 17.62
    b = 243.12
    gamma = (a * temp / (b + temp)) + np.log(rh / 100)
    return (b * gamma) / (a - gamma)


def absolute_humidity(temp: pd.Series, rh: pd.Series) -> pd.Series:
    return 6.112 * np.exp((17.67 * temp) / (temp + 243.5)) * rh * 2.1674 / (273.15 + temp)  # type: ignore


def add_abs_humidity(df: pd.DataFrame) -> pd.DataFrame:
    print("Starting metric calculation...")

    if df.columns.nlevels != 2:
        print("Expected MultiIndex with 2 levels (unit, sensor). Skipping metrics.")
        return df

    sensors = set(sensor for _, sensor in df.columns if "min" not in _ and "max" not in _)
    print(f"Processing sensors: {sensors}")

    for sensor in sensors:
        metrics = [unit for unit, s in df.columns if s == sensor]
        print(f"Sensor '{sensor}' has metrics: {metrics}")
        if "rh_mean" not in metrics:
            continue
        t = df[("temp_mean", sensor)]
        h = df[("rh_mean", sensor)]

        df[("dew", sensor)] = dew_point(t, h)
        df[("abs_hum", sensor)] = absolute_humidity(t, h)
        print(f"Added metrics for sensor '{sensor}': ('dew', '{sensor}'), ('abs_hum', '{sensor}')")

    print("Metric calculation complete.")
    print(f"DataFrame now has columns: {df.columns.tolist()}")
    print(f"DataFrame shape after adding metrics: {df.shape}")

    return df
