#include "cpu/cclass/exec_context.hh"

#include "arch/riscv/pcstate.hh"
#include "base/bitfield.hh"

namespace gem5
{

namespace cclass
{

namespace
{

int64_t
signExtend(uint64_t value, int width)
{
    return static_cast<int64_t>(sext(value, width));
}

int64_t
branchImmediate(uint32_t inst)
{
    uint64_t imm = 0;
    imm |= bits(inst, 31) << 12;
    imm |= bits(inst, 7) << 11;
    imm |= bits(inst, 30, 25) << 5;
    imm |= bits(inst, 11, 8) << 1;
    return signExtend(imm, 13);
}

int64_t
jalImmediate(uint32_t inst)
{
    uint64_t imm = 0;
    imm |= bits(inst, 31) << 20;
    imm |= bits(inst, 19, 12) << 12;
    imm |= bits(inst, 20) << 11;
    imm |= bits(inst, 30, 21) << 1;
    return signExtend(imm, 21);
}

int64_t
jalrImmediate(uint32_t inst)
{
    return signExtend(bits(inst, 31, 20), 12);
}

bool
sameFlattenedReg(const StaticInst *producer, unsigned int dest_index,
    const RegId &src, BaseISA *isa)
{
    if (!producer || dest_index >= producer->numDestRegs())
        return false;

    RegId dest = producer->destRegIdx(dest_index);
    if (dest.is(InvalidRegClass))
        return false;

    return dest.flatten(*isa) == src;
}

bool
forwardMatch(const RegId &src, const std::vector<ForwardingSource> &forwards,
    BaseISA *isa, RegVal &value)
{
    for (const auto &forward : forwards) {
        if (forward.valid &&
            sameFlattenedReg(forward.staticInst, forward.destIndex, src, isa))
        {
            value = forward.value;
            return true;
        }
    }

    return false;
}

void
setResolvedTarget(const PCStateBase &pc, Addr addr,
    RiscvControlResolution &resolution)
{
    resolution.target.reset(pc.clone());
    auto &riscv_pc = resolution.target->as<RiscvISA::PCState>();
    riscv_pc.set(addr);
}

void
setSequentialTarget(const PCStateBase &pc, RiscvControlResolution &resolution)
{
    resolution.target.reset(pc.clone());
    resolution.target->advance();
}

} // anonymous namespace

bool
ExecContext::readSourceWithForwarding(const StaticInst *si, int src_idx,
    const std::vector<ForwardingSource> &mem_exe_forwards,
    const std::vector<ForwardingSource> &mem_wb_forwards,
    RegVal &value) const
{
    if (!si || src_idx < 0 || src_idx >= si->numSrcRegs())
        return false;

    auto *isa = thread.getIsaPtr();
    RegId src = si->srcRegIdx(src_idx);

    if (src.is(InvalidRegClass)) {
        value = 0;
        return true;
    }

    src = src.flatten(*isa);

    if (forwardMatch(src, mem_exe_forwards, isa, value))
        return true;

    if (forwardMatch(src, mem_wb_forwards, isa, value))
        return true;

    value = thread.getReg(src);
    return true;
}

bool
ExecContext::resolveRiscvControl(const StaticInst *si,
    const std::vector<ForwardingSource> &mem_exe_forwards,
    const std::vector<ForwardingSource> &mem_wb_forwards,
    RiscvControlResolution &resolution) const
{
    resolution = RiscvControlResolution{};

    if (!si || !si->isControl())
        return false;

    resolution.isControl = true;

    const uint32_t mach_inst = static_cast<uint32_t>(si->getEMI());
    const uint32_t opcode = bits(mach_inst, 6, 0);
    const Addr pc = inst->pc->instAddr();

    switch (opcode) {
      case 0x63: { // Conditional branch.
        RegVal lhs = 0;
        RegVal rhs = 0;
        if (!readSourceWithForwarding(si, 0, mem_exe_forwards,
                mem_wb_forwards, lhs) ||
            !readSourceWithForwarding(si, 1, mem_exe_forwards,
                mem_wb_forwards, rhs))
        {
            return true;
        }

        const uint32_t funct3 = bits(mach_inst, 14, 12);
        switch (funct3) {
          case 0x0: // BEQ
            resolution.taken = lhs == rhs;
            break;
          case 0x1: // BNE
            resolution.taken = lhs != rhs;
            break;
          case 0x4: // BLT
            resolution.taken =
                static_cast<int64_t>(lhs) < static_cast<int64_t>(rhs);
            break;
          case 0x5: // BGE
            resolution.taken =
                static_cast<int64_t>(lhs) >= static_cast<int64_t>(rhs);
            break;
          case 0x6: // BLTU
            resolution.taken = lhs < rhs;
            break;
          case 0x7: // BGEU
            resolution.taken = lhs >= rhs;
            break;
          default:
            return true;
        }

        if (resolution.taken) {
            setResolvedTarget(*inst->pc, pc + branchImmediate(mach_inst),
                resolution);
        } else {
            setSequentialTarget(*inst->pc, resolution);
        }
        resolution.resolved = true;
        return true;
      }

      case 0x6f: // JAL
        resolution.taken = true;
        setResolvedTarget(*inst->pc, pc + jalImmediate(mach_inst), resolution);
        resolution.resolved = true;
        return true;

      case 0x67: { // JALR
        RegVal base = 0;
        if (!readSourceWithForwarding(si, 0, mem_exe_forwards,
                mem_wb_forwards, base))
        {
            return true;
        }

        resolution.taken = true;
        setResolvedTarget(*inst->pc,
            (base + jalrImmediate(mach_inst)) & ~static_cast<Addr>(1),
            resolution);
        resolution.resolved = true;
        return true;
      }

      default:
        return true;
    }
}

} // namespace cclass
} // namespace gem5
