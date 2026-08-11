#include "cpu/cclass/memory_stage.hh"

namespace gem5
{

namespace cclass
{

void 
Memory::popInput(ThreadID tid){



    
}

void
Memory::evaluate()
{   
    CClassDynInstPtr currentInst = 
    // Do not consume more than one instruction in one cycle.
    if (currentInst || inputBuffer.empty())
        return;

    currentInst = inputBuffer.front();
    inputBuffer.pop();

    // A real implementation would initiate/check the memory operation here.
    process(currentInst);
}

void
Memory::advance()
{
    currentInst = nullptr;
}

void
Memory::process(const InstPtr &inst)
{
    // Generic stage: intentionally no memory-side effect.
    (void)inst;
}

} // namespace cclass
} // namespace gem5