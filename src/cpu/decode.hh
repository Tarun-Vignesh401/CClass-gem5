#include "base/named.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/dyn_inst.hh"
#include "cpu/cclass/pipe_data.hh"
#include "debug/CClassCPU.hh"
#include <vector>

#ifndef __CPU_CCLASS_DECODE_HH__
#define __CPU_CCLASS_DECODE_HH__


namespace gem5
{
namespace cclass{


class Decode : public Named {


protected:
    Latch<BranchData>::Output branch;
    Latch<ForwardLineData>::Output inp;
    Latch<ForwardInstData>::Input out;
    std::vector<InputBuffer<ForwardInstData>> &nextStageReserve;

    CClassCPU &cpu;

    ThreadID threadPriority;

  struct DecodeThreadInfo
    {
        DecodeThreadInfo() {}

        DecodeThreadInfo(const DecodeThreadInfo& other) :
            inputIndex(other.inputIndex),
            lastStreamSeqNum(other.lastStreamSeqNum),
            expectedStreamSeqNum(other.expectedStreamSeqNum),
            predictionSeqNum(other.predictionSeqNum),
            havePC(other.havePC),
            blocked(other.blocked)
        {
            set(pc,other.pc);
        }

        /** Index into an incompletely processed input line that instructions
         *  are to be extracted from */
        unsigned int inputIndex = 0;
        

        /** Stream sequence number of the last seen line used to identify
         *  changes of instruction stream */
        InstSeqNum lastStreamSeqNum = InstId::firstStreamSeqNum;

        /** Decode is the source of fetch sequence numbers.  These represent the
         *  sequence that instructions were extracted from fetched lines. */
        InstSeqNum fetchSeqNum = InstId::firstFetchSeqNum;

        /** Stream sequence number remembered from last time the
         *  predictionSeqNum changed.  Lines should only be discarded when their
         *  predictionSeqNums disagree with Decode::predictionSeqNum *and* they
         *  are from the same stream that bore that prediction number */
        InstSeqNum expectedStreamSeqNum = InstId::firstStreamSeqNum;

        /** Decode is the source of prediction sequence numbers.  These
         *  represent predicted changes of control flow sources from branch
         *  prediction in Decode. */
        InstSeqNum predictionSeqNum = InstId::firstPredictionSeqNum;

        std::unique_ptr<PCStateBase> pc;

        bool havePC = false;

        /** Blocked indication for report */
        bool blocked = false;
    };

     /*struct DecodeStats : public statistics::Group
    {
        DecodeStats(CClassCPU *cpu);
        /** Stats 
        statistics::Scalar totalInstructions;
        statistics::Scalar intInstructions;
        statistics::Scalar fpInstructions;
        statistics::Scalar vecInstructions;
        statistics::Scalar loadInstructions;
        statistics::Scalar storeInstructions;
        statistics::Scalar amoInstructions;
    } stats;*/

    void dumpAllInput(ThreadID tid);

    bool checkRedirect();

    ThreadID getScheduledThread();

    std::vector<DecodeThreadInfo> decodeInfo;

    unsigned int outputWidth;

public:
    /* Public for Pipeline to be able to pass it to Decode */
    std::vector<InputBuffer<ForwardLineData>> inputBuffer;

     Decode(const std::string &name,
        CClassCPU &cpu_,
        const BaseCClassCPUParams &params,
        Latch<BranchData>::Output branch_,
        Latch<ForwardLineData>::Output inp_,
        Latch<ForwardInstData>::Input out_,
        std::vector<InputBuffer<ForwardInstData>> &next_stage_input_buffer);

    
    void evaluate();

    const ForwardLineData* getInput(ThreadID tid);

    void popInput(ThreadID tid);

    bool processMoreThanOneInput;



};
}//namespace cclass
}//namespace gem5

#endif /*__CPU_CCLASS_DECODE_HH__*/
