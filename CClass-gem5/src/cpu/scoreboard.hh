
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

    const unsigned intRegOffset;
    const unsigned floatRegOffset;
    const unsigned ccRegOffset;
    const unsigned vecRegOffset;
    const unsigned vecRegElemOffset;
    const unsigned vecPredRegOffset;
    const unsigned matRegOffset;

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
    std::vector<Index> numResults;

    /** Count of the number of results which can't be predicted */
    std::vector<Index> numUnpredictableResults;

    /** Index of the FU generating this result */
    std::vector<int> fuIndices;
    static constexpr int invalidFUIndex = -1;

    /** The estimated cycle number that the result will be presented.
     *  This can be offset from to allow forwarding to be simulated as
     *  long as instruction completion is *strictly* in order with
     *  respect to instructions with unpredictable result timing */
    std::vector<Cycles> returnCycle;

    /** The execute sequence number of the most recent inst to generate this
     *  register value */
    std::vector<InstSeqNum> writingInst;

  public:
    Scoreboard(const std::string &name,
            const BaseISA::RegClasses& reg_classes) :
        Named(name),
        regClasses(reg_classes),
        intRegOffset(0),
        floatRegOffset(intRegOffset + reg_classes.at(IntRegClass)->numRegs()),
        ccRegOffset(floatRegOffset + reg_classes.at(FloatRegClass)->numRegs()),
        vecRegOffset(ccRegOffset + reg_classes.at(CCRegClass)->numRegs()),
        vecRegElemOffset(vecRegOffset + reg_classes.at(VecRegClass)->numRegs()),
        vecPredRegOffset(vecRegElemOffset +
                reg_classes.at(VecElemClass)->numRegs()),
        matRegOffset(vecPredRegOffset +
                reg_classes.at(VecPredRegClass)->numRegs()),
        numRegs(matRegOffset + reg_classes.at(MatRegClass)->numRegs()),
        numResults(numRegs, 0),
        numUnpredictableResults(numRegs, 0),
        fuIndices(numRegs, invalidFUIndex),
        returnCycle(numRegs, Cycles(0)),
        writingInst(numRegs, 0)
    { }

  public:
    /** Sets scoreboard_index to the index into numResults of the
     *  given register index.  Returns true if the given register
     *  is in the scoreboard and false if it isn't */
    bool findIndex(const RegId& reg, Index &scoreboard_index);

    bool isBranchForward();

    /** Mark up an instruction's effects by incrementing
     *  numResults counts.  If mark_unpredictable is true, the inst's
     *  destination registers are marked as being unpredictable without
     *  an estimated retire time */
    void markupInstDests(CClassDynInstPtr inst, Cycles retire_time,
        ThreadContext *thread_context, bool mark_unpredictable);

    /** Clear down the dependencies for this instruction.  clear_unpredictable
     *  must match mark_unpredictable for the same inst. */
    void clearInstDests(CClassDynInstPtr inst, bool clear_unpredictable);

    /** Returns the exec sequence number of the most recent inst on
     *  which the given inst depends.  Useful for determining which
     *  inst must actually be committed before a dependent inst
     *  can call initiateAcc */
    InstSeqNum execSeqNumToWaitFor(CClassDynInstPtr inst,
        ThreadContext *thread_context);

    /** Can this instruction be issued.  Are any of its source registers
     *  due to be written by other marked-up instructions in flight */
    bool canInstIssue(CClassDynInstPtr inst,
        const std::vector<Cycles> *src_reg_relative_latencies,
        const std::vector<bool> *cant_forward_from_fu_indices,
        Cycles now, ThreadContext *thread_context);

    /** CClassTraceIF interface */
    //void minorTrace() const;
};

} // namespace cclass
} // namespace gem5

#endif /* __CPU_CCLASS_SCOREBOARD_HH__ */
