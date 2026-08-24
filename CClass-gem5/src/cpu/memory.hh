#ifndef __CPU_CCLASS_MEMORY_HH__
#define __CPU_CCLASS_MEMORY_HH__

#include <vector>

#include "base/named.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/pipe_data.hh"
#include "params/BaseCClassCPU.hh"


namespace gem5
{

namespace cclass
{

class CClassCPU;

class Memory : public Named
{
  protected:
    CClassCPU &cpu;
    Latch<ForwardInstData>::Output in_BASE;
    Latch<ForwardMemData>::Output  in_MEMORY;
    Latch<ForwardInstData>::Output in_TRAP;
    Latch<ForwardInstData>::Output in_MBOX;
    Latch<ForwardInstData>::Output in_FBOX;
    
    std::vector<ForwardInstData>::Input nextStageReserve;


  public:
    std::vector<InputBuffer<ForwardMemData>> inputBuffer_MEMORY;
    std::vector<InputBuffer<ForwardInstData>> inputBuffer_BASE;
    std::vector<InputBuffer<ForwardInstData>> inputBuffer_TRAP;
    std::vector<InputBuffer<ForwardInstData>> inputBuffer_MBOX;
    std::vector<InputBuffer<ForwardInstData>> inputBuffer_FBOX;

    Memory(const std::string &name_, CClassCPU &cpu_,
        const BaseCClassCPUParams &params,
        Latch<ForwardMemData>::Output inp_);

    void evaluate();

  protected:
    const ForwardMemData *getInput(ThreadID tid, InputBuffer& inputBuffer);
    const ForwardInstData *getInput(ThreadID tid, InputBuffer& inputBuffer);
    void popInput(ThreadID tid);
    void process(const ExecRequestPtr &request);
};

} // namespace cclass
} // namespace gem5

#endif // __CPU_CCLASS_MEMORY_HH__
