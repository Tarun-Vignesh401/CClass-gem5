#include "cpu/cclass/writeback.hh"

#include "cpu/cclass/cpu.hh"
#include "debug/CClassWriteBack.hh"

namespace gem5
{

namespace cclass
{

Writeback::Writeback(const std::string &name_, CClassCPU &cpu_,
    const BaseCClassCPUParams &params,
    Latch<ForwardResultData>::Output in_COMMON_,
    Latch<ForwardResultData>::Output in_TRAP_,
    Latch<InstOrderData>::Output in_order_) :
    Named(name_),
    cpu(cpu_),
    in_COMMON(in_COMMON_),
    in_TRAP(in_TRAP_),
    in_order(in_order_),
    inputBuffer_ORDER(name_ + ".inputBuffer_ORDER", "inst_order",
        params.executeInputBufferSize),
    writebackInfo(params.numThreads),
    writebackWidth(params.writebackWidth)
{
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        const std::string tid_str = std::to_string(tid);

        inputBuffer_COMMON.emplace_back(name_ + ".inputBuffer_COMMON" + tid_str,
            "common_results", params.executeInputBufferSize);
        inputBuffer_TRAP.emplace_back(name_ + ".inputBuffer_TRAP" + tid_str,
            "trap_results", params.executeInputBufferSize);
    }
}

ForwardResultData *
Writeback::getCommonInput(ThreadID tid)
{
    if (inputBuffer_COMMON[tid].empty())
        return nullptr;

    return &inputBuffer_COMMON[tid].front();
}

ForwardResultData *
Writeback::getTrapInput(ThreadID tid)
{
    if (inputBuffer_TRAP[tid].empty())
        return nullptr;

    return &inputBuffer_TRAP[tid].front();
}

InstOrderData *
Writeback::getOrderInput()
{
    if (inputBuffer_ORDER.empty())
        return nullptr;

    return &inputBuffer_ORDER.front();
}

void
Writeback::popCommonInput(ThreadID tid)
{
    if (!inputBuffer_COMMON[tid].empty())
        inputBuffer_COMMON[tid].pop();
}

void
Writeback::popTrapInput(ThreadID tid)
{
    if (!inputBuffer_TRAP[tid].empty())
        inputBuffer_TRAP[tid].pop();
}

void
Writeback::popOrderInput()
{
    if (!inputBuffer_ORDER.empty())
        inputBuffer_ORDER.pop();
}

void
Writeback::resetConsumedInputs(ThreadID tid)
{
    WritebackThreadInfo &thread = writebackInfo[tid];

    ForwardResultData *common = getCommonInput(tid);
    if (common && thread.commonInputIndex >= common->validEntries()) {
        popCommonInput(tid);
        thread.commonInputIndex = 0;
        //DPRINTF(CClassWriteBack, "Popped the common isb\n");

    }

    ForwardResultData *trap = getTrapInput(tid);
    if (trap && thread.trapInputIndex >= trap->validEntries()) {
        popTrapInput(tid);
        //DPRINTF(CClassWriteBack, "Popped the trap isb\n");
        thread.trapInputIndex = 0;
    }

    InstOrderData *order = getOrderInput();
    if (order && thread.orderIndex >= order->validEntries()) {
        popOrderInput();
        //DPRINTF(CClassWriteBack, "Popped the order isb\n");
        thread.orderIndex = 0;
    }
}

void
Writeback::evaluate()
{
    if (!in_COMMON.outputWire->isBubble())
        inputBuffer_COMMON[in_COMMON.outputWire->threadId].setTail(
            *in_COMMON.outputWire);
    if (!in_TRAP.outputWire->isBubble())
        inputBuffer_TRAP[in_TRAP.outputWire->threadId].setTail(
            *in_TRAP.outputWire);
    if (!in_order.outputWire->isBubble())
        inputBuffer_ORDER.setTail(*in_order.outputWire);
    
    unsigned int num_issued = 0;

    ThreadID tid = 0;
    WritebackThreadInfo &thread = writebackInfo[tid];
    InstOrderData *order = getOrderInput();

    DPRINTF(CClassWriteBack,
    "WB loop check: order=%p orderIndex=%u orderEntries=%u "
    "numIssued=%u width=%u bufferEmpty=%d\n",
    order,
    thread.orderIndex,
    order ? order->validEntries() : 0,
    num_issued,
    writebackWidth,
    inputBuffer_ORDER.empty());
    
    while (order && thread.orderIndex < order->validEntries() && num_issued < writebackWidth) {
        InstSeqNum inst_num = order->seqNums[thread.orderIndex];
        bool moved = false;
        ForwardResultData *common = getCommonInput(tid);
        if (!moved && common && thread.commonInputIndex < common->validEntries()) {
            ExecResult &result = common->results[thread.commonInputIndex];
            CClassDynInstPtr inst = result.inst;

            if (inst && !inst->isBubble() &&
                inst->id.execSeqNum == inst_num)
            {
                if (inst->fault == NoFault)
                    result.commit(*cpu.threads[inst->id.threadId]);

                DPRINTF(CClassWriteBack,
                    "Writeback committed common execSeq=%llu fault=%d\n",
                    inst->id.execSeqNum, inst->fault != NoFault);

                thread.commonInputIndex++;
                thread.orderIndex++;
                moved = true;
            }
        }

        ForwardResultData *trap = getTrapInput(tid);
        if (!moved && trap && thread.trapInputIndex < trap->validEntries()) {
            ExecResult &result = trap->results[thread.trapInputIndex];
            CClassDynInstPtr inst = result.inst;

            if (inst && !inst->isBubble() &&
                inst->id.execSeqNum == inst_num)
            {
                DPRINTF(CClassWriteBack,
                    "Writeback saw trap execSeq=%llu\n",
                    inst->id.execSeqNum);

                thread.trapInputIndex++;
                thread.orderIndex++;
                moved = true;
            }
        }

        if (!moved)
        {
            DPRINTF(CClassWriteBack,
                    "ORDER BLOCKED: expected=%llu common=%llu trap=%llu "
                    "orderIndex=%u orderWidth=%u\n",
                    inst_num,
                    common && common->validEntries() ?
                        common->results[thread.commonInputIndex].inst->id.execSeqNum : 0,
                    trap && trap->validEntries() ?
                        trap->results[thread.trapInputIndex].inst->id.execSeqNum : 0,
                    thread.orderIndex, order->validEntries());
            break;
        }
        num_issued++;
        resetConsumedInputs(tid);
        //order = getOrderInput();
        DPRINTF(CClassWriteBack,
        "WB progress: commonIndex=%u trapIndex=%u orderIndex=%u\n",
        thread.commonInputIndex,
        thread.trapInputIndex,
        thread.orderIndex);
    }

    if (!in_COMMON.outputWire->isBubble())
        inputBuffer_COMMON[in_COMMON.outputWire->threadId].pushTail();
    if (!in_TRAP.outputWire->isBubble())
        inputBuffer_TRAP[in_TRAP.outputWire->threadId].pushTail();
    if (!in_order.outputWire->isBubble())
        inputBuffer_ORDER.pushTail();
}

} // namespace cclass
} // namespace gem5
