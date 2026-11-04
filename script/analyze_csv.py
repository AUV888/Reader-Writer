#!/usr/bin/env python3
"""Analyze reader-writer CSV logs produced by bin/Parser."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


DAY_NS = 86_400_000_000_000
REQUIRED_COLUMNS = {
    "seq",
    "time",
    "checksum_ok",
    "thread_id",
    "latency_ns",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compare reader-writer policy CSV logs. Each input file is treated "
            "as one strategy; use --label when analyzing a single file."
        )
    )
    parser.add_argument("csv_files", nargs="+", type=Path)
    parser.add_argument(
        "--label",
        help="Strategy label for a single CSV file.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "output",
        help="Directory for summary CSVs and charts.",
    )
    parser.add_argument(
        "--writer-thread-min",
        type=int,
        default=100,
        help="Thread IDs at or above this value are writers (default: 100).",
    )
    args = parser.parse_args()

    if args.label and len(args.csv_files) != 1:
        parser.error("--label can only be used with one CSV file")
    if args.writer_thread_min <= 0:
        parser.error("--writer-thread-min must be positive")

    return args


def parse_day_time(time_values: pd.Series) -> np.ndarray:
    """Parse HH:MM:SS.nnnnnnnnn values and unwrap a midnight boundary."""
    parsed = pd.to_timedelta(time_values.astype(str), errors="coerce")
    if parsed.isna().any():
        bad_rows = np.flatnonzero(parsed.isna().to_numpy())[:5]
        raise ValueError(f"Invalid time values at CSV rows: {bad_rows.tolist()}")

    time_ns = parsed.to_numpy().astype("timedelta64[ns]").astype(np.int64)
    if len(time_ns) < 2:
        return time_ns

    midnight_wraps = np.diff(time_ns) < -(DAY_NS // 2)
    offsets = np.concatenate(([0], np.cumsum(midnight_wraps)))
    return time_ns + offsets * DAY_NS


def load_csv(
    path: Path,
    strategy: str,
    writer_thread_min: int,
) -> pd.DataFrame:
    """Load and validate one new-format parser CSV."""
    if not path.is_file():
        raise FileNotFoundError(f"CSV file not found: {path}")

    frame = pd.read_csv(path)
    missing = REQUIRED_COLUMNS.difference(frame.columns)
    if missing:
        missing_text = ", ".join(sorted(missing))
        raise ValueError(
            f"{path}: missing required columns: {missing_text}. "
            "This script only supports CSVs with a latency_ns column."
        )
    if frame.empty:
        raise ValueError(f"{path}: CSV contains no data rows")

    frame = frame.copy()
    for column in ("seq", "checksum_ok", "thread_id", "latency_ns"):
        frame[column] = pd.to_numeric(frame[column], errors="coerce")

    numeric_columns = ["seq", "checksum_ok", "thread_id", "latency_ns"]
    if frame[numeric_columns].isna().any().any():
        raise ValueError(f"{path}: non-numeric value in {numeric_columns}")
    if (frame["latency_ns"] < 0).any():
        raise ValueError(f"{path}: latency_ns contains negative values")

    frame["seq"] = frame["seq"].astype("int64")
    frame["thread_id"] = frame["thread_id"].astype("int64")
    frame["latency_ns"] = frame["latency_ns"].astype("int64")
    frame["checksum_ok"] = frame["checksum_ok"].astype("int64")
    frame["_time_ns"] = parse_day_time(frame["time"])
    frame = frame.sort_values("seq", kind="stable").reset_index(drop=True)

    frame["is_writer"] = frame["thread_id"] >= writer_thread_min
    frame["strategy"] = strategy
    frame["source_file"] = str(path.resolve())
    return frame


def mean_ms(values: pd.Series) -> float:
    return float(values.mean() / 1e6) if not values.empty else float("nan")


def p95_ms(values: pd.Series) -> float:
    return (
        float(values.quantile(0.95) / 1e6)
        if not values.empty
        else float("nan")
    )


def max_ms(values: pd.Series) -> float:
    return float(values.max() / 1e6) if not values.empty else float("nan")


def completion_intervals_ns(
    frame: pd.DataFrame,
    thread_id: int,
) -> pd.Series:
    """Return times between consecutive completed operations for one thread."""
    thread_rows = frame.loc[frame["thread_id"] == thread_id].sort_values("seq")
    intervals = thread_rows["_time_ns"].diff().dropna()
    intervals = intervals[intervals > 0]
    return intervals.astype("int64")


def thread_metric_rows(
    frame: pd.DataFrame,
) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for thread_id, thread_rows in frame.groupby("thread_id", sort=True):
        intervals = completion_intervals_ns(frame, int(thread_id))
        waits = thread_rows["latency_ns"]
        rows.append(
            {
                "strategy": str(frame["strategy"].iloc[0]),
                "role": "writer" if thread_rows["is_writer"].iloc[0] else "reader",
                "thread_id": int(thread_id),
                "operations": int(len(thread_rows)),
                "wait_avg_ms": mean_ms(waits),
                "wait_p95_ms": p95_ms(waits),
                "wait_max_ms": max_ms(waits),
                "completion_interval_avg_ms": mean_ms(intervals),
                "completion_interval_p95_ms": p95_ms(intervals),
                "completion_interval_max_ms": max_ms(intervals),
            }
        )
    return rows


def strategy_metric_row(frame: pd.DataFrame) -> dict[str, object]:
    readers = frame.loc[~frame["is_writer"]]
    writers = frame.loc[frame["is_writer"]]

    interval_groups = [
        completion_intervals_ns(frame, int(thread_id))
        for thread_id in writers["thread_id"].unique()
    ]
    writer_intervals = (
        pd.concat(interval_groups, ignore_index=True)
        if interval_groups
        else pd.Series(dtype="int64")
    )
    duration_s = float(
        (frame["_time_ns"].max() - frame["_time_ns"].min()) / 1e9
    )

    return {
        "strategy": str(frame["strategy"].iloc[0]),
        "source_file": str(frame["source_file"].iloc[0]),
        "operations": int(len(frame)),
        "duration_s": duration_s,
        "readers": int(len(readers)),
        "writers": int(len(writers)),
        "reader_threads": int(readers["thread_id"].nunique()),
        "writer_threads": int(writers["thread_id"].nunique()),
        "reader_operations_per_thread": (
            len(readers) / readers["thread_id"].nunique()
            if not readers.empty
            else float("nan")
        ),
        "writer_operations_per_thread": (
            len(writers) / writers["thread_id"].nunique()
            if not writers.empty
            else float("nan")
        ),
        "reader_ops_per_s": len(readers) / duration_s if duration_s > 0 else 0.0,
        "writer_ops_per_s": len(writers) / duration_s if duration_s > 0 else 0.0,
        "checksum_errors": int((frame["checksum_ok"] != 1).sum()),
        "reader_wait_avg_ms": mean_ms(readers["latency_ns"]),
        "reader_wait_p95_ms": p95_ms(readers["latency_ns"]),
        "reader_wait_max_ms": max_ms(readers["latency_ns"]),
        "writer_wait_avg_ms": mean_ms(writers["latency_ns"]),
        "writer_wait_p95_ms": p95_ms(writers["latency_ns"]),
        "writer_wait_max_ms": max_ms(writers["latency_ns"]),
        "writer_completion_interval_avg_ms": mean_ms(writer_intervals),
        "writer_completion_interval_p95_ms": p95_ms(writer_intervals),
        "writer_completion_interval_max_ms": max_ms(writer_intervals),
    }


def draw_grouped_metric_bars(
    summary: pd.DataFrame,
    metric_columns: list[str],
    labels: list[str],
    title: str,
    ylabel: str,
    output_path: Path,
) -> None:
    strategies = summary["strategy"].tolist()
    x_positions = np.arange(len(strategies))
    width = 0.8 / max(len(metric_columns), 1)

    figure, axis = plt.subplots(figsize=(10, 5.5))
    for index, (column, label) in enumerate(zip(metric_columns, labels)):
        offsets = x_positions + (index - (len(metric_columns) - 1) / 2) * width
        values = summary[column].to_numpy(dtype=float)
        bars = axis.bar(offsets, values, width, label=label)
        axis.bar_label(bars, fmt="%.2f", padding=2, fontsize=8)

    axis.set_title(title)
    axis.set_ylabel(ylabel)
    axis.set_xticks(x_positions, strategies, rotation=15, ha="right")
    axis.grid(axis="y", alpha=0.25)
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=160)
    plt.close(figure)


def draw_writer_wait_ecdf(frames: list[pd.DataFrame], output_path: Path) -> None:
    figure, axis = plt.subplots(figsize=(9, 5.5))
    plotted = False

    for frame in frames:
        values = np.sort(
            frame.loc[frame["is_writer"], "latency_ns"].to_numpy() / 1e6
        )
        if len(values) == 0:
            continue
        probabilities = np.arange(1, len(values) + 1) / len(values)
        axis.step(
            values,
            probabilities,
            where="post",
            label=frame["strategy"].iloc[0],
        )
        plotted = True

    if plotted:
        axis.axhline(0.95, color="black", linestyle="--", label="P95")
        axis.set_xscale("symlog", linthresh=0.001)
        axis.legend()
    else:
        axis.text(0.5, 0.5, "No writer records", ha="center", va="center")

    axis.set_title("Writer Lock-Wait Distribution")
    axis.set_xlabel("Lock wait (ms, symlog scale)")
    axis.set_ylabel("Cumulative fraction")
    axis.grid(alpha=0.25)
    figure.tight_layout()
    figure.savefig(output_path, dpi=160)
    plt.close(figure)


def draw_writer_timelines(frames: list[pd.DataFrame], output_path: Path) -> None:
    figure, axes = plt.subplots(
        len(frames),
        1,
        figsize=(11, max(3.0, 2.5 * len(frames) + 1.0)),
        squeeze=False,
    )

    for axis, frame in zip(axes[:, 0], frames):
        writers = frame.loc[frame["is_writer"]].copy()
        strategy = str(frame["strategy"].iloc[0])
        if writers.empty:
            axis.text(0.5, 0.5, "No writer records", ha="center", va="center")
            axis.set_title(strategy)
            continue

        start_ns = frame["_time_ns"].min()
        writers["elapsed_s"] = (writers["_time_ns"] - start_ns) / 1e9
        for thread_id, thread_rows in writers.groupby("thread_id", sort=True):
            axis.plot(
                thread_rows["elapsed_s"],
                np.full(len(thread_rows), thread_id),
                marker="o",
                linewidth=1,
                markersize=3,
                label=f"Writer {thread_id}",
            )

        axis.set_title(f"{strategy}: Writer Completion Timeline")
        axis.set_xlabel("Elapsed time (s)")
        axis.set_ylabel("Thread ID")
        axis.set_yticks(sorted(writers["thread_id"].unique()))
        axis.grid(alpha=0.25)
        axis.legend(fontsize=8, ncol=3)

    figure.tight_layout()
    figure.savefig(output_path, dpi=160)
    plt.close(figure)


def write_notes(output_path: Path) -> None:
    output_path.write_text(
        "\n".join(
            [
                "Analysis notes",
                "==============",
                "1. latency_ns is lock-acquisition waiting time, not total "
                "operation time.",
                "2. Writer completion interval is the time between consecutive "
                "completed writes from the same thread.",
                "3. The CSV does not contain request arrival events or the "
                "start/end of the protected operation.",
                "4. Therefore, new readers entering during a writer wait and "
                "pure in-lock writer service time cannot be recovered.",
                "5. Long writer inactivity can be inspected through per-thread "
                "completion intervals and the writer activity timeline.",
                "",
            ]
        ),
        encoding="utf-8",
    )


def main() -> None:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    frames: list[pd.DataFrame] = []
    strategies: set[str] = set()
    for path in args.csv_files:
        strategy = args.label or path.stem
        if strategy in strategies:
            raise ValueError(
                f"Duplicate strategy label {strategy!r}; use distinct "
                "filenames or analyze files separately."
            )
        strategies.add(strategy)
        try:
            frames.append(load_csv(path, strategy, args.writer_thread_min))
        except (FileNotFoundError, ValueError) as exc:
            raise SystemExit(f"error: {exc}") from exc

    summary = pd.DataFrame(
        strategy_metric_row(frame) for frame in frames
    ).sort_values("strategy")
    thread_metrics = pd.DataFrame(
        row for frame in frames for row in thread_metric_rows(frame)
    ).sort_values(["strategy", "role", "thread_id"])

    summary.to_csv(output_dir / "strategy_metrics.csv", index=False)
    thread_metrics.to_csv(output_dir / "thread_metrics.csv", index=False)
    write_notes(output_dir / "analysis_notes.txt")

    ordered_frames = [
        frame for strategy in summary["strategy"] for frame in frames
        if frame["strategy"].iloc[0] == strategy
    ]
    draw_grouped_metric_bars(
        summary,
        [
            "writer_wait_avg_ms",
            "writer_wait_p95_ms",
            "writer_wait_max_ms",
        ],
        ["Average", "P95", "Maximum"],
        "Writer Lock-Acquisition Wait by Strategy",
        "Wait time (ms)",
        output_dir / "writer_wait_by_strategy.png",
    )
    draw_grouped_metric_bars(
        summary,
        ["reader_operations_per_thread", "writer_operations_per_thread"],
        ["Readers", "Writers"],
        "Average Operations per Thread",
        "Operations",
        output_dir / "operations_per_thread.png",
    )
    draw_grouped_metric_bars(
        summary,
        [
            "writer_completion_interval_avg_ms",
            "writer_completion_interval_p95_ms",
            "writer_completion_interval_max_ms",
        ],
        ["Average", "P95", "Maximum"],
        "Time Between Consecutive Completed Writes",
        "Completion interval (ms)",
        output_dir / "writer_completion_interval.png",
    )
    draw_writer_wait_ecdf(
        ordered_frames,
        output_dir / "writer_wait_ecdf.png",
    )
    draw_writer_timelines(
        ordered_frames,
        output_dir / "writer_activity_timeline.png",
    )

    display_columns = [
        "strategy",
        "reader_operations_per_thread",
        "writer_operations_per_thread",
        "writer_wait_avg_ms",
        "writer_wait_p95_ms",
        "writer_wait_max_ms",
        "writer_completion_interval_avg_ms",
        "writer_completion_interval_p95_ms",
        "writer_completion_interval_max_ms",
        "checksum_errors",
    ]
    print("Strategy summary:")
    print(summary[display_columns].to_string(index=False, float_format="%.3f"))
    print(f"\nWrote analysis outputs to: {output_dir}")


if __name__ == "__main__":
    main()
