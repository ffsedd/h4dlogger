from __future__ import annotations
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import pandas as pd
import numpy as np

def plot_dashboard(df: pd.DataFrame, roll_window: str = "60min", resample: str | None = None) -> None:
    """
    Pretty dashboard plot for environmental sensor data + system metrics.
    
    Args:
        df: DataFrame with MultiIndex columns (unit, sensor)
        roll_window: Rolling mean window (e.g., '60min')
        resample: Optional resampling frequency (e.g., '1T')
    """
    # Environmental metrics
    env_metrics = [
        ("temp_mean", "Temperature (°C)"),
        ("rh_mean", "Humidity (%RH)"),
        ("dew", "Dew Point (°C)"),
        ("abs_hum", "Absolute Humidity (g/m³)"),
        ("pressure_mean", "Pressure (hPa)"),
        ("lux_mean", "Light (lx)")
    ]

    # System metrics: no sensor differentiation
    sys_metrics = [
        ("heap", "Heap (B)"),
        ("uptime", "Uptime (s)"),
        ("wifi_rssi", "WiFi RSSI (dBm)")
    ]

    sensors = sorted({sensor for unit, sensor in df.columns if unit in [m[0] for m in env_metrics]})
    colors = plt.cm.tab10.colors

    fig = plt.figure(constrained_layout=True, figsize=(18, 10))
    gs = fig.add_gridspec(3, 4)  # 2×3 env + 1×4 sys

    # ----------------- Environmental plots -----------------
    for idx, (unit, label) in enumerate(env_metrics):
        row = idx // 3
        col = idx % 3
        ax = fig.add_subplot(gs[row, col])

        for i, sensor in enumerate(sensors):
            if (unit, sensor) not in df.columns:
                continue

            series = df[(unit, sensor)]
            if resample:
                series = series.resample(resample).mean()
            if roll_window:
                series_smooth = series.rolling(roll_window).mean()
            else:
                series_smooth = series

            ax.plot(series.index, series.values, color=colors[i % len(colors)], alpha=0.3, lw=1)
            ax.plot(series_smooth.index, series_smooth.values, color=colors[i % len(colors)], lw=2, label=sensor)
            ax.fill_between(series.index,
                            series_smooth.values - 0.5,
                            series_smooth.values + 0.5,
                            color=colors[i % len(colors)], alpha=0.1)

        ax.set_title(label, fontsize=12, weight='bold')
        ax.grid(True, ls='--', alpha=0.5)
        ax.legend(fontsize=9)
        ax.set_ylabel(label)
        if row == 1:
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha='right')

    # ----------------- System metric plots -----------------
    for idx, (unit, label) in enumerate(sys_metrics):
        ax = fig.add_subplot(gs[2, idx])
        if (unit, "kit_lab/system") not in df.columns:
            continue

        series = df[(unit, "kit_lab/system")]
        if resample:
            series = series.resample(resample).mean()

        ax.plot(series.index, series.values, color='tab:purple', lw=2)
        ax.fill_between(series.index,
                        series.values - 0.5,
                        series.values + 0.5,
                        color='tab:purple', alpha=0.1)
        ax.set_title(label, fontsize=11)
        ax.grid(True, ls='--', alpha=0.5)
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha='right')

    fig.suptitle("Kitchen Lab Sensor + System Dashboard", fontsize=16, weight='bold', y=1.02)
    plt.show()
