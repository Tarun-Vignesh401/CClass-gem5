namespace gem5
{
    namespace cclass
    {

        Fetch1::Fetch1(const std::string &name_, 
            CClassCPU &cpu_,
            const BaseCClassCPUParams &params,
            Latch<Fetch1ThreadInfo>::Input out_,
            std::vector<InputBuffer<Fetch1ThreadInfo>> &next_stage_input_buffer) :
            Named(name_),
            cpu(cpu_),
            nextStageReserve(next_stage_input_buffer),
            out(out_),
            fetchLimit(params.fetch1FetchLimit)
        {
        for (auto &info: fetchInfo)
            info.pc.reset(params.isa[0]->newPCState());
            
            
        if (fetchLimit < 1) {
        fatal("%s: fetch1FetchLimit must be >= 1 (%d)\n", name_,
            fetchLimit);
        }
        
    }
    ThreadID Fetch1::getScheduledThread(){

        //for now only one thread can be extended for multithreading support later
        return 0;
    }

void
Fetch1::changeStream(const BranchData &branch)
{
    Fetch1ThreadInfo &thread = fetchInfo[branch.threadId];

    updateExpectedSeqNums(branch);

    /* Start fetching again if we were stopped */
    switch (branch.reason) {
      case BranchData::SuspendThread:
        {
            if (thread.wakeupGuard) {
                DPRINTF(Fetch, "Not suspending fetch due to guard: %s\n",
                                branch);
            } else {
                DPRINTF(Fetch, "Suspending fetch: %s\n", branch);
                thread.state = PCWaitingForChange;
            }
        }
        break;
      case BranchData::HaltFetch:
        DPRINTF(Fetch, "Halting fetch\n");
        thread.state = PCGenHalted;
        break;
      default:
        DPRINTF(Fetch, "Changing stream on branch: %s\n", branch);
        thread.state = PCGenRunning;
        break;
    }
    set(thread.pc, branch.target);
    thread.fetchAddr = thread.pc->instAddr();
}

void evaluate(){
        //once execute implemented(it is a latch timing involved)
        //const BranchData &execute_branch = *inp.outputWire;

        //once the branchpred is integrated just a wire 
        //const BranchData &branchPred = ;

        Fetch1ThreadInfo &out_thread = *out.inputWire;

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++)
        fetchInfo[tid].blocked = !nextStageReserve[tid].canReserve();

    //multi threading infrastructure
    /** Are both branches from later stages valid and for the same thread? */
    if (execute_branch.threadId != InvalidThreadID &&
        execute_branch.threadId == branchPred.threadId) {

        Fetch1ThreadInfo &thread = fetchInfo[execute_branch.threadId];

        /* Are we changing stream?  Look to the Execute branches first, then
         * to predicted changes of stream from Fetch2 */
        if (execute_branch.isStreamChange()) {
            if (thread.state == PCGenHalted) {
                DPRINTF(Fetch1, "Halted, ignoring branch: %s\n", execute_branch);
            } else {
                changeStream(execute_branch);
            }

            if (!branchPred.isBubble()) {
                DPRINTF(Fetch1, "Ignoring simultaneous prediction: %s\n",
                    branchPred);
            }

            /* The streamSeqNum tagging in request/response ->req should handle
             *  discarding those requests when we get to them. */
        } else if (thread.state != PCGenHalted && branchPred.isStreamChange()) {
            /* Handle branch predictions by changing the instruction source
             * if we're still processing the same stream (as set by streamSeqNum)
             * as the one of the prediction.
             */
            if (branchPred.newStreamSeqNum != thread.streamSeqNum) {
                DPRINTF(Fetch1, "Not changing stream on prediction: %s,"
                    " streamSeqNum mismatch\n",
                    branchPred);
            } else {
                changeStream(branchPred);
            }
        }
    } else {
        /* Fetch2 and Execute branches are for different threads */
        if (execute_branch.threadId != InvalidThreadID &&
            execute_branch.isStreamChange()) {

            if (fetchInfo[execute_branch.threadId].state == PCGenHalted) {
                DPRINTF(Fetch, "Halted, ignoring branch: %s\n", execute_branch);
            } else {
                changeStream(execute_branch);
            }
        }

        if (branchPred.threadId != InvalidThreadID &&
            branchPred.isStreamChange()) {

            if (fetchInfo[branchPred.threadId].state == PCGenHalted) {
                DPRINTF(Fetch1, "Halted, ignoring branch: %s\n", branchPred);
            } else if (branchPred.newStreamSeqNum != fetchInfo[branchPred.threadId].streamSeqNum) {
                DPRINTF(Fetch1, "Not changing stream on prediction: %s,"
                    " streamSeqNum mismatch\n", branchPred);
            } else {
                changeStream(branchPred);
            }
        }
    }
    // have to make sure to that there aren't too many fetches going on.
        if(nextstageReserve[0].canReserve() < fetchLimit){
            nextstageReserve[0].reserve();
            processResponse(out_thread,fetchInfo[0]);
        }


    }


void processResponse(Fetch1ThreadInfo &out, Fetch1ThreadInfo &thread){
    out.state = thread.state;
    out.pc = thread.pc->clone();
    out.streamSeqNum = thread.streamSeqNum;
    out.predictionSeqNum = thread.predictionSeqNum;
    out.blocked = thread.blocked;
    out.fetchAddr = thread.fetchAddr;        
    }



    
  } // namespace cclass
} //namespace gem5