<h1 align="center">读者-写者同步实验</h1>

<p align="center"><a href="README.md">English</a></p>

本项目是北京邮电大学大三操作系统课程的实验之一，使用 C、POSIX线程和信号量实现读者优先与写者优先调度。多个读者和写者线程可以同时运行，程序会记录锁获取等待时间并生成二进制日志，供后续解析和分析。

## 功能

- 读者优先和写者优先两种调度模式
- 可配置的读者线程数和写者线程数
- 基于 POSIX 线程和信号量的同步机制
- 记录锁等待延迟的二进制日志
- 将日志转换为 CSV 的解析器
- Python 分析脚本，生成吞吐、每线程操作数、锁等待分布和活动时间图
- 使用固定随机种子进行可复现的策略对比

## 目录结构

| 路径                      | 说明                       |
| ------------------------- | -------------------------- |
| `src/`                  | 读写模拟器与日志解析器源码 |
| `include/`              | C 头文件和负载参数         |
| `bin/RW`                | 读写模拟器                 |
| `bin/Parser`            | 二进制日志转 CSV 工具      |
| `script/analyze_csv.py` | CSV 分析与图表生成脚本     |
| `script/output/`        | 分析结果和图表             |

## 编译

需要 Linux 环境、GCC、Make 和 POSIX threads。

```bash
make
```

调试构建：

```bash
make debug
```

## 运行

```bash
./bin/RW --reader 5 --writer 5 --fr
./bin/RW --reader 5 --writer 5 --fw
```

调度参数：

| 参数              | 模式     |
| ----------------- | -------- |
| `-fr`, `--fr` | 读者优先 |
| `-fw`, `--fw` | 写者优先 |

读者线程编号从 `0` 开始，写者线程编号从 `100` 开始。按 `Ctrl-C` 停止程序，程序会输出最终读写次数、QPS、最大锁等待时间和 checksum 错误数。

当前负载参数为：请求到达间隔 `[0, 20) ms`，读写业务时间均为`[0, 5) ms`，基础随机种子固定为 `0x5EED1234ABCDEF01`。

## 解析日志

每次运行会在当前目录生成 `YYYY_MM_DD_HHMMSS.log`：

```bash
./bin/Parser YYYY_MM_DD_HHMMSS.log > parsed.csv
```

生成的 CSV 包含序号、时间、数据、checksum 状态、线程编号和锁获取等待时间。

## 分析结果

首次使用时创建 Python 环境：

```bash
python3 -m venv script/.venv
script/.venv/bin/pip install -r script/requirements.txt
```

分析一个或多个 CSV：

```bash
script/.venv/bin/python script/analyze_csv.py \
  rf_5r_5w.csv \
  rf_7r_3w.csv \
  wf_5r_5w.csv \
  wf_3r_7w.csv
```

分析结果写入 `script/output/`，包括：

- `strategy_metrics.csv`
- `thread_metrics.csv`
- `operations_per_thread.png`
- `writer_wait_by_strategy.png`
- `writer_completion_interval.png`
- `writer_wait_ecdf.png`
- `writer_activity_timeline.png`

线程编号小于 `100` 的记录按读者处理，大于等于 `100` 的记录按写者处理。
