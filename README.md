<h1 align="center">Reader-Writer Synchronization Lab</h1>

<p align="center"><a href="README_ZH.md">中文说明</a></p>

This project is part of the Operating Systems laboratory course in the junior year at Beijing University of Posts and Telecommunications. It is a C implementation of reader-first and writer-first scheduling using POSIX threads and semaphores. Multiple reader and writer threads run concurrently, record lock-acquisition latency, and write binary logs for later analysis.

## Features

- Reader-first and writer-first scheduling modes
- Configurable numbers of reader and writer threads
- POSIX thread and semaphore synchronization
- Binary operation logs with lock-wait latency
- CSV parser for offline analysis
- Python analyzer for throughput, per-thread operations, lock-wait
  distributions, and activity timelines
- Fixed random seed for reproducible policy comparisons

## Repository layout

| Path                      | Description                                   |
| ------------------------- | --------------------------------------------- |
| `src/`                  | Reader-writer simulator and log parser source |
| `include/`              | C headers and workload constants              |
| `bin/RW`                | Reader-writer simulator                       |
| `bin/Parser`            | Binary log to CSV converter                   |
| `script/analyze_csv.py` | CSV analysis and chart generation             |
| `script/output/`        | Generated analysis results and charts         |

## Build

The build requires a Linux environment with GCC, Make, and POSIX threads.

```bash
make
```

For a debug build:

```bash
make debug
```

## Run

```bash
./bin/RW --reader 5 --writer 5 --fr
./bin/RW --reader 5 --writer 5 --fw
```

Available scheduling flags:

| Flag              | Mode         |
| ----------------- | ------------ |
| `-fr`, `--fr` | Reader-first |
| `-fw`, `--fw` | Writer-first |

Reader thread IDs start at `0`; writer thread IDs start at `100`. Press  `Ctrl-C` to stop the process. The simulator prints final operation counts, QPS, maximum lock latency, and checksum errors.

The current workload uses a random arrival interval of `[0, 20) ms`, `[0, 5) ms` of business time for both roles, and a fixed base seed of  `0x5EED1234ABCDEF01`.

## Parse logs

Each run creates a binary log named `YYYY_MM_DD_HHMMSS.log` in the current directory. Convert a log with:

```bash
./bin/Parser YYYY_MM_DD_HHMMSS.log > parsed.csv
```

The CSV contains sequence, time, payload, checksum status, thread ID, and lock-acquisition latency.

## Analyze results

Create the Python environment once:

```bash
python3 -m venv script/.venv
script/.venv/bin/pip install -r script/requirements.txt
```

Analyze one or more parser CSV files:

```bash
script/.venv/bin/python script/analyze_csv.py \
  rf_5r_5w.csv \
  rf_7r_3w.csv \
  wf_5r_5w.csv \
  wf_3r_7w.csv
```

The analyzer writes results to `script/output/`:

- `strategy_metrics.csv`
- `thread_metrics.csv`
- `operations_per_thread.png`
- `writer_wait_by_strategy.png`
- `writer_completion_interval.png`
- `writer_wait_ecdf.png`
- `writer_activity_timeline.png`

Thread IDs below `100` are treated as readers and IDs at or above `100` as writers.
