#include "cpu/cclass/memory.hh"
#include "cpu/cclass/cpu.hh"
#include "debug/CClassMemory.hh"

namespace gem5
{

namespace cclass
{

Memory::Memory(const std::string &name_, CClassCPU &cpu_,
    const BaseCClassCPUParams &params,
    Latch<ForwardResultData>::Output in_BASE_,
    Latch<ForwardMemData>::Output in_MEMORY_,
    Latch<ForwardResultData>::Output in_TRAP_,
    Latch<ForwardResultData>::Output in_MBOX_,
    Latch<ForwardResultData>::Output in_FBOX_,
    Latch<InstOrderData>::Output in_order_,
    Latch<ForwardResultData>::Input out_COMMON_,
    Latch<ForwardResultData>::Input out_TRAP_,
    Latch<InstOrderData>::Input out_order_,
    std::vector<InstructionInputBuffer<ForwardResultData>> &nextStageReserve_COMMON_,
    std::vector<InstructionInputBuffer<ForwardResultData>> &nextStageReserve_TRAP_) :
    Named(name_),
    cpu(cpu_),
    in_BASE(in_BASE_),
    in_MEMORY(in_MEMORY_),
    in_TRAP(in_TRAP_),
    in_MBOX(in_MBOX_),
    in_FBOX(in_FBOX_),
    in_order(in_order_),
    out_COMMON(out_COMMON_),
    out_TRAP(out_TRAP_),
    out_order(out_order_),
    inputBuffer_ORDER(name_ + ".inputBuffer_ORDER", "inst_order",
        params.executeInputBufferSize * MAX_FORWARD_INSTS),
    nextStageReserve_COMMON(nextStageReserve_COMMON_),
    nextStageReserve_TRAP(nextStageReserve_TRAP_),
    memoryIssueLimit(params.memoryIssueLimit),
    memoryInfo(params.numThreads)
{
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        const std::string tid_str = std::to_string(tid);

        inputBuffer_BASE.emplace_back(name_ + ".inputBuffer_BASE" + tid_str,
            "base_reqs", params.executeInputBufferSize);
        inputBuffer_TRAP.emplace_back(name_ + ".inputBuffer_TRAP" + tid_str,
            "trap_reqs", params.executeInputBufferSize);
        inputBuffer_MBOX.emplace_back(name_ + ".inputBuffer_MBOX" + tid_str,
            "mbox_reqs", params.executeInputBufferSize);
        inputBuffer_FBOX.emplace_back(name_ + ".inputBuffer_FBOX" + tid_str,
            "fbox_reqs", params.executeInputBufferSize);
        inputBuffer_MEMORY.emplace_back(name_ + ".inputBuffer_MEMORY" + tid_str,
            "mem_reqs", params.executeInputBufferSize);
    }
}

ForwardMemData *
Memory::getMemInput(ThreadID tid)
{
    if (inputBuffer_MEMORY[tid].empty())
        return nullptr;

    return &inputBuffer_MEMORY[tid].front();
}

ForwardResultData *
Memory::getInstInput(ThreadID tid,
    std::vector<InstructionInputBuffer<ForwardResultData>> &input_buffer)
{
    if (input_buffer[tid].empty())
        return nullptr;

    return &input_buffer[tid].front();
}

void
Memory::popMemInput(ThreadID tid)
{
    if (!inputBuffer_MEMORY[tid].empty())
        inputBuffer_MEMORY[tid].pop();
}

void
Memory::popInstInput(ThreadID tid,
    std::vector<InstructionInputBuffer<ForwardResultData>> &input_buffer)
{
    if (!input_buffer[tid].empty())
        input_buffer[tid].pop();
}

InstOrderData *
Memory::getOrderInput()
{
    if (inputBuffer_ORDER.empty())
        return nullptr;

    return &inputBuffer_ORDER.front();
}

void
Memory::popOrderInput()
{
    if (!inputBuffer_ORDER.empty())
        inputBuffer_ORDER.pop();
}

bool
Memory::pushCommon(ThreadID tid, const ExecResult &result)
{
    ForwardResultData &common_out = *out_COMMON.inputWire;
    CClassDynInstPtr inst = result.inst;
    MemoryThreadInfo &thread = memoryInfo[tid];
    unsigned int index = thread.commonOutputIndex;

    if (index >= MAX_FORWARD_INSTS)
        return false;

    ExecResult &slot = common_out.results[index];
    if (slot.inst && !slot.inst->isBubble())
        return false;

    common_out.threadId = tid;
    slot = result;
    DPRINTF(CClassMemory,
        "Memory pushed common inst: slot=%u execSeq=%llu "
        "staticInst=%s\n",
        index, inst->id.execSeqNum,
        inst->staticInst ? inst->staticInst->getName() : "null");
    return true;
}
/*
bool
Memory::pushCommonMem(ThreadID tid, ExecRequestPtr request)
{
    ForwardReData &mem_out = *out_COMMON.inputWire;

    if (mem_out.isBubble()) {
        mem_out = ForwardMemData(MAX_FORWARD_INSTS, tid);
        mem_out.threadId = tid;
    }

    for (unsigned int i = 0; i < mem_out.width(); i++) {
        if (!mem_out.requests[i]) {
            mem_out.requests[i] = request;
            DPRINTF(CClassMemory,
                "Memory pushed common mem request: slot=%u execSeq=%llu "
                "staticInst=%s\n",
                i,
                request && request->inst ? request->inst->id.execSeqNum : 0,
                request && request->inst && request->inst->staticInst ?
                    request->inst->staticInst->getName() : "null");
            return true;
        }
    }

    return false;
}
*/
bool
Memory::pushTrap(ThreadID tid, const ExecResult &result)
{
    ForwardResultData &trap_out = *out_TRAP.inputWire;
    CClassDynInstPtr inst = result.inst;
    MemoryThreadInfo &thread = memoryInfo[tid];
    unsigned int index = thread.trapOutputIndex;


    if (index >= MAX_FORWARD_INSTS)
        return false;

    ExecResult &slot = trap_out.results[index];
    if (slot.inst && !slot.inst->isBubble())
        return false;

    trap_out.threadId = tid;
    slot = result;
    DPRINTF(CClassMemory,
        "Memory pushed trap inst: slot=%u execSeq=%llu "
        "staticInst=%s\n",
        index, inst->id.execSeqNum,
        inst->staticInst ? inst->staticInst->getName() : "null");
    return true;
}

void
Memory::resetConsumedInputs(ThreadID tid)
{
    MemoryThreadInfo &thread = memoryInfo[tid];

    ForwardResultData *base = getInstInput(tid, inputBuffer_BASE);
    if (base && thread.baseInputIndex >= base->validEntries()) {
        popInstInput(tid, inputBuffer_BASE);
        thread.baseInputIndex = 0;
    }

    ForwardResultData *trap = getInstInput(tid, inputBuffer_TRAP);
    if (trap && thread.trapInputIndex >= trap->validEntries()) {
        popInstInput(tid, inputBuffer_TRAP);
        thread.trapInputIndex = 0;
    }

    ForwardResultData *mbox = getInstInput(tid, inputBuffer_MBOX);
    if (mbox && thread.mboxInputIndex >= mbox->validEntries()) {
        popInstInput(tid, inputBuffer_MBOX);
        thread.mboxInputIndex = 0;
    }

    ForwardResultData *fbox = getInstInput(tid, inputBuffer_FBOX);
    if (fbox && thread.fboxInputIndex >= fbox->validEntries() ) {
        popInstInput(tid, inputBuffer_FBOX);
        thread.fboxInputIndex = 0;
    }

    ForwardMemData *mem = getMemInput(tid);
    if (mem && thread.memoryInputIndex >= mem->validEntries()) {
        popMemInput(tid);
        thread.memoryInputIndex = 0;
    }

    InstOrderData *order = getOrderInput();
    if (order && thread.order_index >= order->width()) {
        popOrderInput();
        thread.order_index = 0;
    }
    ForwardResultData& isb_COMMON = *out_COMMON.inputWire;
    if((!isb_COMMON.isBubble()) && thread.commonOutputIndex >= MAX_FORWARD_INSTS )
        thread.commonOutputIndex = 0;

    ForwardResultData& isb_TRAP = *out_TRAP.inputWire;
    if((!isb_TRAP.isBubble()) && thread.trapOutputIndex >= MAX_FORWARD_INSTS)
        thread.trapOutputIndex = 0;

}

void
Memory::evaluate()
{
    if (!in_BASE.outputWire->isBubble())
        inputBuffer_BASE[in_BASE.outputWire->threadId].setTail(
            *in_BASE.outputWire);
    if (!in_TRAP.outputWire->isBubble())
        inputBuffer_TRAP[in_TRAP.outputWire->threadId].setTail(
            *in_TRAP.outputWire);
    if (!in_MBOX.outputWire->isBubble())
        inputBuffer_MBOX[in_MBOX.outputWire->threadId].setTail(
            *in_MBOX.outputWire);
    if (!in_FBOX.outputWire->isBubble())
        inputBuffer_FBOX[in_FBOX.outputWire->threadId].setTail(
            *in_FBOX.outputWire);
    if (!in_MEMORY.outputWire->isBubble())
        inputBuffer_MEMORY[in_MEMORY.outputWire->threadId].setTail(
            *in_MEMORY.outputWire);
    if (!in_order.outputWire->isBubble())
        inputBuffer_ORDER.setTail(*in_order.outputWire);

   

    ThreadID tid = 0;
    MemoryThreadInfo &thread = memoryInfo[tid];
    thread.commonOutputIndex = 0;
    thread.trapOutputIndex = 0;
    ForwardResultData *inst_base = getInstInput(tid, inputBuffer_BASE);
    ForwardResultData *inst_trap = getInstInput(tid, inputBuffer_TRAP);
    ForwardResultData *inst_mbox = getInstInput(tid, inputBuffer_MBOX);
    ForwardResultData *inst_fbox = getInstInput(tid, inputBuffer_FBOX);
    ForwardMemData *inst_mem = getMemInput(tid);

    unsigned int num_issued = 0;
    InstOrderData issued_order;
    InstOrderData *order = getOrderInput();
    while (order && (thread.order_index < order->width()) && num_issued < memoryIssueLimit)
    {
        InstSeqNum inst_num = order->seqNums[thread.order_index];
        bool found = false;
        bool moved = false;

        if (inst_base) {
                ExecResult &result = inst_base->results[thread.baseInputIndex];
                CClassDynInstPtr inst = result.inst;
                if (inst && !inst->isBubble() &&
                    inst->id.execSeqNum == inst_num)
                {
                    found = true;
                    if (pushCommon(tid, result)) {
                        nextStageReserve_COMMON[tid].reserve();
                        moved = true;
                        thread.baseInputIndex++;
                        thread.order_index++;
                        thread.commonOutputIndex++;
                    }
                }
        }

        if (!found && inst_trap) {
                ExecResult &result = inst_trap->results[thread.trapInputIndex];
                CClassDynInstPtr inst = result.inst;
                if (inst && !inst->isBubble() &&
                    inst->id.execSeqNum == inst_num)
                {
                    found = true;
                    if (pushTrap(tid, result)) {
                        nextStageReserve_TRAP[tid].reserve();
                        moved = true;
                        thread.trapInputIndex++;
                        thread.order_index++;
                        thread.trapOutputIndex++;
                }
            }
        }
      

        if (!found && inst_mbox) {
                ExecResult &result = inst_mbox->results[thread.mboxInputIndex];
                CClassDynInstPtr inst = result.inst;
                if (inst && !inst->isBubble() &&
                    inst->id.execSeqNum == inst_num)
                {
                    found = true;
                    if (pushCommon(tid, result)) {
                        nextStageReserve_COMMON[tid].reserve();
                        thread.mboxInputIndex++;
                        thread.order_index++;
                        thread.commonOutputIndex++;
                        moved = true;
                    }
                }
        }

        if (!found && inst_fbox) {
                ExecResult &result = inst_fbox->results[thread.fboxInputIndex];
                CClassDynInstPtr inst = result.inst;
                if (inst && !inst->isBubble() &&
                    inst->id.execSeqNum == inst_num)
                {
                    found = true;
                    if (pushCommon(tid, result)) {
                        nextStageReserve_COMMON[tid].reserve();
                        thread.fboxInputIndex++;
                        thread.order_index++;
                        thread.commonOutputIndex++;
                        moved = true;
                    }
                }
        }

        if (!found && inst_mem) {
                ExecRequestPtr request = inst_mem->requests[thread.memoryInputIndex];
                CClassDynInstPtr inst = request ? request->inst : nullptr;
                if (inst && !inst->isBubble() &&
                    inst->id.execSeqNum == inst_num)
                {
                    found = true;
                    /* translation fault check */
                    if (inst->fault != NoFault || request->failed()) {
                        if (request->failed())
                            inst->fault = request->fault;
                        ExecResult result;
                        result.inst = inst;
                        if (pushTrap(tid, result)) {
                            nextStageReserve_TRAP[tid].reserve();
                            moved = true;
                            thread.memoryInputIndex++;
                            thread.order_index++;
                            thread.trapOutputIndex++;
                        }// fault handling block
                    } else if (request->complete()) {
                        PacketPtr packet = request->packet;
                        ExecResult result;
                        ExecContext context(cpu, *cpu.threads[inst->id.threadId], inst, &result);
                        inst->staticInst->completeAcc(packet, &context, inst->traceData);
                        result.inst = inst;
                        if (pushCommon(tid, result)) {
                            nextStageReserve_COMMON[tid].reserve();
                            thread.memoryInputIndex++;
                            thread.order_index++;
                            thread.commonOutputIndex++;
                            moved = true;
                        }// normal handling block
                    /* checked if the dcache request is complete or not*/
                    } else {
                        DPRINTF(CClassMemory,
                            "Memory request waiting: execSeq=%llu state=%d\n",
                            inst->id.execSeqNum, request->state);
                    }
                }
        }

        DPRINTF(CClassMemory, "We are currently processing the output Index: %d\n",
            thread.order_index);

        /*if there is no eligible instructions this cycle just break 
        *off and check in the next cycle*/
        if (!moved)
            break;

        issued_order.push_back(inst_num);
        num_issued++;

        inst_base = getInstInput(tid, inputBuffer_BASE);
        inst_trap = getInstInput(tid, inputBuffer_TRAP);
        inst_mbox = getInstInput(tid, inputBuffer_MBOX);
        inst_fbox = getInstInput(tid, inputBuffer_FBOX);
        inst_mem = getMemInput(tid);
    }

    if (order && order->isBubble())
        popOrderInput();
    
    /*Popping Mem-Exe ISB's here and resetting the sequence numbers */
    resetConsumedInputs(tid);

    *out_order.inputWire = issued_order;

    if (!in_BASE.outputWire->isBubble())
        inputBuffer_BASE[in_BASE.outputWire->threadId].pushTail();
    if (!in_TRAP.outputWire->isBubble())
        inputBuffer_TRAP[in_TRAP.outputWire->threadId].pushTail();
    if (!in_MBOX.outputWire->isBubble())
        inputBuffer_MBOX[in_MBOX.outputWire->threadId].pushTail();
    if (!in_FBOX.outputWire->isBubble())
        inputBuffer_FBOX[in_FBOX.outputWire->threadId].pushTail();
    if (!in_MEMORY.outputWire->isBubble())
        inputBuffer_MEMORY[in_MEMORY.outputWire->threadId].pushTail();
    if (!in_order.outputWire->isBubble())
        inputBuffer_ORDER.pushTail();

}



} // namespace cclass
} // namespace gem5
