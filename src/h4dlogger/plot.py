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
    
    plt.rcParams.update({'axes.labelsize': 8, 'xtick.labelsize': 7, 'ytick.labelsize': 7})
    
    # Environmental metrics
    env_metrics = [
        ("temp_mean", "Temperature (°C)", "red"), 
        ("rh_mean", "Humidity (%RH)", "blue"),
        ("dew", "Dew Point (°C)", "green"),
        ("abs_hum", "Absolute Humidity (g/m³)", "orange"),
        ("pressure_mean", "Pressure (hPa)", "purple"),
        ("lux_mean", "Light (lx)", "brown")
    ]

    # System metrics: no sensor differentiation
    sys_metrics = [
        ("heap", "Heap (B)", "black"),
        ("uptime", "Uptime (s)", "gray"),
        ("wifi_rssi", "WiFi RSSI (dBm)", "cyan")
    ]

    sensors = sorted({sensor for unit, sensor in df.columns if unit in [m[0] for m in env_metrics]})
    

    fig = plt.figure(constrained_layout=True, figsize=(12, 8))
    gs = fig.add_gridspec(3, 3)  # 2×3 env + 1×4 sys

    # ----------------- Environmental plots -----------------
    for idx, (unit, label, color) in enumerate(env_metrics):
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

            x = series.index
            y = np.array(series.values).astype(float)
            ys = np.array(series_smooth.values).astype(float)

            ax.plot(x, y, color=color, alpha=0.3, lw=1)
            ax.plot(x, ys, color=color, lw=2, label=sensor)
            ax.fill_between(x,
                            ys - 0.5,
                            ys + 0.5,
                            color=color, alpha=0.1)

        ax.set_title(label, fontsize=10, weight='bold')
        ax.grid(True, ls='--', alpha=0.5)
        ax.legend(fontsize=8)
        ax.set_ylabel(label)
        if row == 1:
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha='right')

    # ----------------- System metric plots -----------------
    for idx, (unit, label, color) in enumerate(sys_metrics):
        ax = fig.add_subplot(gs[2, idx])
        if (unit, "kit_lab/system") not in df.columns:
            continue

        series = df[(unit, "kit_lab/system")]
        if resample:
            series = series.resample(resample).mean()
        if isinstance(series.dtype, pd.api.types.CategoricalDtype) or series.nunique() < 5:
            continue
        x = series.index
        y = np.array(series.values).astype(float)
    
        ax.plot(x, y, color=color, lw=1, alpha=.4)
        ax.fill_between(x,
                        y - 0.5,
                        y + 0.5,
                        color='tab:grey', alpha=0.1)
        ax.set_title(label, fontsize=10)
        ax.grid(True, ls='--', alpha=0.5)
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha='right')

    fig.suptitle("Kitchen Lab Sensor + System Dashboard", fontsize=12, weight='bold', y=1.02)
    plt.show()
