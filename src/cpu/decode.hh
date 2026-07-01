#include "base/named.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/dyn_inst.hh"
#include "cpu/cclass/pipe_data.hh"

#include <vector>

#ifndef __CPU_CCLASS_DECODE_HH__
#define __CPU_CCLASS_DECODE_HH__


namespace gem5
{
namespace cclass{


class Decode : public Named {


protected:
    Latch<ForwardLineData>::Output inp;

    CClassCPU &cpu;

    ThreadID threadPriority;
public:
    /* Public for Pipeline to be able to pass it to Fetch2 */
    std::vector<InputBuffer<ForwardLineData>> inputBuffer;

     Decode(const std::string &name,
        CClassCPU &cpu_,
        const BaseCClassCPUParams &params,
        Latch<ForwardLineData>::Output inp_);
    
    void evaluate();

    

};
}//namespace cclass
}//namespace gem5

#endif /*__CPU_CCLASS_DECODE_HH__*/