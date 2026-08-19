/**
 * @file
 *
 *  All the fun of executing instructions from Decode and sending branch/new
 *  instruction stream info. to Fetch1.
 */

#ifndef __CPU_CCLASS_EXECUTE_HH__
#define __CPU_CCLASS_EXECUTE_HH__

#include <map>
#include <vector>

#include "base/named.hh"
#include "base/types.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/func_unit.hh"
#include "cpu/cclass/pipe_data.hh"
#include "cpu/cclass/scoreboard.hh"

namespace gem5
{

namespace cclass
{

/** Execute stage.  Everything apart from fetching and decoding instructions.
 *  The LSQ lives here too. */
class Execute : public Named
{
  protected:

    /** Input port carrying instructions from Decode */
    Latch<ForwardInstData>::Output inp;
    Latch<BranchData>::Input out_fetch1;
    Latch<BranchData>::Input out_fetch2;
    Latch<BranchData>::Input out_decode;

    Latch<ForwardInstData>::Input out_BASE;
    Latch<ForwardMemData>::Input out_MEMORY;
    Latch<ForwardInstData>::Input out_TRAP;
    Latch<ForwardInstData>::Input out_MBOX;
    Latch<ForwardInstData>::Input out_FBOX;

    CClassCPU &cpu;
        
    unsigned int memoryIssueLimit;

    unsigned int issueLimit;

    unsigned int InputBufferSize;

    bool allowEarlyMemIssue;

    bool processMoreThanOneInput;

    CClassFUPool &fuDescriptions;

    unsigned int numFuncUnits;

    // required for setting up activity recorder.
    Cycles longestFuLatency;

    /** Modify instruction trace times on commit */
    bool setTraceTimeOnCommit;

    /** Modify instruction trace times on issue */
    bool setTraceTimeOnIssue;

    /** The FU index of the non-existent costless FU for instructions
     *  which pass the MinorDynInst::isNoCostInst test */
    unsigned int noCostFUIndex;

    std::vector<Scoreboard> scoreboard;

    std::vector<FUPipeline *> funcUnits;

    // the execseqnum of the fence instructions
    //std::vector<InstSeqNum> lastMemBarrier;

    //bool tryToSendMemRequests(CClassDynInstPtr inst);


    public:
    std::vector<InputBuffer<ForwardInstData>> inputBuffer; 

    protected:
    enum DrainState
    {
        NotDraining, /* Not draining, possibly running */
        DrainHaltFetch, /* Halting Fetch after completing current inst */
        DrainAllInsts /* Discarding all remaining insts */
    };

    struct ExecuteThreadInfo
    {
        ExecuteThreadInfo(unsigned int insts_committed) :
            inputIndex(0),
            outputIndex(0),
            baseOutputIndex(0),
            memoryOutputIndex(0),
            trapOutputIndex(0),
            mboxOutputIndex(0),
            fboxOutputIndex(0),
            instsBeingCommitted(insts_committed),
            streamSeqNum(InstId::firstStreamSeqNum),
            lastPredictionSeqNum(InstId::firstPredictionSeqNum),
            drainState(NotDraining)
        { }

        ExecuteThreadInfo(const ExecuteThreadInfo& other) :
            inputIndex(other.inputIndex),
            outputIndex(other.outputIndex),
            baseOutputIndex(other.baseOutputIndex),
            memoryOutputIndex(other.memoryOutputIndex),
            trapOutputIndex(other.trapOutputIndex),
            mboxOutputIndex(other.mboxOutputIndex),
            fboxOutputIndex(other.fboxOutputIndex),
            instsBeingCommitted(other.instsBeingCommitted),
            streamSeqNum(other.streamSeqNum),
            lastPredictionSeqNum(other.lastPredictionSeqNum),
            drainState(other.drainState)
        { }

           /** Instructions either in FUs or waiting to be pushed onward */
        std::vector<QueuedInst> inFlightInsts;

        /** Memory ref instructions still in the FUs */
        Queue<QueuedInst, ReportTraitsAdaptor<QueuedInst> > *inFUMemInsts;

        /** Index that we've completed upto in getInput data.  We can say we're
         *  popInput when this equals getInput()->width() */
        unsigned int inputIndex;

        unsigned int outputIndex;
        unsigned int baseOutputIndex;
        unsigned int memoryOutputIndex;
        unsigned int trapOutputIndex;
        unsigned int mboxOutputIndex;
        unsigned int fboxOutputIndex;

         /** Structure for reporting insts currently being processed/retired
         *  for MinorTrace */
        ForwardInstData instsBeingCommitted;

        /** Source of sequence number for instuction streams.  Increment this and
         *  pass to fetch whenever an instruction stream needs to be changed.
         *  For any more complicated behaviour (e.g. speculation) there'll need
         *  to be another plan. */
        InstSeqNum streamSeqNum;

        /** A prediction number for use where one isn't available from an
         *  instruction.  This is harvested from committed instructions.
         *  This isn't really needed as the streamSeqNum will change on
         *  a branch, but it minimises disruption in stream identification */
        InstSeqNum lastPredictionSeqNum;

        /** State progression for draining NotDraining -> ... -> DrainAllInsts */
        DrainState drainState;
    };
    // all of this is to observe the state of execute..

    std::vector<ExecuteThreadInfo> executeInfo;

    ThreadID interruptPriority;
    ThreadID issuePriority;
    ThreadID commitPriority;

    protected:

    /** Get a piece of data to work on from the inputBuffer, or 0 if there
     *  is no data. */
    const ForwardInstData *getInput(ThreadID tid);

     /** Pop an element off the input buffer, if there are any */
    void popInput(ThreadID tid);

    /** Generate Branch data based (into branch) on an observed (or not)
     *  change in PC while executing an instruction.
     *  Also handles branch prediction information within the inst. */
    //void tryToBranch(CClassDynInstPtr inst, Fault fault, BranchData &branch);

    /** Actually create a branch to communicate to Fetch1/Fetch2 and,
     *  if that is a stream-changing branch update the streamSeqNum */
    void updateBranchData(ThreadID tid, BranchData::Reason reason, CClassDynInstPtr inst, const PCStateBase &target, BranchData &branch);

    void handleBranch(ThreadID tid, CClassDynInstPtr inst);


    //void issuedMemBarrierInst(CClassDynInstPtr inst);

    //bool tryPCEvents(ThreadID thread_id);

    void setDrainState(ThreadID thread_id, DrainState state);

    ThreadID getIssuingThread();

    void trytoPush(ThreadID tid);

    void cleanupInFlightInsts(ThreadID tid);

    void resetISBOutputIndexes(ThreadID tid);

    bool pushInstToLatch(ThreadID tid, CClassDynInstPtr inst);
    bool pushMemReqToLatch(ThreadID tid, ExecRequestPtr request);
    Fault initiateMemAccess(CClassDynInstPtr inst, ExecRequestPtr &request);



    public:
      class DcachePort : public CClassCPU::CClassCPUPort
    {
      protected:
        /** My owner */
        Execute &execute;

      public:
        DcachePort(std::string name, Execute &execute_, CClassCPU &cpu) :
            CClassCPU::CClassCPUPort(name, cpu), execute(execute_)
        { }

      protected:
        bool recvTimingResp(PacketPtr pkt) override
        { return execute.recvTimingResp(pkt); }

        void recvReqRetry() override { execute.recvReqRetry(); }

        bool isSnooping() const override { return true; }

        void recvTimingSnoopReq(PacketPtr pkt) override
        { return ; }

        //void recvFunctionalSnoop(PacketPtr pkt) override { }
    };

    DcachePort dcachePort;

    Execute(const std::string &name_,
        CClassCPU &cpu_,
        const BaseCClassCPUParams &params,
        Latch<ForwardInstData>::Output inp_,
        Latch<BranchData>::Input out_fetch1,
        Latch<BranchData>::Input out_fetch2,
        Latch<BranchData>::Input out_decode,
        Latch<ForwardInstData>::Input out_BASE,
        Latch<ForwardMemData>::Input out_MEMORY,
        Latch<ForwardInstData>::Input out_TRAP,
        Latch<ForwardInstData>::Input out_MBOX,
        Latch<ForwardInstData>::Input out_FBOX);

    ~Execute();

    public:

    /** Returns true if the given instruction is still tracked in inFlightInsts. */
    bool instIsHeadInst(CClassDynInstPtr inst);

    /** Pass on input/buffer data to the output if you can */
    void evaluate();

    unsigned int issue(ThreadID thread_id);

    /** After thread suspension, has Execute been drained of in-flight
     *  instructions and memory accesses. */
    bool isDrained();

    //unsigned int drain();

    //void drainResume();
     /* 
    struct IssueStats : public statistics::Group
    {
       IssueStats(CClassCPU *cpu);
       statistics::Vector2d issuedInstType;
    } issueStats;
      */

  public:
    Fault initiateMemRead(CClassDynInstPtr inst, Addr addr, unsigned int size,
        Request::Flags flags, const std::vector<bool> &byte_enable);
    Fault writeMem(CClassDynInstPtr inst, uint8_t *data, unsigned int size,
        Addr addr, Request::Flags flags, uint64_t *res,
        const std::vector<bool> &byte_enable);
    Fault initiateMemAMO(CClassDynInstPtr inst, Addr addr, unsigned int size,
        Request::Flags flags, AtomicOpFunctorPtr amo_op);

    void finishMemTranslation(ExecRequest *request, const Fault &fault);
    bool sendTimingMemReq(ExecRequest *request);
    bool recvTimingResp(PacketPtr pkt);
    void recvReqRetry();

  protected:
    ExecRequestPtr requestBeingIssued;
    ExecRequest *retryRequest = nullptr;
    std::map<std::pair<ThreadID, InstSeqNum>, ExecRequestPtr> pendingMemRequests;


};
}
}
#endif
