#ifndef __CPU_CCLASS_MEMORY_HH__
#define __CPU_CCLASS_MEMORY_HH__

#include <vector>

#include "base/named.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/pipe_data.hh"
#include "params/BaseCClassCPU.hh"
#include "cpu/cclass/scoreboard.hh"
#include "cpu/cclass/exec_context.hh"



namespace gem5
{

class CClassCPU;

namespace cclass
{


class Memory : public Named
{
  protected:
    CClassCPU &cpu;
    Latch<ForwardResultData>::Output in_BASE;
    Latch<ForwardMemData>::Output in_MEMORY;
    Latch<ForwardResultData>::Output in_TRAP;
    Latch<ForwardResultData>::Output in_MBOX;
    Latch<ForwardResultData>::Output in_FBOX;
    Latch<InstOrderData>::Output in_order;

    Latch<ForwardResultData>::Input out_COMMON;
    Latch<ForwardResultData>::Input out_TRAP;
    Latch<InstOrderData>::Input out_order;

  public:
    std::vector<InputBuffer<ForwardMemData>> inputBuffer_MEMORY;
    std::vector<InputBuffer<ForwardResultData>> inputBuffer_BASE;
    std::vector<InputBuffer<ForwardResultData>> inputBuffer_TRAP;
    std::vector<InputBuffer<ForwardResultData>> inputBuffer_MBOX;
    std::vector<InputBuffer<ForwardResultData>> inputBuffer_FBOX;
    InputBuffer<InstOrderData> inputBuffer_ORDER;

    Memory(const std::string &name_, CClassCPU &cpu_,
        const BaseCClassCPUParams &params,
        Latch<ForwardResultData>::Output in_BASE,
        Latch<ForwardMemData>::Output in_MEMORY,
        Latch<ForwardResultData>::Output in_TRAP,
        Latch<ForwardResultData>::Output in_MBOX,
        Latch<ForwardResultData>::Output in_FBOX,
        Latch<InstOrderData>::Output in_order,
        Latch<ForwardResultData>::Input out_COMMON,
        Latch<ForwardResultData>::Input out_TRAP,
        Latch<InstOrderData>::Input out_order,
        std::vector<InputBuffer<ForwardResultData>> &nextStageReserve_COMMON,
        std::vector<InputBuffer<ForwardResultData>> &nextStageReserve_TRAP);

    void evaluate();

  protected:
  std::vector<InputBuffer<ForwardResultData>> &nextStageReserve_COMMON;
  std::vector<InputBuffer<ForwardResultData>> &nextStageReserve_TRAP;

    ForwardMemData *getMemInput(ThreadID tid);

    ForwardResultData *getInstInput(
        ThreadID tid, std::vector<InputBuffer<ForwardResultData>> &input_buffer);

    void popMemInput(ThreadID tid);

    void popInstInput(
        ThreadID tid, std::vector<InputBuffer<ForwardResultData>> &input_buffer);

    InstOrderData *getOrderInput();

    void popOrderInput();

    bool pushCommon(ThreadID tid, const ExecResult &result);

    bool pushCommonMem(ThreadID tid, ExecRequestPtr request);

    bool pushTrap(ThreadID tid, const ExecResult &result);

    unsigned int memoryIssueLimit;

    void resetConsumedInputs(ThreadID tid);
    
    struct MemoryThreadInfo
    { 
      /*input Indices book keeping*/
      unsigned int baseInputIndex = 0;
      unsigned int memoryInputIndex = 0;
      unsigned int trapInputIndex = 0;
      unsigned int mboxInputIndex = 0;
      unsigned int fboxInputIndex = 0;

      /* order index for tracking current index of seqnums*/
      unsigned int order_index = 0;
      InstOrderData order;

      /*output indices bookkeeping*/
      unsigned int commonOutputIndex = 0;
      unsigned int trapOutputIndex = 0;

      bool isStreamed(){
        if(order_index == order.width())return true;
        return false;
      }
    };
    std::vector<MemoryThreadInfo> memoryInfo;
};

} // namespace cclass
} // namespace gem5

#endif // __CPU_CCLASS_MEMORY_HH__
