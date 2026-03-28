from __future__ import annotations

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import pandas as pd
import numpy as np


def plot_dashboard(
    df: pd.DataFrame,
    roll_window: str = "60min",
    resample: str | None = None,
) -> None:
    plt.rcParams.update({
        "axes.labelsize": 8,
        "xtick.labelsize": 7,
        "ytick.labelsize": 7,
    })

    # ---- CONFIG (single source of truth) ----
    PLOT_CONFIG = [
        ("temp_mean", "Temperature (°C)", "red"),
        ("rh_mean", "Humidity (%RH)", "blue"),
        ("dew", "Dew Point (°C)", "green"),
        ("abs_hum", "Absolute Humidity (g/m³)", "orange"),
        ("pressure_mean", "Pressure (hPa)", "purple"),
        ("lux_mean", "Light (lx)", "brown"),
        ("co2_smooth", "CO₂ (ppm)", "black"),
    ]

    SYS_METRICS = [
        ("heap", "Heap (B)", "black"),
        ("uptime", "Uptime (s)", "gray"),
        ("wifi_rssi", "WiFi RSSI (dBm)", "cyan"),
    ]

    fig = plt.figure(constrained_layout=True, figsize=(14, 9))
    gs = fig.add_gridspec(4, 3)

    # ---- ENV + CO2 ----
    for idx, (unit, label, color) in enumerate(PLOT_CONFIG):
        row, col = divmod(idx, 3)
        ax = fig.add_subplot(gs[row, col])

        # find all sensors that provide this metric
        sensors = [sensor for u, sensor in df.columns if u == unit]

        for sensor in sensors:
            series = df[(unit, sensor)]

            if resample:
                series = series.resample(resample).mean()

            smooth = series.rolling(roll_window).mean() if roll_window else series

            ax.plot(series.index, series.values, color=color, alpha=0.3, lw=1)
            ax.plot(smooth.index, smooth.values, color=color, lw=2, label=sensor)

        ax.set_title(label, fontsize=10, weight="bold")
        ax.set_ylabel(label)
        ax.grid(True, ls="--", alpha=0.5)
        ax.legend(fontsize=7)

        if row == 1:
            ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha="right")

    # ---- SYSTEM ----
    for idx, (unit, label, color) in enumerate(SYS_METRICS):
        ax = fig.add_subplot(gs[row+1, idx])

        # system metrics use single sensor name
        candidates = [col for col in df.columns if col[0] == unit]
        if not candidates:
            continue

        _, sensor = candidates[0]
        series = df[(unit, sensor)]

        if resample:
            series = series.resample(resample).mean()

        y = series.values.astype(float)
        x = series.index

        ax.plot(x, y, color=color, lw=1, alpha=0.4)
        ax.set_title(label)
        ax.grid(True, ls="--", alpha=0.5)

        ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha="right")

    fig.suptitle(
        "Kitchen Lab Sensor + System Dashboard",
        fontsize=12,
        weight="bold",
        y=1.02,
    )

    plt.show()
