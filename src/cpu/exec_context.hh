/**
 * @file
 *
 * ExecContext interface used by the CClass pipeline.
 */

#ifndef __CPU_CCLASS_EXEC_CONTEXT_HH__
#define __CPU_CCLASS_EXEC_CONTEXT_HH__

#include <memory>
#include <vector>

#include "base/logging.hh"
#include "base/types.hh"
#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/dyn_inst.hh"
#include "cpu/exec_context.hh"
#include "cpu/simple_thread.hh"
#include "cpu/static_inst.hh"
#include "mem/packet.hh"
#include "mem/request.hh"

namespace gem5
{

namespace cclass
{

class Execute;

struct ForwardingSource
{
    bool valid = false;
    const StaticInst *staticInst = nullptr;
    unsigned int destIndex = 0;
    RegVal value = 0;
};

struct RiscvControlResolution
{
    bool isControl = false;
    bool resolved = false;
    bool taken = false;
    std::unique_ptr<PCStateBase> target;
};

class ExecContext : public gem5::ExecContext
{
  public:
    CClassCPU &cpu;
    SimpleThread &thread;
    Execute &execute;
    CClassDynInstPtr inst;

    ExecContext(CClassCPU &cpu_, SimpleThread &thread_, Execute &execute_,
                CClassDynInstPtr inst_) :
        cpu(cpu_),
        thread(thread_),
        execute(execute_),
        inst(inst_)
    {
        pcState(*inst->pc);
        setPredicate(inst->readPredicate());
        setMemAccPredicate(inst->readMemAccPredicate());
    }

    ~ExecContext()
    {
        inst->setPredicate(readPredicate());
        inst->setMemAccPredicate(readMemAccPredicate());
    }

    bool readSourceWithForwarding(const StaticInst *si, int src_idx,
        const std::vector<ForwardingSource> &mem_exe_forwards,
        const std::vector<ForwardingSource> &mem_wb_forwards,
        RegVal &value) const;

    bool resolveRiscvControl(const StaticInst *si,
        const std::vector<ForwardingSource> &mem_exe_forwards,
        const std::vector<ForwardingSource> &mem_wb_forwards,
        RiscvControlResolution &resolution) const;

    Fault initiateMemRead(Addr addr, unsigned int size, Request::Flags flags,
        const std::vector<bool> &byte_enable) override
    {
        panic("CClass ExecContext::initiateMemRead is not wired to an LSQ yet\n");
        return NoFault;
    }

    Fault initiateMemMgmtCmd(Request::Flags flags) override
    {
        panic("CClass ExecContext::initiateMemMgmtCmd is not implemented\n");
        return NoFault;
    }

    Fault writeMem(uint8_t *data, unsigned int size, Addr addr,
        Request::Flags flags, uint64_t *res,
        const std::vector<bool> &byte_enable) override
    {
        panic("CClass ExecContext::writeMem is not wired to an LSQ yet\n");
        return NoFault;
    }

    Fault initiateMemAMO(Addr addr, unsigned int size, Request::Flags flags,
        AtomicOpFunctorPtr amo_op) override
    {
        panic("CClass ExecContext::initiateMemAMO is not wired to an LSQ yet\n");
        return NoFault;
    }

    RegVal getRegOperand(const StaticInst *si, int idx) override
    {
        const RegId &reg = si->srcRegIdx(idx);
        return reg.is(InvalidRegClass) ? 0 : thread.getReg(reg);
    }

    void getRegOperand(const StaticInst *si, int idx, void *val) override
    {
        thread.getReg(si->srcRegIdx(idx), val);
    }

    void *getWritableRegOperand(const StaticInst *si, int idx) override
    {
        return thread.getWritableReg(si->destRegIdx(idx));
    }

    void setRegOperand(const StaticInst *si, int idx, RegVal val) override
    {
        const RegId &reg = si->destRegIdx(idx);
        if (!reg.is(InvalidRegClass))
            thread.setReg(reg, val);
    }

    void setRegOperand(const StaticInst *si, int idx, const void *val) override
    {
        thread.setReg(si->destRegIdx(idx), val);
    }

    bool readPredicate() const override { return thread.readPredicate(); }
    void setPredicate(bool val) override { thread.setPredicate(val); }

    bool readMemAccPredicate() const override
    { return thread.readMemAccPredicate(); }

    void setMemAccPredicate(bool val) override
    { thread.setMemAccPredicate(val); }

    uint64_t getHtmTransactionUid() const override
    {
        panic("CClass ExecContext::getHtmTransactionUid is not implemented\n");
        return 0;
    }

    uint64_t newHtmTransactionUid() const override
    {
        panic("CClass ExecContext::newHtmTransactionUid is not implemented\n");
        return 0;
    }

    bool inHtmTransactionalState() const override { return false; }

    uint64_t getHtmTransactionalDepth() const override
    {
        panic("CClass ExecContext::getHtmTransactionalDepth is not implemented\n");
        return 0;
    }

    const PCStateBase &pcState() const override { return thread.pcState(); }
    void pcState(const PCStateBase &val) override { thread.pcState(val); }

    RegVal readMiscRegNoEffect(int misc_reg) const
    { return thread.readMiscRegNoEffect(misc_reg); }

    RegVal readMiscReg(int misc_reg) override
    { return thread.readMiscReg(misc_reg); }

    void setMiscReg(int misc_reg, RegVal val) override
    { thread.setMiscReg(misc_reg, val); }

    RegVal readMiscRegOperand(const StaticInst *si, int idx) override
    {
        const RegId &reg = si->srcRegIdx(idx);
        assert(reg.is(MiscRegClass));
        return thread.readMiscReg(reg.index());
    }

    void setMiscRegOperand(const StaticInst *si, int idx, RegVal val) override
    {
        const RegId &reg = si->destRegIdx(idx);
        assert(reg.is(MiscRegClass));
        thread.setMiscReg(reg.index(), val);
    }

    ThreadContext *tcBase() const override { return thread.getTC(); }

    unsigned int readStCondFailures() const override { return 0; }
    void setStCondFailures(unsigned int st_cond_failures) override {}

    ContextID contextId() { return thread.contextId(); }

    void demapPage(Addr vaddr, uint64_t asn) override
    {
        thread.getMMUPtr()->demapPage(vaddr, asn);
    }

    BaseCPU *getCpuPtr() { return &cpu; }

    void armMonitor(Addr address) override
    {
        getCpuPtr()->armMonitor(inst->id.threadId, address);
    }

    bool mwait(PacketPtr pkt) override
    {
        return getCpuPtr()->mwait(inst->id.threadId, pkt);
    }

    void mwaitAtomic(ThreadContext *tc) override
    {
        getCpuPtr()->mwaitAtomic(inst->id.threadId, tc, thread.mmu);
    }

    AddressMonitor *getAddrMonitor() override
    {
        return getCpuPtr()->getCpuAddrMonitor(inst->id.threadId);
    }
};

} // namespace cclass
} // namespace gem5

#endif /* __CPU_CCLASS_EXEC_CONTEXT_HH__ */
