# Superscalar Out-of-Order MIPS Simulator

A cycle-accurate MIPS32 simulator written in C++ that models three execution
modes selectable at runtime:

- `-O0` — single-cycle reference (one instruction per cycle, used to verify correctness for other optimization levels).
- `-O1` — 5-stage in-order pipeline with hazard handling.
- `-O2` — superscalar, out-of-order core with register renaming, a reorder
  buffer, an issue queue, a load/store queue, a non-blocking cache with
  MSHRs, and a BTB-backed branch predictor.

Configurable microarchitectural parameters (pipeline width, ROB / IQ / LSQ
sizes, ALU count, MSHR count, BTB size, physical register file size) live in
[`config.h`](config.h).

## Layout

| Area | Files |
| --- | --- |
| Front-end / dispatch | `processor.{h,cpp}`, `processorOOO.{h,cpp}`, `main.cpp` |
| Rename / ROB / IQ | `prf.h`, `regfile.h`, `reorder_buffer.{h,cpp}`, `instruction_queue.{h,cpp}` |
| Execute | `ALU.h`, `functional_units.h`, `control.h` |
| Memory | `memory.{h,cpp}`, `memory_ooo.{h,cpp}`, `load_store_queue.{h,cpp}`, `non_blocking_cache.{h,cpp}` |
| Branch prediction | `branch_predictor.h` |
| Benchmarks | `test_data_pipeline/` (MIPS asm), `benchmarking/` (sweep + plot scripts) |

## Build

```
make clean && make
```

## Prepare a benchmark

Cross-compile a MIPS32 assembly source into a flat binary the simulator can
load:

```
mipsel-linux-gnu-gcc -mips32 <path-to-benchmark-source> \
    -nostartfiles -Ttext=0 -o <path-to-benchmark-executable>
```

## Run

```
./processor --bmk=<path-to-benchmark-executable> -O<0|1|2> > log
```

The log records the register file at every cycle, the final cycle count, the
elapsed simulated time, and (under `-O2`) branch-prediction stats:

```
CYCLE X
R[0]: 0
R[1]: 0
...
R[31]: 0

Completed execution in 646875 nanoseconds.
BP_TOTAL 1234
BP_MISPREDICTS 56
BP_ACCURACY 95.46%
```

## Test harnesses

- `./run_tests.sh` — runs every directory in `test_data_pipeline/` under
  `-O0` and `-O1`, diffs the final register file, and reports IPC, CPI, and
  branch-prediction accuracy.
- `./run_tests_ooo.sh` — same comparison against the OOO core (`-O2`).
- `./run_single.sh` / `./run_single_ooo.sh` — single-benchmark variants.

## Benchmarking

Scripts in `benchmarking/` automate microarchitectural sweeps and plotting:

- `sweep_pipeline_width.py` — patches `PIPELINE_WIDTH` / `NUM_ALUS`, rebuilds,
  reruns each benchmark in `tests.txt`, and writes per-test CSV/JSON plus a
  combined CSV to `benchmarking/results/`.
- `branch_accuracy.py` — collects `BP_*` stats per benchmark.
- `plot_results.py`, `plot_branch_accuracy.py` — render PNGs into
  `benchmarking/graphs/`.

Generated CSV/JSON/PNG outputs and the vendored MIPS test sources are
excluded from GitHub language stats via `.gitattributes`.
