
/**
 * @file
 *
 *  A simple instruction scoreboard for tracking dependencies in Execute.
 */

#ifndef __CPU_CCLASS_SCOREBOARD_HH__
#define __CPU_CCLASS_SCOREBOARD_HH__

#include <vector>

#include "base/named.hh"
#include "base/types.hh"
#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/dyn_inst.hh"
#include "cpu/cclass/pipe_data.hh"
#include "cpu/cclass/trace.hh"
#include "cpu/reg_class.hh"

namespace gem5
{

namespace cclass
{

/** A scoreboard of register dependencies including, for each register:
 *  The number of in-flight instructions which will generate a result for
 *  this register */
class Scoreboard : public Named
{
  public:
    const BaseISA::RegClasses regClasses;

    /* for now no float to float forwarding */
    typedef enum{
        Int,
        None
    } forwardresult;

    const unsigned intRegOffset;
    const unsigned floatRegOffset;
    //const unsigned vecRegOffset;
    //const unsigned vecRegElemOffset;
    //const unsigned vecPredRegOffset;

    /** The number of registers in the Scoreboard.  These
     *  are just the integer, CC and float registers packed
     *  together with integer regs in the range [0,NumIntRegs-1],
     *  CC regs in the range [NumIntRegs, NumIntRegs+NumCCRegs-1]
     *  and float regs in the range
     *  [NumIntRegs+NumCCRegs, NumFloatRegs+NumIntRegs+NumCCRegs-1] */
    const unsigned numRegs;

    /** Type to use when indexing numResults */
    typedef unsigned short int Index;

    /** Count of the number of in-flight instructions that
     *  have results for each register */
    //std::vector<Index> numResults;

    /** Count of the number of results which can't be predicted */
    //std::vector<Index> numUnpredictableResults;

    /** Index of the FU generating this result */
    //std::vector<int> fuIndices;
    //static constexpr int invalidFUIndex = -1;

    /** The estimated cycle number that the result will be presented.
     *  This can be offset from to allow forwarding to be simulated as
     *  long as instruction completion is *strictly* in order with
     *  respect to instructions with unpredictable result timing */
    //std::vector<Cycles> returnCycle;

    /** The execute sequence number of the most recent inst to generate this
     *  register value */
    std::vector<InstSeqNum> rename_id;

  public:
    Scoreboard(const std::string &name,
    const BaseISA::RegClasses &reg_classes,
    const std::vector<InstructionInputBuffer<ForwardResultData>> &base_buf,
    const std::vector<InstructionInputBuffer<ForwardResultData>> &mbox_buf,
    const std::vector<InstructionInputBuffer<ForwardResultData>> &fbox_buf) :
    Named(name),
    regClasses(reg_classes),
    intRegOffset(0),
    floatRegOffset(intRegOffset + reg_classes.at(IntRegClass)->numRegs()),
    numRegs(floatRegOffset + reg_classes.at(FloatRegClass)->numRegs()),
    rename_id(numRegs, 0),
    baseBuf(base_buf),
    mboxBuf(mbox_buf),
    fboxBuf(fbox_buf)
{ }

  public:
    /** Sets scoreboard_index to the index into numResults of the
     *  given register index.  Returns true if the given register
     *  is in the scoreboard and false if it isn't */
    bool findIndex(const RegId& reg, Index &scoreboard_index);

    /** Mark up an instruction's effects by incrementing
     *  numResults counts.  If mark_unpredictable is true, the inst's
     *  destination registers are marked as being unpredictable without
     *  an estimated retire time */
    void markupInstDests(CClassDynInstPtr inst, ThreadContext *thread_ctx);

    /** Clear down the dependencies for this instruction.  clear_unpredictable
     *  must match mark_unpredictable for the same inst. */
    void clearInstDests(CClassDynInstPtr inst);

    void clearScoreBoard();

    /** Returns the exec sequence number of the most recent inst on
     *  which the given inst depends.  Useful for determining which
     *  inst must actually be committed before a dependent inst
     *  can call initiateAcc */
    InstSeqNum execSeqNumToWaitFor(CClassDynInstPtr inst, ThreadContext *thread_context);

    /** Can this instruction be issued.  Are any of its source registers
     *  due to be written by other marked-up instructions in flight */
    bool canInstIssue(CClassDynInstPtr inst, ThreadContext *thread_context);

    forwardresult checkExeIsbForId(ThreadID tid, InstSeqNum num);

    forwardresult checkMemIsbForId(ThreadID tid, InstSeqNum num);

    RegVal forwardRegResult(ThreadID tid, InstSeqNum num, const RegId &reg);
      
    bool lookForForwards(ThreadID tid, const RegId &reg, RegVal& forwarded_value);

    /** CClassTraceIF interface  have to figure out*/
    //void minorTrace() const;

    private:
    const std::vector<InstructionInputBuffer<ForwardResultData>> &baseBuf;
    const std::vector<InstructionInputBuffer<ForwardResultData>> &mboxBuf;
    const std::vector<InstructionInputBuffer<ForwardResultData>> &fboxBuf;
};

} // namespace cclass
} // namespace gem5

#endif /* __CPU_CCLASS_SCOREBOARD_HH__ */
