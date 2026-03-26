from __future__ import annotations

import numpy as np
import pandas as pd


pd.set_option('display.max_rows', 500)
pd.set_option('display.max_columns', 500)
pd.set_option('display.width', 1000)

def dew_point(temp: pd.Series, rh: pd.Series) -> pd.Series:
    a = 17.62
    b = 243.12
    gamma = (a * temp / (b + temp)) + np.log(rh / 100)
    return (b * gamma) / (a - gamma)

def absolute_humidity(temp: pd.Series, rh: pd.Series) -> pd.Series:
    return 6.112 * np.exp((17.67 * temp) / (temp + 243.5)) * rh * 2.1674 / (273.15 + temp) # type: ignore

def add_metrics(df: pd.DataFrame) -> pd.DataFrame:
    print("Starting metric calculation...")

    if df.columns.nlevels != 2:
        print("Expected MultiIndex with 2 levels (unit, sensor). Skipping metrics.")
        return df

    # Only consider mean temperature/humidity columns
    temp_cols = [c for c in df.columns if c[0] in ("temp_mean",)]
    rh_cols   = [c for c in df.columns if c[0] in ("rh_mean",)]
    co2_cols   = [c for c in df.columns if c[0] in ("co2_smooth",)]

    print(f"Found temperature columns: {[c[1] for c in temp_cols]}")
    print(f"Found humidity columns: {[c[1] for c in rh_cols]}")

    # Match sensors that have both temp_mean and rh_mean
    sensors = set(c[1] for c in temp_cols) & set(c[1] for c in rh_cols)
    print(f"Processing sensors: {sensors}")

    for sensor in sensors:
        t = df[("temp_mean", sensor)]
        h = df[("rh_mean", sensor)]

        df[("dew", sensor)] = dew_point(t, h)
        df[("abs_hum", sensor)] = absolute_humidity(t, h)
        print(f"Added metrics for sensor '{sensor}': ('dew', '{sensor}'), ('abs_hum', '{sensor}')")

    print("Metric calculation complete.")
    print (f"DataFrame now has columns: {df.columns.tolist()}")
    print(f"DataFrame shape after adding metrics: {df.shape}")
    preview_cols = temp_cols + rh_cols + co2_cols
    print(f"Sample data after adding metrics:\n{df[preview_cols].tail(100)}")
    print(f"Sample stats:\n{df[preview_cols].describe()}")

    return df
