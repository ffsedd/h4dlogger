from __future__ import annotations

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates


UNIT_ORDER = ["temp", "hum", "dew", "abs_hum", "pres", "lux"]


def plot(
    df: pd.DataFrame,
    roll_window: str = "60min",
    resample: str | None = None,
) -> None:

    if resample:
        df = df.resample(resample).mean()

    sensors = sorted(df.columns.get_level_values("sensor").unique())

    cmap = plt.get_cmap("tab10")
    colors = {s: cmap(i % 10) for i, s in enumerate(sensors)}

    units = [u for u in UNIT_ORDER if u in df.columns.get_level_values("unit")]

    fig, axes = plt.subplots(len(units), 1, sharex=True, figsize=(10, 3 * len(units)))

    if len(units) == 1:
        axes = [axes]

    for ax, unit in zip(axes, units):

        sub = df[unit]

        for sensor in sub.columns:

            s = sub[sensor].dropna()

            ax.scatter(
                s.index,
                s.values,
                s=2,
                alpha=0.1,
                color=colors[sensor],
            )

            roll = s.rolling(roll_window).mean()

            ax.plot(
                roll.index,
                roll.values,
                color=colors[sensor],
                linewidth=2,
                label=sensor,
            )

        ax.set_ylabel(unit)
        ax.grid(alpha=0.3)
        ax.legend(ncol=2, fontsize=8)

    axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%Y-%m-%d %H:%M"))
    axes[-1].set_xlabel("Time")

    fig.autofmt_xdate()
    plt.tight_layout()
    plt.show()
