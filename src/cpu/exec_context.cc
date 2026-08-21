#include "cpu/cclass/exec_context.hh"

#include "arch/riscv/pcstate.hh"
#include "base/bitfield.hh"
#include "cpu/cclass/execute.hh"

#include <utility>

namespace gem5
{

namespace cclass
{
Fault
ExecContext::initiateMemRead(Addr addr, unsigned int size,
    Request::Flags flags, const std::vector<bool> &byte_enable)
{
    return execute.initiateMemRead(inst, addr, size, flags, byte_enable);
}

Fault
ExecContext::writeMem(uint8_t *data, unsigned int size, Addr addr,
    Request::Flags flags, uint64_t *res,
    const std::vector<bool> &byte_enable)
{
    return execute.writeMem(inst, data, size, addr, flags, res, byte_enable);
}

Fault
ExecContext::initiateMemAMO(Addr addr, unsigned int size,
    Request::Flags flags, AtomicOpFunctorPtr amo_op)
{
    return execute.initiateMemAMO(inst, addr, size, flags, std::move(amo_op));
}

} // namespace cclass
} // namespace gem5
