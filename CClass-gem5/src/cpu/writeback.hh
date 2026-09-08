#ifndef __CPU_CCLASS_WRITEBACK_HH__
#define __CPU_CCLASS_WRITEBACK_HH__

#include <vector>

#include "base/named.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/pipe_data.hh"
#include "params/BaseCClassCPU.hh"

namespace gem5
{

class CClassCPU;

namespace cclass
{

class Writeback : public Named
{
  protected:
    CClassCPU &cpu;
    Latch<ForwardResultData>::Output in_COMMON;
    Latch<ForwardResultData>::Output in_TRAP;
    Latch<InstOrderData>::Output in_order;

  public:
    std::vector<InputBuffer<ForwardResultData>> inputBuffer_COMMON;
    std::vector<InputBuffer<ForwardResultData>> inputBuffer_TRAP;
    InputBuffer<InstOrderData> inputBuffer_ORDER;

    Writeback(const std::string &name_, CClassCPU &cpu_,
        const BaseCClassCPUParams &params,
        Latch<ForwardResultData>::Output in_COMMON_,
        Latch<ForwardResultData>::Output in_TRAP_,
        Latch<InstOrderData>::Output in_order_);

    void evaluate();

  protected:
    struct WritebackThreadInfo
    {
        unsigned int commonInputIndex = 0;
        unsigned int trapInputIndex = 0;
        unsigned int orderIndex = 0;
    };

    std::vector<WritebackThreadInfo> writebackInfo;

    ForwardResultData *getCommonInput(ThreadID tid);
    ForwardResultData *getTrapInput(ThreadID tid);
    InstOrderData *getOrderInput();

    unsigned int writebackWidth; 

    void resetConsumedInputs(ThreadID tid);
};

} // namespace cclass
} // namespace gem5

#endif // __CPU_CCLASS_WRITEBACK_HH__
