#include "cpu/cclass/exec_context.hh"

#include "arch/riscv/pcstate.hh"
#include "base/bitfield.hh"
#include "cpu/cclass/execute.hh"
#include "cpu/cclass/pipe_data.hh"

#include <cassert>
#include <cstring>
#include <utility>

namespace gem5
{

namespace cclass
{
Fault
ExecContext::initiateMemRead(Addr addr, unsigned int size,
    Request::Flags flags, const std::vector<bool> &byte_enable)
{   
    assert(execute != nullptr);
    return execute->initiateMemRead(inst, addr, size, flags, byte_enable);
}

Fault
ExecContext::writeMem(uint8_t *data, unsigned int size, Addr addr,
    Request::Flags flags, uint64_t *res,
    const std::vector<bool> &byte_enable)
{   
    assert(execute != nullptr);
    return execute->writeMem(inst, data, size, addr, flags, res, byte_enable);
}

Fault
ExecContext::initiateMemAMO(Addr addr, unsigned int size,
    Request::Flags flags, AtomicOpFunctorPtr amo_op)
{
    assert(execute != nullptr);
    return execute->initiateMemAMO(inst, addr, size, flags, std::move(amo_op));
}

RegVal
ExecContext::getRegOperand(const StaticInst *si, int idx)
{
    assert(execute != nullptr);
    const RegId &reg = si->srcRegIdx(idx);

    if (reg.is(InvalidRegClass))
        return 0;

    if (reg.is(IntRegClass)) {
        RegVal forwarded_value = 0;

        if (execute->lookForForwards(inst->id.threadId, reg, forwarded_value))
            return forwarded_value;
    }

    return thread.getReg(reg);
}

/* filler into void pointer vector/fp instructions*/
void
ExecContext::getRegOperand(const StaticInst *si, int idx, void *val)
{
    const RegId &reg = si->srcRegIdx(idx);

    if (reg.is(InvalidRegClass))
        return;

    thread.getReg(reg, val);
}

void *
ExecContext::getWritableRegOperand(const StaticInst *si, int idx)
{
    const RegId &reg = si->destRegIdx(idx);

    if (reg.is(InvalidRegClass))
        return nullptr;
    /* result can't be empty*/
    assert(result);
    result->writes.emplace_back(reg, reg.regClass().regBytes(), true);
    return result->writes.back().bytes.data();
}

void
ExecContext::setRegOperand(const StaticInst *si, int idx, RegVal val)
{
    const RegId &reg = si->destRegIdx(idx);

    if (reg.is(InvalidRegClass))
        return;
    /* result object can't be empty*/
    assert(result);
    result->writes.push_back(RegWrite{reg, val});
}

void
ExecContext::setRegOperand(const StaticInst *si, int idx, const void *val)
{
    const RegId &reg = si->destRegIdx(idx);

    if (reg.is(InvalidRegClass))
        return;

    assert(result);
    result->writes.emplace_back(reg, reg.regClass().regBytes(), true);
    std::memcpy(result->writes.back().bytes.data(), val,
        result->writes.back().bytes.size());
}

RegVal
ExecContext::readMiscRegOperand(const StaticInst *si, int idx)
{
    const RegId &reg = si->srcRegIdx(idx);
    assert(reg.is(MiscRegClass));
    return thread.readMiscReg(reg.index());
}

void
ExecContext::setMiscRegOperand(const StaticInst *si, int idx, RegVal val)
{
    const RegId &reg = si->destRegIdx(idx);
    assert(reg.is(MiscRegClass));
    /* result object can't be empty*/
    assert(result);
    result->miscWrites.push_back({reg.index(), val});
}

void
ExecContext::setMiscReg(int misc_reg, RegVal val)
{
    /* result object can't be empty*/
    assert(result);
    result->miscWrites.push_back({misc_reg, val});
}

} // namespace cclass
} // namespace gem5
