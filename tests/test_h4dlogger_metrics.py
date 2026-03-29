from __future__ import annotations

import numpy as np
import pandas as pd
import pytest

from h4dlogger.metrics import absolute_humidity, add_abs_humidity, dew_point


def test_dew_point_known_value() -> None:
    temp = pd.Series([20.0])
    rh = pd.Series([50.0])

    dp = dew_point(temp, rh)

    # expected value from Magnus formula (~9.3 °C)
    assert dp.iloc[0] == pytest.approx(9.26, abs=0.2)


def test_absolute_humidity_known_value() -> None:
    temp = pd.Series([20.0])
    rh = pd.Series([50.0])

    ah = absolute_humidity(temp, rh)

    # expected ~8.6 g/m³
    assert ah.iloc[0] == pytest.approx(8.6, abs=0.3)


def make_test_df() -> pd.DataFrame:
    index = pd.RangeIndex(3)

    cols = pd.MultiIndex.from_tuples(
        [
            ("temp_mean", "s1"),
            ("rh_mean", "s1"),
            ("temp_mean", "s2"),
            ("rh_mean", "s2"),
        ]
    )

    data = np.array(
        [
            [20.0, 50.0, 22.0, 60.0],
            [21.0, 55.0, 23.0, 65.0],
            [19.0, 45.0, 24.0, 70.0],
        ]
    )

    return pd.DataFrame(data, columns=cols, index=index)


def test_add_metrics_adds_columns() -> None:
    df = make_test_df()

    out = add_abs_humidity(df.copy())

    assert ("dew", "s1") in out.columns
    assert ("abs_hum", "s1") in out.columns
    assert ("dew", "s2") in out.columns
    assert ("abs_hum", "s2") in out.columns


def test_add_metrics_values_match_functions() -> None:
    df = make_test_df()
    out = add_abs_humidity(df.copy())

    expected_dew = dew_point(df[("temp_mean", "s1")], df[("rh_mean", "s1")])
    expected_ah = absolute_humidity(df[("temp_mean", "s1")], df[("rh_mean", "s1")])

    assert np.allclose(out[("dew", "s1")], expected_dew)
    assert np.allclose(out[("abs_hum", "s1")], expected_ah)


def test_add_metrics_missing_sensor_pair() -> None:
    cols = pd.MultiIndex.from_tuples(
        [
            ("temp_mean", "s1"),
            ("rh_mean", "s2"),  # mismatched sensor
        ]
    )

    df = pd.DataFrame([[20, 50]], columns=cols)

    out = add_abs_humidity(df.copy())

    assert ("dew", "s1") not in out.columns
    assert ("dew", "s2") not in out.columns


def test_add_metrics_non_multiindex_returns_original() -> None:
    df = pd.DataFrame(
        {
            "temp_mean": [20, 21],
            "rh_mean": [50, 55],
        }
    )

    out = add_abs_humidity(df.copy())

    assert list(out.columns) == list(df.columns)
