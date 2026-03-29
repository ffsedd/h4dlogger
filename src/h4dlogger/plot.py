from __future__ import annotations

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import pandas as pd


def plot_dashboard(
    df: pd.DataFrame,
    roll_window: str = "60min",
    resample: str | None = None,
) -> None:
    plt.rcParams.update(
        {
            "axes.labelsize": 8,
            "xtick.labelsize": 7,
            "ytick.labelsize": 7,
        }
    )

    PLOT_CONFIG = [
        ("temp_mean", "Temperature (°C)", "red"),
        ("rh_mean", "Humidity (%RH)", "blue"),
        ("dew", "Dew Point (°C)", "green"),
        ("abs_hum", "Absolute Humidity (g/m³)", "orange"),
        ("pressure_mean", "Pressure (hPa)", "purple"),
        ("lux_mean", "Light (lx)", "brown"),
        ("co2_smooth", "CO₂ (ppm)", "black"),
        ("motion_fraction", "Motion", "pink"),
    ]

    SYS_METRICS = [
        ("heap", "Heap (B)", "black"),
        ("uptime", "Uptime (s)", "gray"),
        ("wifi_rssi", "WiFi RSSI (dBm)", "cyan"),
    ]
    print(f"Plotting dashboard with roll_window={roll_window} and resample={resample}")
    print(f"DataFrame columns: {df.columns}")
    fig = plt.figure(constrained_layout=True, figsize=(14, 9))
    gs = fig.add_gridspec(4, 3)

    # ---- ENV + CO2 ----
    for idx, (unit, label, color) in enumerate(PLOT_CONFIG):
        row, col = divmod(idx, 3)
        ax = fig.add_subplot(gs[row, col])

        # find all sensors for this metric
        sensors = [sensor for u, sensor in df.columns if u == unit]

        for sensor in sensors:
            series = df[(unit, sensor)]
            if resample:
                series = series.resample(resample).mean()
            smooth = series.rolling(roll_window).mean() if roll_window else series

            ax.plot(series.index, series.to_numpy(), color=color, alpha=0.3, lw=1)
            ax.plot(smooth.index, smooth.to_numpy(), color=color, lw=2, label=sensor)

            # ---- check for gradient ----
            base_unit = unit.rsplit("_", 1)[0]  # e.g. "temp" from "temp_mean"
            grad_col = (f"{base_unit}_grad", sensor)
            # print(f"Checking for gradient column: {grad_col} in df.columns: {df.columns}")
            if grad_col in df.columns:
                grad_series = df[grad_col]
                if resample:
                    grad_series = grad_series.resample(resample).mean()
                grad_smooth = (
                    grad_series.rolling(roll_window).mean() if roll_window else grad_series
                )

                ax2 = ax.twinx()
                ax2.plot(
                    grad_series.index,
                    grad_smooth.to_numpy(),
                    color="gray",
                    lw=1.5,
                    linestyle="--",
                    label=f"{sensor} grad",
                )
                ax2.set_ylabel(f"{label} grad", color="gray", fontsize=7)
                ax2.tick_params(axis="y", labelcolor="gray", labelsize=7)

        ax.set_title(label, fontsize=10, weight="bold")
        ax.set_ylabel(label)
        ax.grid(True, ls="--", alpha=0.5)
        ax.legend(fontsize=7)

        if row == 1:
            ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=30, ha="right")

    # ---- SYSTEM ----
    for idx, (unit, label, color) in enumerate(SYS_METRICS):
        ax = fig.add_subplot(gs[row + 1, idx])
        candidates = [col for col in df.columns if col[0] == unit]
        if not candidates:
            continue

        _, sensor = candidates[0]
        series = df[(unit, sensor)]
        if resample:
            series = series.resample(resample).mean()

        ax.plot(series.index, series.astype(float).to_numpy(), color=color, lw=1, alpha=0.4)
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
