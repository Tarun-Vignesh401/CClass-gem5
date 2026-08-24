#include "cpu/cclass/memory.hh"
#include "cpu/cclass/cpu.hh"

namespace gem5
{

namespace cclass
{

Memory::Memory(const std::string &name_, CClassCPU &cpu_,
    const BaseCClassCPUParams &params, Latch<ForwardInstData>::Output in_BASE,Latch<ForwardMemData>::Output in_MEMORY,
    Latch<ForwardInstData>::Output in_TRAP,
    Latch<ForwardInstData>::Output in_MBOX,
    Latch<ForwardInstData>::Output in_FBOX):
    Named(name_),
    cpu(cpu_),
    in_BASE(in_BASE),
    in_MEMORY(in_MEMORY),
    in_TRAP(in_TRAP),
    in_MBOX(in_MBOX),
    in_FBOX(in_FBOX)

{
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        inputBuffer_BASE.emplace_back(name_ + ".inputBuffer_BASE" + std::to_string(tid),
            "base_reqs", params.executeInputBufferSize);
         inputBuffer_BASE.emplace_back(name_ + ".inputBuffer_TRAP" + std::to_string(tid),
            "trap_reqs", params.executeInputBufferSize);
         inputBuffer_BASE.emplace_back(name_ + ".inputBuffer_MBOX" + std::to_string(tid),
            "mbox_reqs", params.executeInputBufferSize);
         inputBuffer_BASE.emplace_back(name_ + ".inputBuffer_FBOX" + std::to_string(tid),
            "fbox_reqs", params.executeInputBufferSize);
         inputBuffer_MEMORY.emplace_back(name_ + ".inputBuffer_MEMORY" + std::to_string(tid),
            "mem_reqs", params.executeInputBufferSize);
    }
}

const ForwardMemData *
Memory::getInput(ThreadID tid, &InputBuffer inputBuffer)
{
    if (inputBuffer[tid].empty())
        return nullptr;

    return &(inputBuffer[tid].front());
}
const ForwardInstData *
Memory::getInput(ThreadID tid, &InputBuffer inputBuffer)
{
    if (inputBuffer[tid]->empty())
        return nullptr;

    return inputBuffer[tid]->front();
}

void
Memory::popInput(ThreadID tid, &InputBuffer inputBuffer)
{
    if (!inputBuffer[tid]->empty())
        inputBuffer[tid]->pop();
}

void
Memory::evaluate()
{
    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->threadId].setTail(*inp.outputWire);

    
    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->threadId].pushTail();
}


void
Memory::process(const ExecRequestPtr &request)
{

}

} // namespace cclass
} // namespace gem5
