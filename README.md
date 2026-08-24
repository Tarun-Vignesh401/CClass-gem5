# CClass gem5

This repository contains the CClass in-order CPU work based on gem5.

## Current status

- Fetch, decode, and execute-stage work is present.
- Execute-stage debug output should be visible after the run in the /m5out directory that will get created.
- The memory and writeback stages are not implemented yet.

## Running

Run the supplied script from the repository root:

```bash
./script.sh
```

The script runs gem5 with this RISC-V test program:

```text
gem5/tests/test-progs/hello/bin/riscv/linux/hello
```

## Configuration

The simulation configuration script is:

```text
configs/learning_gem5/part1/simple-riscv.py
```
