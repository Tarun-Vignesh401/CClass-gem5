#include "cpu/cclass/memory.hh"

#include "cpu/cclass/cpu.hh"
#include "debug/CClassMem.hh"

namespace gem5
{

namespace cclass
{

Memory::Memory(const std::string &name_, CClassCPU &cpu_,
    const BaseCClassCPUParams &params, Latch<ForwardMemData>::Output inp_) :
    Named(name_),
    cpu(cpu_),
    inp(inp_)
{
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        inputBuffer.emplace_back(name_ + ".inputBuffer" + std::to_string(tid),
            "mem_reqs", params.executeInputBufferSize);
    }
}

const ForwardMemData *
Memory::getInput(ThreadID tid)
{
    if (inputBuffer[tid].empty())
        return nullptr;

    return &(inputBuffer[tid].front());
}

void
Memory::popInput(ThreadID tid)
{
    if (!inputBuffer[tid].empty())
        inputBuffer[tid].pop();
}

void
Memory::evaluate()
{
    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->threadId].setTail(*inp.outputWire);

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        const ForwardMemData *mem_in = getInput(tid);
        if (!mem_in || mem_in->isBubble())
            continue;

        process(mem_in->request);
    }
}

void
Memory::advance()
{
}

void
Memory::process(const ExecRequestPtr &request)
{
    if (!request)
        return;

    DPRINTF(CClassMem, "Memory stage sees request for inst %s complete=%d\n",
        *request->inst, request->complete());

    if (request->complete() || request->failed())
        popInput(request->inst->id.threadId);
}

} // namespace cclass
} // namespace gem5
