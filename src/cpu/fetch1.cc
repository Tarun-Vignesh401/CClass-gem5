#include "cpu/cclass/fetch1.hh"

#include <cstring>
#include <iomanip>
#include <sstream>

#include "arch/generic/decoder.hh"
#include "base/cast.hh"
#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "cpu/cclass/pipeline.hh"
#include "cpu/cclass/pipe_data.hh"
//#include "debug/Drain.hh"
#include "arch/riscv/pcstate.hh"
#include "debug/CClassFetch.hh"
#include "debug/CClassCPU.hh"


#include "cpu/cclass/buffers.hh"
//#include "debug/MinorTrace.hh"

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
            fetchInfo(params.numThreads),
            threadPriority(0),
            fetchLimit(params.fetch1FetchLimit)
        {
        for (auto &info: fetchInfo)
            info.pc.reset(params.isa[0]->newPCState());

        for (ThreadID i=0;i<params.numThreads;i++)
            fetchInfo[i].tid = i;            
            
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
    Fetch1::wakeupFetch(ThreadID tid)
    {
    ThreadContext *thread_ctx = cpu.getContext(tid);
    Fetch1ThreadInfo &thread = fetchInfo[tid];
    set(thread.pc, thread_ctx->pcState());
    thread.FetchAddr = thread.pc->instAddr();
    thread.state = PCGenRunning;
    thread.wakeupGuard = true;
    thread.makeValid();
    DPRINTF(CClassFetch, "[tid:%d]: Changing stream wakeup %s\n", tid, *thread.pc);

    cpu.wakeupOnEvent(Pipeline::Fetch1StageId);
    }
    void 
    Fetch1::advancepc(ThreadID tid){

        //advance the pc in thread context and copy it into fetchInfo for advancement into fetch2
        ThreadContext *thread_ctx = cpu.getContext(tid);
        Fetch1ThreadInfo &thread = fetchInfo[tid];
        RiscvISA::PCState pc = thread_ctx->pcState().as<RiscvISA::PCState>();
        pc.advance();
        thread_ctx->pcState(pc);
        set(thread.pc, thread_ctx->pcState());
        thread.FetchAddr = thread.pc->instAddr();
        thread.state = PCGenRunning;
        thread.wakeupGuard = true;
        thread.makeValid();

    }


/*
void updateExpectedSeqNums(const BranchData &branch)
{
    // sequence number change only when branch occurs pending implementation until branch predictor
}

*/
/*
void
Fetch1::changeStream(const BranchData &branch)
{
    Fetch1ThreadInfo &thread = fetchInfo[branch.threadId];

    updateExpectedSeqNums(branch);

     Start fetching again if we were stopped 
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
*/
void Fetch1::evaluate(){
        //once execute implemented(it is a latch timing involved)
        //const BranchData &execute_branch = *inp.outputWire;

        //once the branchpred is integrated just a wire 
        //const BranchData &branchPred = ;

        Fetch1ThreadInfo &out_thread = *out.inputWire;
      //for (ThreadID tid = 0; tid < cpu.numThreads; tid++)
      //for now single threaded support
    //advance architectural PC here 
    //std::cout<< "size of fetchInfo"<< fetchInfo.size() <<"\n";
    for (ThreadID tid = 0; tid < cpu.numThreads; tid++){
        fetchInfo[tid].blocked = !nextStageReserve[tid].canReserve();
        if(nextStageReserve[tid].canReserve()){
            nextStageReserve[tid].reserve();
            Fetch1::advancepc(tid);
            processResponse(out_thread,fetchInfo[tid]);
        }
        else{
            //DPRINTF(CClassCPU,"PC generation halted\n");
            fetchInfo[tid].state = PCGenHalted;
        }

    }



    //multi threading infrastructure
    /** Are both branches from later stages valid and for the same thread? */
    /*
    if (execute_branch.threadId != InvalidThreadID &&
        execute_branch.threadId == branchPred.threadId) {

        Fetch1ThreadInfo &thread = fetchInfo[execute_branch.threadId];

        /* Are we changing stream?  Look to the Execute branches first, then
         * to predicted changes of stream from Fetch2 
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
             *  discarding those requests when we get to them. 
        } else if (thread.state != PCGenHalted && branchPred.isStreamChange()) {
            /* Handle branch predictions by changing the instruction source
             * if we're still processing the same stream (as set by streamSeqNum)
             * as the one of the prediction.
             
            if (branchPred.newStreamSeqNum != thread.streamSeqNum) {
                DPRINTF(Fetch1, "Not changing stream on prediction: %s,"
                    " streamSeqNum mismatch\n",
                    branchPred);
            } else {
                changeStream(branchPred);
            }
        }
    } else {
        /* Fetch2 and Execute branches are for different threads 
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
    */
    // have to make sure to that there aren't too many fetches going on.
    //std::cout<<"dummy\n";
     
    }


void Fetch1::processResponse(Fetch1ThreadInfo &out, Fetch1ThreadInfo &thread){
   
    out.state = thread.state;
    out.pc.reset(thread.pc->clone());
    out.streamSeqNum = thread.streamSeqNum;
    out.predictionSeqNum = thread.predictionSeqNum;
    out.blocked = thread.blocked;
    out.FetchAddr = thread.FetchAddr;
    out.tid = thread.tid; 
    if(thread.pc){
    out.makeValid();
    }    
    //std::cout << "Fetch1 processResponse called \n";
    DPRINTF(CClassCPU,
    " Fetch 1 stage: processResponse: state=%d streamSeq=%llu predSeq=%llu "
    "blocked=%d fetchAddr=%#lx Bubble : %d\n",
    out.state,
    out.streamSeqNum,
    out.predictionSeqNum,
    out.blocked,
    out.pc->instAddr(),
    out.bubbleFlag);
    
}
  } // namespace cclass
} //namespace gem5