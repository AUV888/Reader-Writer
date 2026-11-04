# CSV analysis

The analysis script only accepts parser CSVs that contain `latency_ns`.
Each input file is treated as one strategy, and its filename is used as the
strategy label. For a single experiment, use `--label` to choose a clearer
name.

## Environment

The local WSL environment uses an isolated virtual environment:

```bash
$HOME/.local/bin/uv venv script/.venv --python /usr/bin/python3
$HOME/.local/bin/uv pip install \
  --python script/.venv/bin/python \
  -r script/requirements.txt
```

## Run

Analyze one policy:

```bash
script/.venv/bin/python script/analyze_csv.py \
  278new.csv --label reader_priority
```

Compare multiple policies:

```bash
script/.venv/bin/python script/analyze_csv.py \
  reader_priority.csv writer_priority.csv fair.csv
```

Outputs are written to `script/output/` by default:

- `strategy_metrics.csv`
- `thread_metrics.csv`
- one chart per requested metric group
- `analysis_notes.txt`

Thread IDs below `100` are readers and IDs at or above `100` are writers.
Override this with `--writer-thread-min`.
