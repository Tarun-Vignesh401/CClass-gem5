#include "cpu/cclass/execute.hh"
#include <functional>


/*
#include "cpu/CClass/cpu.hh"
#include "cpu/CClass/exec_context.hh"
#include "cpu/CClass/fetch1.hh"
#include "cpu/CClass/lsq.hh"
#include "cpu/op_class.hh"
#include "debug/Activity.hh"
#include "debug/Branch.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/CClassCPU.hh"
#include "debug/CClassInterrupt.hh"
#include "debug/CClassCPU.hh"nstages
#include "debug/CClassTrace.hh"
#include "debug/PCEvent.hh"
*/


#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/exec_context.hh"
#include "cpu/cclass/fetch1.hh"
#include "cpu/op_class.hh"
#include "debug/Activity.hh"
#include "debug/Branch.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/CClassCPU.hh"
#include"cpu/cclass/pipe_data.hh"
//#include "debug/CClassTrace.hh"
#include "debug/PCEvent.hh"
#include"cpu/cclass/pipeline.hh"
namespace gem5{

namespace cclass{



Execute::Execute(const std::string &name_, CClassCPU &cpu_,
                 const BaseCClassCPUParams &params,
                 Latch<ForwardInstData>::Output inp_, 
                 Latch<BranchData>::Input out_fetch1,
                 Latch<BranchData>::Input out_fetch2,
                 Latch<BranchData>::Input out_decode,
                 Latch<ForwardInstData>::Input out_mem, 
                 std::vector<InputBuffer<ForwardInstData>> &next_stage_input_buffer):
      Named(name_),
      inp(inp_),
      out_fetch1(out_fetch1),
      out_fetch2(out_fetch2),
      out_decode(out_decode),
      out_mem(out_mem),
      nextStageReserve(next_stage_input_buffer),
      cpu(cpu_),
      issueLimit(params.executeIssueLimit),
      memoryIssueLimit(params.executeMemoryIssueLimit),
      InputBufferSize(params.executeInputBufferLimit),
      fuDescriptions(*params.executeFuncUnits),
      numFuncUnits(fuDescriptions.funcUnits.size()),
      allowEarlyMemIssue(params.executeAllowEarlyMemoryIssue),
      noCostFUIndex(fuDescriptions.funcUnits.size() + 1),
      executeInfo(params.numThreads,ExecuteThreadInfo(params.executeIssueLimit)),
      dcachePort(cpu.name() + ".dcache_port", *this, cpu)
      //issueStats(&cpu_)
{
for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        std::string tid_str = std::to_string(tid);

        /* Input Buffers */
        inputBuffer.push_back(
            InputBuffer<ForwardInstData>(
                name_ + ".inputBuffer" + tid_str, "insts",
                params.executeInputBufferSize));
}


  /* This should be large enough to count all the in-FU instructions
     *  which need to be accounted for in the inFlightInsts
     *  queue */
    unsigned int total_slots = 0;

    /* Make FUPipelines for each MinorFU */
    for (unsigned int i = 0; i < numFuncUnits; i++) {
        std::ostringstream fu_name;
        CClassFU *fu_description = fuDescriptions.funcUnits[i];

        /* Note the total number of instruction slots (for sizing
         *  the inFlightInst queue) and the maximum latency of any FU
         *  (for sizing the activity recorder) */
        total_slots += fu_description->opLat;

        fu_name << name_ << ".fu." << i;

        FUPipeline *fu = new FUPipeline(fu_name.str(), *fu_description, cpu);

        funcUnits.push_back(fu);
    }

    /** Check that there is a functional unit for all operation classes */
    for (int op_class = No_OpClass + 1; op_class < Num_OpClasses; op_class++) {
        bool found_fu = false;
        unsigned int fu_index = 0;

        while (fu_index < numFuncUnits && !found_fu)
        {
            if (funcUnits[fu_index]->provides(
                static_cast<OpClass>(op_class)))
            {
                found_fu = true;
            }
            fu_index++;
        }

        if (!found_fu) {
            warn("No functional unit for OpClass %s\n",
                enums::OpClassStrings[op_class]);
        }
    }

    /* Per-thread structures */
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        std::string tid_str = std::to_string(tid);

        /* Input Buffers */
        inputBuffer.push_back(
            InputBuffer<ForwardInstData>(
                name_ + ".inputBuffer" + tid_str, "insts",
                params.executeInputBufferSize));

        const auto &regClasses = cpu.threads[tid]->getIsaPtr()->regClasses();

        /* Scoreboards */
        scoreboard.emplace_back(name_ + ".scoreboard" + tid_str, regClasses);

        /* In-flight instruction records */
        executeInfo[tid].inFlightInsts =  new Queue<QueuedInst,
            ReportTraitsAdaptor<QueuedInst> >(
            name_ + ".inFlightInsts" + tid_str, "insts", total_slots);

        executeInfo[tid].inFUMemInsts = new Queue<QueuedInst,
            ReportTraitsAdaptor<QueuedInst> >(
            name_ + ".inFUMemInsts" + tid_str, "insts", total_slots);
    }
}

void
Execute::popInput(ThreadID tid)
{
    if (!inputBuffer[tid].empty())
        inputBuffer[tid].pop();

}

void Execute::evaluate(){

    
    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->threadId].setTail(*inp.outputWire);

    BranchData &branch_fetch1 = *out_fetch1.inputWire;
    BranchData &branch_fetch2 = *out_fetch2.inputWire;
    BranchData &branch_decode = *out_decode.inputWire;

   // ForwardInstData &insts_out = *out_mem.inputWire;

    //std::vector<ForwardingSource> mem_exe_forwards;
    //std::vector<ForwardingSource> mem_wb_forwards;

    static unsigned int output_index = 0;

    unsigned int num_issued = 0;
    
    bool becoming_stalled = true;

    bool can_issue_next = false;

    // for each cycle one issuing thread...

    ThreadID issue_tid = getIssuingThread();
    ExecuteThreadInfo& thread = executeInfo[issue_tid];

    for(unsigned int i=0; i < numFuncUnits; i++){
    FUPipeline *fu = funcUnits[i];
    if(issue_tid != InvalidThreadID){
        
        if(thread.drainState != NotDraining){
             if(thread.drainState == DrainHaltFetch){
                updateBranchData(issue_tid,BranchData::HaltFetch,
                CClassDynInst::bubble(),cpu.getContext(issue_tid)->pcState(),branch_fetch1);
                updateBranchData(issue_tid,BranchData::HaltFetch,
                CClassDynInst::bubble(),cpu.getContext(issue_tid)->pcState(),branch_fetch2);
                updateBranchData(issue_tid,BranchData::HaltFetch,
                CClassDynInst::bubble(),cpu.getContext(issue_tid)->pcState(),branch_decode);
                
                cpu.wakeupOnEvent(Pipeline::ExecuteStageId);
                setDrainState(issue_tid,DrainAllInsts);

            }   
            else if(thread.drainState == DrainAllInsts){
                
                while(getInput(issue_tid)){
                    popInput(issue_tid);
                }
                fu->advance();
            }
        }
        else {
            //DPRINTF(CClassCPU,"Trying to Issuing instruction for tid:%d\n",issue_tid);
            num_issued = issue(issue_tid); 
            fu->advance();
        }
        // for ticking again..
      
    }
    }// for loop ends..
    std::vector<CClassDynInstPtr> next_issuable_insts;

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        /* Find the next issuable instruction for each thread and see if it can
           be issued */
        if (getInput(tid)) {
            unsigned int input_index = executeInfo[tid].inputIndex;
            CClassDynInstPtr inst = getInput(tid)->insts[input_index];
            if (inst->isFault()) {
                can_issue_next = true;
            } else if (!inst->isBubble()) {
                next_issuable_insts.push_back(inst);
            }
        }
    }
    // start reserving the next stage buffer based on the operation
    //branch handling stuff below--> 
       /*if(inst->staticInst->isControl){
            //branches are executed in this stage.. not writeback!!!
            // make branches as bubbles and send them to the next stage
            bool committed = commit_branch(inst,issue_tid);
            if(committed){
                insts_out.insts[output_index] = CClassDynInstPtr::bubble();
                output_index++;
            }
        }*/

    trytoPush(issue_tid,thread.outputIndex);
    // for resolving mem requests to dcache...
   // for(i = 0; i<sizeof(inFUMemInsts); i++){
     //   inst = inFUMemInsts[i].inst;


    //}
    // evaluating if we have to tick again or not..
    for (int j = 0; j< numFuncUnits; j++){
        FUPipeline *fu_pipe = funcUnits[j];
        // this is for if the execute stage should be awake the next cycle
        if (fu_pipe->occupancy !=0 && !fu_pipe->stalled)
            becoming_stalled = false;

        for (auto inst : next_issuable_insts) {
            if (!fu_pipe->stalled && fu_pipe->provides(inst->staticInst->opClass()) &&
                scoreboard[inst->id.threadId].canInstIssue(inst,
                    NULL, NULL, cpu.curCycle() + Cycles(1),
                    cpu.getContext(inst->id.threadId))) {
                can_issue_next = true;
                break;
            }
        }

    }
    /*bool need_to_tick = num_issued != 0 || !becoming_stalled || can_issue_next;

    if(!need_to_tick){
        DPRINTF(CClassCPU,"Next Cycle might be skippable!! \n");
    }
    else*/ 
        cpu.wakeupOnEvent(Pipeline::ExecuteStageId);

    /* Make sure the input (if any left) is pushed */
    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->threadId].pushTail();
    

}//evaluate end..

const ForwardInstData *
Execute::getInput(ThreadID tid){
        /* Get a line from the inputBuffer to work with */
    if (!inputBuffer[tid].empty()) {
        const ForwardInstData &head = inputBuffer[tid].front();

        return (head.isBubble() ? NULL : &(inputBuffer[tid].front()));
    } else {
        return NULL;
    } 
}



unsigned int
Execute::issue(ThreadID thread_id)
{
    const ForwardInstData *insts_in = getInput(thread_id);
    ExecuteThreadInfo &thread = executeInfo[thread_id];

    /* Early termination if we have no instructions */
    if (!insts_in)
        return 0;

    /* Start from the first FU */
    unsigned int fu_index = 0;

    /* Remains true while instructions are still being issued.  If any
     *  instruction fails to issue, this is set to false and we exit issue.
     *  This strictly enforces in-order issue.  For other issue behaviours,
     *  a more complicated test in the outer while loop below is needed. */
    bool issued = true;

    /* Number of insts issues this cycle to check for issueLimit */
    unsigned num_insts_issued = 0;

    /* Number of memory ops issues this cycle to check for memoryIssueLimit */
    unsigned num_mem_insts_issued = 0;

    do {
        CClassDynInstPtr inst = insts_in->insts[thread.inputIndex];
        Fault fault = inst->fault;
        bool discarded = false;
        bool issued_mem_ref = false;
        // all of the stuff that doesn't require the functional units to handle..

        if (inst->isBubble()) {
            /* Skip */
            issued = true;
        } else if (cpu.getContext(thread_id)->status() == ThreadContext::Suspended)
        {
            DPRINTF(CClassCPU, "Discarding inst: %s from suspended"
                " thread\n", *inst);

            issued = true;
            discarded = true;
        } else if (inst->id.streamSeqNum != thread.streamSeqNum) {
            DPRINTF(CClassCPU, "Discarding inst: %s as its stream"
                " state was unexpected, expected: %d\n",
                *inst, thread.streamSeqNum);
            issued = true;
            discarded = true;
        } else {
            /* Try and issue an instruction into an FU, assume we didn't and
             * fix that in the loop */
            issued = false;

            /* Try FU from 0 each instruction */
            fu_index = 0;

            /* Try and issue a single instruction stepping through the
             *  available FUs */
            do {
                FUPipeline *fu = funcUnits[fu_index];

                DPRINTF(CClassCPU, "Trying to issue inst: %s to FU: %d\n",
                    *inst, fu_index);

                /* Does the examined fu have the OpClass-related capability
                 *  needed to execute this instruction?  Faults can always
                 *  issue to any FU but probably should just 'live' in the
                 *  inFlightInsts queue rather than having an FU. */
                bool fu_is_capable = (!inst->isFault() ?
                    fu->provides(inst->staticInst->opClass()) : true);

                if (inst->isNoCostInst()) {
                    /* Issue free insts. to a fake numbered FU */
                    fu_index = noCostFUIndex;

                    /* And start the countdown on activity to allow
                     *  this instruction to get to the end of its FU */
                    //cpu.activityRecorder->activity();

                    /* Mark the destinations for this instruction as
                     *  busy */
                    scoreboard[thread_id].markupInstDests(inst, cpu.curCycle() +
                        Cycles(0), cpu.getContext(thread_id), false);

                    DPRINTF(CClassCPU, "Issuing %s to %d\n", inst->id, noCostFUIndex);
                    inst->fuIndex = noCostFUIndex;
                    inst->extraCommitDelay = Cycles(0);
                    inst->extraCommitDelayExpr = NULL;

                    /* Push the instruction onto the nextStage queue so
                     *  it can be committed in order */
                    QueuedInst fu_inst(inst);
                    thread.inFlightInsts->push(fu_inst);

                    issued = true;

                } else if (!fu_is_capable || fu->alreadyPushed()) {
                    /* Skip */
                    if (!fu_is_capable) {
                        DPRINTF(CClassCPU, "Can't issue as FU: %d isn't"
                            " capable\n", fu_index);
                    } else {
                        DPRINTF(CClassCPU, "Can't issue as FU: %d is"
                            " already busy\n", fu_index);
                    }
                } else if (fu->stalled) {
                    DPRINTF(CClassCPU, "Can't issue inst: %s into FU: %d,"
                        " it's stalled\n",
                        *inst, fu_index);
                } else if (!fu->canInsert()) {
                    DPRINTF(CClassCPU, "Can't issue inst: %s to busy FU"
                        " for another: %d cycles\n",
                        *inst, fu->cyclesBeforeInsert());
                } else {
                    CClassFUTiming *timing = (!inst->isFault() ?
                        fu->findTiming(inst->staticInst) : NULL);

                    const std::vector<Cycles> *src_latencies =
                        (timing ? &(timing->srcRegsRelativeLats)
                            : NULL);

                    const std::vector<bool> *cant_forward_from_fu_indices =
                        &(fu->cantForwardFromFUIndices);

                    if (timing && timing->suppress) {
                        DPRINTF(CClassCPU, "Can't issue inst: %s as extra"
                            " decoding is suppressing it\n",
                            *inst);
                    } else if (!scoreboard[thread_id].canInstIssue(inst,
                        src_latencies, cant_forward_from_fu_indices,
                        cpu.curCycle(), cpu.getContext(thread_id)))
                    {
                        DPRINTF(CClassCPU, "Can't issue inst: %s yet\n",
                            *inst);
                    } else {
                        /* Can insert the instruction into this FU */
                        DPRINTF(CClassCPU, "Issuing inst: %s"
                            " into FU %d\n", *inst,
                            fu_index);
                        // Update ALU access stats.
                        if (!inst->isFault()) {
                            auto tid = thread_id;
                            if (inst->staticInst->isInteger()) {
                                cpu.executeStats[tid]->numIntAluAccesses++;
                            }
                            if (inst->staticInst->isFloating()) {
                                cpu.executeStats[tid]->numFpAluAccesses++;
                            }
                            if (inst->staticInst->isVector()) {
                                cpu.executeStats[tid]->numVecAluAccesses++;
                            }
                        }
                        Cycles extra_dest_retire_lat = Cycles(0);
                        TimingExpr *extra_dest_retire_lat_expr = NULL;
                        Cycles extra_assumed_lat = Cycles(0);

                        /* Add the extraCommitDelay and extraAssumeLat to
                         *  the FU pipeline timings */
                        if (timing) {
                            extra_dest_retire_lat =
                                timing->extraCommitLat;
                            extra_dest_retire_lat_expr =
                                timing->extraCommitLatExpr;
                            extra_assumed_lat =
                                timing->extraAssumedLat;
                        }

                        issued_mem_ref = inst->isMemRef();

                        QueuedInst fu_inst(inst);

                        /* Decorate the inst with FU details */
                        inst->fuIndex = fu_index;
                        inst->extraCommitDelay = extra_dest_retire_lat;
                        inst->extraCommitDelayExpr =
                            extra_dest_retire_lat_expr;

                        // letting to pass through the fu pipeline...
                        if (issued_mem_ref) {
                            /* Remember which instruction this memory op
                             *  depends on so that initiateAcc can be called
                             *  early */
                            if (allowEarlyMemIssue) {
                                inst->instToWaitFor =
                                    scoreboard[thread_id].execSeqNumToWaitFor(inst,
                                        cpu.getContext(thread_id));

                                //if (lastMemBarrier(thread_id) >
                                  //  inst->instToWaitFor)
                                //{
                                  //  DPRINTF(CClassCPU, "A barrier will"
                                    //    " cause a delay in mem ref issue of"
                                      //  " inst: %s until after inst"
                                        //" %d(exec)\n", *inst,
                                        //lastMemBarrier(thread_id));

                                    //inst->instToWaitFor =
                                      //  lastMemBarrier(thread_id);
                                //} else {
                                    DPRINTF(CClassCPU, "Memory ref inst:"
                                        " %s must wait for inst %d(exec)"
                                        " before issuing\n",
                                        *inst, inst->instToWaitFor);
                                //}

                                inst->canEarlyIssue = true;
                            }
                            /* Also queue this instruction in the memory ref
                             *  queue to ensure in-order issue to the LSQ */
                            DPRINTF(CClassCPU, "Pushing mem inst: %s\n",
                                *inst);
                            thread.inFUMemInsts->push(fu_inst);
                        }

                        /* Update the # of insts. issued per OpClass type */
                        if (!inst->isFault()) {
                            auto opclass = inst->staticInst->opClass();
                            //issueStats.issuedInstType[thread_id][opclass]++;
                        }

                        /* Issue to FU */
                        fu->push(fu_inst);
                        /* And start the countdown on activity to allow
                         *  this instruction to get to the end of its FU */
                        //cpu.activityRecorder->activity();

                        /* Mark the destinations for this instruction as
                         *  busy */
                        scoreboard[thread_id].markupInstDests(inst, cpu.curCycle() +
                            fu->description.opLat +
                            extra_dest_retire_lat +
                            extra_assumed_lat,
                            cpu.getContext(thread_id),
                            issued_mem_ref && extra_assumed_lat == Cycles(0));

                        /* Push the instruction onto the nextStage queue so
                         *  it can be committed in order */
                        thread.inFlightInsts->push(fu_inst);

                        issued = true;
                    }
                }

                fu_index++;
            } while (fu_index != numFuncUnits && !issued);

            if (!issued)
                DPRINTF(CClassCPU, "Didn't issue inst: %s\n", *inst);
        }

        if (issued) {
            /* Generate CClassTrace's CClassInst lines.  Do this at commit
             *  to allow better instruction annotation? */
            /*    
             if (debug::CClassTrace && !inst->isBubble()) {
                inst->cclassTraceInst(*this);
            }*/

            /* Mark up barriers */
            if (!discarded && inst->isInst() &&
                inst->staticInst->isFullMemBarrier())
            {
                DPRINTF(CClassCPU, "Issuing memory barrier inst: %s\n", *inst);
                // all of membarrier funcitonality disabled for now..
                //issuedMemBarrierInst(inst);
            }

            if (inst->traceData && setTraceTimeOnIssue) {
                inst->traceData->setWhen(curTick());
            }

            if (issued_mem_ref)
                num_mem_insts_issued++;

            if (!discarded && !inst->isBubble()) {
                num_insts_issued++;

                if (num_insts_issued == issueLimit)
                    DPRINTF(CClassCPU, "Reached inst issue limit\n");
            }

            thread.inputIndex++;
            DPRINTF(CClassCPU, "Stepping to next inst inputIndex: %d\n",
                thread.inputIndex);
        }

        /* Got to the end of a line */
        if (thread.inputIndex == insts_in->width()) {
            popInput(thread_id);
            /* Set insts_in to null to force us to leave the surrounding
             *  loop */
            insts_in = NULL;

            if (processMoreThanOneInput) {
                DPRINTF(CClassCPU, "Wrapping\n");
                insts_in = getInput(thread_id);
            }
        }
    } while (insts_in && thread.inputIndex < insts_in->width() &&
        /* We still have instructions */
        fu_index != numFuncUnits && /* Not visited all FUs */
        issued && /* We've not yet failed to issue an instruction */
        num_insts_issued != issueLimit && /* Still allowed to issue */
        num_mem_insts_issued != memoryIssueLimit);

    return num_insts_issued;
}


void
Execute::trytoPush(ThreadID tid, unsigned int output_index){
    ExecuteThreadInfo &thread = executeInfo[tid];
    ForwardInstData &insts_out = *out_mem.inputWire;

    if (thread.inFlightInsts->empty())
        return;

    QueuedInst &head = thread.inFlightInsts->front();
    CClassDynInstPtr inst = head.inst;

    if (inst->isBubble())
        return;

    if (!nextStageReserve[tid].canReserve())
        return;

    bool inst_ready = false; 
    bool isbubble = inst->isNoCostInst();

    if (!isbubble) {
        FUPipeline *fu = funcUnits[inst->fuIndex];
        QueuedInst &fu_head = fu->front();

        inst_ready = !fu_head.inst->isBubble() &&
            fu_head.inst->id.execSeqNum == inst->id.execSeqNum &&
            fu_head.inst->id == inst->id;
        
        if(inst_ready){
            nextStageReserve[tid].reserve();
            insts_out.insts[output_index] = inst;
            thread.inFlightInsts->pop();
            // unstall the corresponding fupipeline..
            if (inst->fuIndex != noCostFUIndex)
                funcUnits[inst->fuIndex]->stalled = false;
        }
        else return;
    }
    
    else{
        nextStageReserve[tid].reserve();
        insts_out.insts[output_index] = inst;
        thread.inFlightInsts->pop();
        // unstall the corresponding fupipeline..
        if (inst->fuIndex != noCostFUIndex)
            funcUnits[inst->fuIndex]->stalled = false;  
    }
}

/*
void
Execute::issuedMemBarrierInst(CClassDynInstPtr inst)
{
    assert(inst->isInst() && inst->staticInst->isFullMemBarrier());
    assert(inst->id.execSeqNum > lastMemBarrier[inst->id.threadId]);

    /* Remember the barrier.  We only have a notion of one
     *  barrier so this may result in some mem refs being
     *  delayed if they are between barriers 
    lastMemBarrier[inst->id.threadId] = inst->id.execSeqNum;
}*/

/*
void
Execute::recvTimingSnoopReq(PacketPtr pkt)
{
    /* LLSC operations in Minor can't be speculative and are executed from
     * the head of the requests queue.  We shouldn't need to do more than
     * this action on snoops. 
    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        if (cpu.getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu.wakeup(tid);
        }
    }

    if (pkt->isInvalidate() || pkt->isWrite()) {
        for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
            cpu.getContext(tid)->getIsaPtr()->handleLockedSnoop(
                    pkt, cacheBlockMask);
        }
    }
}
void
Execute::recvTimingSnoopReq(PacketPtr pkt)
{
    /* LLSC operations in Minor can't be speculative and are executed from
     * the head of the requests queue.  We shouldn't need to do more than
     * this action on snoops. 
    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        if (cpu.getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu.wakeup(tid);
        }
    }

    if (pkt->isInvalidate() || pkt->isWrite()) {
        for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
            cpu.getContext(tid)->getIsaPtr()->handleLockedSnoop(
                    pkt, cacheBlockMask);
        }
    }
}

void
Execute::recvReqRetry()
{
    DPRINTF(CClassCPU, "Received retry request\n");

    assert(state == MemoryNeedsRetry);

    switch (retryRequest->state) {
      case LSQRequest::RequestNeedsRetry:
        /* Retry in the requests queue 
        retryRequest->setState(LSQRequest::Translated);
        break;
      case LSQRequest::StoreBufferNeedsRetry:
        /* Retry in the store buffer 
        retryRequest->setState(LSQRequest::StoreInStoreBuffer);
        break;
      default:
        panic("Unrecognized retry request state %d.", retryRequest->state);
    }

    /* Set state back to MemoryRunning so that the following
     *  tryToSend can actually send.  Note that this won't
     *  allow another transfer in as tryToSend should
     *  issue a memory request and either succeed for this
     *  request or return the LSQ back to MemoryNeedsRetry 
    state = MemoryRunning;

    /* Try to resend the request 
    if (tryToSend(retryRequest)) {
        /* Successfully sent, need to move the request 
        switch (retryRequest->state) {
          case LSQRequest::RequestIssuing:
            /* In the requests queue 
            moveFromRequestsToTransfers(retryRequest);
            break;
          case LSQRequest::StoreBufferIssuing:
            /* In the store buffer 
            storeBuffer.countIssuedStore(retryRequest);
            break;
          default:
            panic("Unrecognized retry request state %d.", retryRequest->state);
        }

        retryRequest = NULL;
    }
}
*/
// figure out if it is ready to push.. 
//I don't need it currently it is hard coded..
bool
Execute::instIsHeadInst(CClassDynInstPtr inst)
{
    bool ret = false;

    if (!executeInfo[inst->id.threadId].inFlightInsts->empty())
        ret = executeInfo[inst->id.threadId].inFlightInsts->front().inst->id == inst->id;

    return ret;
}

bool
Execute::isDrained()
{

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        if (!inputBuffer[tid].empty() ||
            !executeInfo[tid].inFlightInsts->empty()) {

            return false;
        }
    }

    return true;
}


void
Execute::setDrainState(ThreadID thread_id, DrainState state)
{
    DPRINTF(Drain, "setDrainState[%d]: %s\n", thread_id, state);
    executeInfo[thread_id].drainState = state;
}


inline ThreadID
Execute::getIssuingThread()
{
  return 0;
}


void
Execute::updateBranchData(
    ThreadID tid,
    BranchData::Reason reason,
    CClassDynInstPtr inst, const PCStateBase &target,
    BranchData &branch)
{
    if (reason != BranchData::NoBranch) {
        /* Bump up the stream sequence number on a real branch*/
        if (BranchData::isStreamChange(reason))
            executeInfo[tid].streamSeqNum++;

        /* Branches (even mis-predictions) don't change the predictionSeqNum,
         *  just the streamSeqNum */
        branch = BranchData(reason, tid,
            executeInfo[tid].streamSeqNum,
            /* Maintaining predictionSeqNum if there's no inst is just a
             * courtesy and looks better on minorview */
            (inst->isBubble() ? executeInfo[tid].lastPredictionSeqNum
                : inst->id.predictionSeqNum),
            target, inst);

        DPRINTF(Branch, "Branch data signalled: %s\n", branch);
    }
}

Execute::~Execute()
{
    for (unsigned int i = 0; i < numFuncUnits; i++)
        delete funcUnits[i];

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++)
        delete executeInfo[tid].inFlightInsts;
}


/*Execute::IssueStats::IssueStats(CClassCPU *cpu)
    : statistics::Group(cpu),
      ADD_STAT(issuedInstType, statistics::units::Count::get(),
               "Number of instructions issued per FU type, per thread")
{
    issuedInstType.init(cpu->numThreads, enums::Num_OpClass)
        .flags(statistics::total | statistics::pdf | statistics::dist);
    issuedInstType.ysubnames(enums::OpClassStrings);
}*/
}//namespace cclass
}//namespace gem5
