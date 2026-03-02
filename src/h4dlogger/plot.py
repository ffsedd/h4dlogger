from __future__ import annotations
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import pandas as pd
import numpy as np

def plot_dashboard(df: pd.DataFrame, roll_window: str = "60min", resample: str | None = None) -> None:
    """
    Pretty dashboard plot for environmental sensor data.
    
    Args:
        df: DataFrame with MultiIndex columns (unit, sensor)
        roll_window: Rolling mean window (e.g., '60min')
        resample: Optional resampling frequency (e.g., '1T')
    """
    # Define units and their labels
    metrics = [
        ("temp_mean", "Temperature (°C)"),
        ("rh_mean", "Humidity (%RH)"),
        ("dew", "Dew Point (°C)"),
        ("abs_hum", "Absolute Humidity (g/m³)"),
        ("pressure_mean", "Pressure (hPa)"),
        ("lux_mean", "Light (lx)")
    ]

    sensors = sorted({sensor for unit, sensor in df.columns if unit in [m[0] for m in metrics]})
    colors = plt.cm.tab10.colors

    fig, axes = plt.subplots(2, 3, figsize=(18, 8), sharex=True)
    axes = axes.flatten()

    for ax, (unit, label) in zip(axes, metrics):
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

            # Plot raw values lightly
            ax.plot(series.index, series.values, color=colors[i % len(colors)], alpha=0.3, lw=1)
            # Plot rolling mean prominently
            ax.plot(series_smooth.index, series_smooth.values, color=colors[i % len(colors)], lw=2, label=sensor)
            # Optional: shaded band for rolling window variability
            ax.fill_between(series.index, series_smooth.values - 0.5, series_smooth.values + 0.5,
                            color=colors[i % len(colors)], alpha=0.1)

        ax.set_title(label, fontsize=12, weight='bold')
        ax.grid(True, which='both', ls='--', alpha=0.5)
        ax.legend(fontsize=9)
        ax.set_ylabel(label)

    # Format X-axis
    for ax in axes[-3:]:
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha='right')

    fig.tight_layout()
    fig.suptitle("Kitchen Lab Sensor Dashboard", fontsize=16, weight='bold', y=1.02)
    plt.show()