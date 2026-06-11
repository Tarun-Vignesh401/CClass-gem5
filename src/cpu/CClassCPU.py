
import m5.defines

arch_vars = [
    "USE_ARM_ISA",
    "USE_RISCV_ISA",
    "USE_X86_ISA",
]

enabled = list(filter(lambda var: m5.defines.buildEnv[var], arch_vars))

if len(enabled) == 1:
    arch = enabled[0]
    if arch == "USE_ARM_ISA":
        from m5.objects.ArmCPU import ArmCClassCPU as CClassCPU
    if arch == "USE_RISCV_ISA":
        try:
            from m5.objects.RiscvCPU import RiscvCClassCPU as CClassCPU
        except ImportError:
            from m5.objects import BaseCClassCPU as CClassCPU
