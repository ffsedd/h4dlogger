from __future__ import annotations

from pathlib import Path

import pandas as pd

from h4dlogger.parser import SensorLog


def write_log(tmp_path: Path, text: str) -> Path:
    p = tmp_path / "log.txt"
    p.write_text(text)
    return p


def test_rows_valid_line(tmp_path: Path) -> None:
    log = write_log(
        tmp_path,
        "1700000000 sensors/dev1/temp/temp_mean 21.5\n",
    )

    sl = SensorLog(log)

    rows = list(sl.rows())

    assert rows == [(1700000000, "dev1/temp", "temp_mean", 21.5)]


def test_rows_multiple_lines(tmp_path: Path) -> None:
    log = write_log(
        tmp_path,
        "\n".join(
            [
                "1700000000 sensors/dev1/temp/temp_mean 21.5",
                "1700000001 sensors/dev1/rh/rh_mean 55",
            ]
        ),
    )

    rows = list(SensorLog(log).rows())

    assert len(rows) == 2
    assert rows[0][1] == "dev1/temp"
    assert rows[1][1] == "dev1/rh"


def test_rows_invalid_lines_skipped(tmp_path: Path) -> None:
    log = write_log(
        tmp_path,
        "\n".join(
            [
                "bad line",
                "notint sensors/dev1/temp/temp_mean 20",
                "1700000000 sensors/dev1/temp 21",  # wrong topic length
                "1700000000 sensors/dev1/temp/temp_mean badfloat",
            ]
        ),
    )

    rows = list(SensorLog(log).rows())

    assert rows == []


def test_parse_dataframe_structure(tmp_path: Path) -> None:
    log = write_log(
        tmp_path,
        "1700000000 sensors/dev1/temp/temp_mean 21.5\n",
    )

    df = SensorLog(log).parse()

    assert list(df.columns) == ["ts", "sensor", "unit", "value"]
    assert df.iloc[0]["sensor"] == "dev1/temp"
    assert df.iloc[0]["unit"] == "temp_mean"
    assert df.iloc[0]["value"] == 21.5


def test_parse_timestamp_conversion(tmp_path: Path) -> None:
    log = write_log(
        tmp_path,
        "1700000000 sensors/dev1/temp/temp_mean 21.5\n",
    )

    df = SensorLog(log).parse()

    assert pd.api.types.is_datetime64_any_dtype(df["ts"])
    assert df["ts"].dt.tz is not None


def test_parse_empty_file(tmp_path: Path) -> None:
    log = write_log(tmp_path, "")

    df = SensorLog(log).parse()

    assert df.empty