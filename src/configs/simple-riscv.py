#!/usr/bin/env python3

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


def cclass_cpu():
    cpu_dir = Path(__file__).resolve().parents[1] / "cpu"
    sys.path.insert(0, str(cpu_dir))

    candidates = (
        lambda: __import__("m5.objects.RiscvCPU", fromlist=["RiscvCClassCPU"]).RiscvCClassCPU,
        lambda: __import__("m5.objects", fromlist=["RiscvCClassCPU"]).RiscvCClassCPU,
        lambda: __import__("m5.objects", fromlist=["CClassCPU"]).CClassCPU,
        lambda: __import__("m5.objects", fromlist=["BaseCClassCPU"]).BaseCClassCPU,
    )

    errors = []
    for get_cls in candidates:
        try:
            return get_cls()(cpu_id=0)
        except Exception as exc:
            errors.append(str(exc))

    raise RuntimeError("Could not import the C-Class CPU:\n" + "\n".join(errors))


system = System()
system.clk_domain = SrcClockDomain(
    clock="1GHz",
    voltage_domain=VoltageDomain(),
)
system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MiB")]
system.cpu = cclass_cpu()

system.membus = SystemXBar()
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports
system.system_port = system.membus.cpu_side_ports

try:
    system.cpu.createInterruptController()
except Exception:
    pass

system.memory = SimpleMemory(range=system.mem_ranges[0])
system.memory.port = system.membus.mem_side_ports

binary = Path(__file__).resolve().parents[3] / "tests/test-progs/hello/bin/riscv/linux/hello"
if binary.exists():
    system.workload = SEWorkload.init_compatible(str(binary))
    process = Process()
    process.cmd = [str(binary)]
    system.cpu.workload = process
else:
    system.workload = SEWorkload()

system.cpu.createThreads()

root = Root(full_system=False, system=system)
m5.instantiate()

print("Beginning C-Class simple RISC-V simulation")
exit_event = m5.simulate(1000)
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
