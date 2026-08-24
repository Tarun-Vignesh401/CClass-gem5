#!/usr/bin/env python3
"""
Run the current C-Class pcgen-only CPU model.

This script intentionally stays small and only builds enough of a system to
let the top-level C-Class CPU object instantiate and tick.  It assumes the
CPU implementation from CClass-gem5/src/cpu has been copied or mounted into
the gem5 source tree and compiled into the gem5 binary.
"""

import argparse
from pathlib import Path
import sys

import m5
from m5.objects import (
    AddrRange,
    Process,
    Root,
    SEWorkload,
    SimpleMemory,
    SrcClockDomain,
    System,
    SystemXBar,
    VoltageDomain,
)


THIS_FILE = Path(__file__).resolve()
CCLASS_CPU_DIR = THIS_FILE.parents[1] / "cpu"


def get_cclass_cpu_class():
    """Resolve the CPU using the names currently present in this codebase."""

    # Prefer the local top-level alias file in CClass-gem5/src/cpu/CClass.py.
    sys.path.insert(0, str(CCLASS_CPU_DIR))
    try:
        from CClass import CClassCPU

        return CClassCPU
    except Exception as exc:
        cclass_alias_error = exc

    # If the SimObject was compiled directly as m5.objects.CClassCPU.
    try:
        from m5.objects import CClassCPU

        return CClassCPU
    except Exception as exc:
        direct_error = exc

    # If the ISA wrapper generated RiscvCClassCPU under RiscvCPU.py.
    try:
        from m5.objects.RiscvCPU import RiscvCClassCPU

        return RiscvCClassCPU
    except Exception as exc:
        riscv_error = exc

    raise ImportError(
        "Could not resolve the C-Class CPU class. Tried local CClass.CClassCPU, "
        "m5.objects.CClassCPU, and m5.objects.RiscvCPU.RiscvCClassCPU.\n"
        f"local CClass error: {cclass_alias_error}\n"
        f"direct SimObject error: {direct_error}\n"
        f"RISC-V wrapper error: {riscv_error}"
    )


def build_system(args):
    system = System()
    system.clk_domain = SrcClockDomain(
        clock=args.clk_freq,
        voltage_domain=VoltageDomain(),
    )
    system.mem_mode = "timing"
    system.mem_ranges = [AddrRange(args.mem_size)]

    CClassCPU = get_cclass_cpu_class()
    system.cpu = CClassCPU(cpu_id=0)

    system.membus = SystemXBar()
    system.system_port = system.membus.cpu_side_ports

    # The current pcgen-only model exposes instruction/data CPU ports through
    # BaseCPU.  Data is connected too so BaseCPU port lookup does not fail even
    # though the current stage under test is fetch/pc generation.
    system.cpu.icache_port = system.membus.cpu_side_ports
    system.cpu.dcache_port = system.membus.cpu_side_ports

    system.memory = SimpleMemory(range=system.mem_ranges[0])
    system.memory.port = system.membus.mem_side_ports

    if args.binary:
        binary = str(Path(args.binary).resolve())
        system.workload = SEWorkload.init_compatible(binary)
        process = Process()
        process.cmd = [binary]
        system.cpu.workload = process
    else:
        # This lets the object instantiate for pcgen-only bring-up even before
        # a real fetch/decode/execute path can run a program.
        system.workload = SEWorkload()

    system.cpu.createThreads()
    return system


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--binary",
        default="",
        help="Optional RISC-V SE binary. Omit for instantiate/tick bring-up.",
    )
    parser.add_argument("--clk-freq", default="1GHz")
    parser.add_argument("--mem-size", default="512MiB")
    parser.add_argument(
        "--max-tick",
        type=int,
        default=1000,
        help="Simulation tick limit for pcgen bring-up.",
    )
    args = parser.parse_args()

    root = Root(full_system=False, system=build_system(args))
    m5.instantiate()

    print("Beginning C-Class pcgen-only simulation")
    exit_event = m5.simulate(args.max_tick)
    print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")


if __name__ == "__m5_main__":
    main()
