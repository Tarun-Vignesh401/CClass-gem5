#include "cpu/cclass/execute.hh"
#include <algorithm>
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
#include "base/cast.hh"
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
                 Latch<ForwardInstData>::Input out_BASE,
                 Latch<ForwardMemData>::Input out_MEMORY,
                 Latch<ForwardInstData>::Input out_TRAP,
                 Latch<ForwardInstData>::Input out_MBOX,
                 Latch<ForwardInstData>::Input out_FBOX):
      Named(name_),
      inp(inp_),
      out_fetch1(out_fetch1),
      out_fetch2(out_fetch2),
      out_decode(out_decode),
      out_BASE(out_BASE),
      out_MEMORY(out_MEMORY),
      out_TRAP(out_TRAP),
      out_MBOX(out_MBOX),
      out_FBOX(out_FBOX),
      cpu(cpu_),
      stalled(false),
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
        executeInfo[tid].inFlightInsts.reserve(total_slots);

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
    
    executeInfo[tid].inputIndex = 0;

}

void Execute::evaluate(){

    
    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->threadId].setTail(*inp.outputWire);

    BranchData &branch_fetch1 = *out_fetch1.inputWire;
    BranchData &branch_fetch2 = *out_fetch2.inputWire;
    BranchData &branch_decode = *out_decode.inputWire;

    //std::vector<ForwardingSource> mem_exe_forwards;
    //std::vector<ForwardingSource> mem_wb_forwards;

    //static unsigned int output_index = 0;

    unsigned int num_issued = 0;
    
    bool becoming_stalled = true;

    bool can_issue_next = false;

    // for each cycle one issuing thread...

    ThreadID issue_tid = getIssuingThread();
    ExecuteThreadInfo& thread = executeInfo[issue_tid];
    resetISBOutputIndexes(issue_tid);

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

    trytoPush(issue_tid);
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

                    /* Track this instruction until it is ready to push onward. */
                    QueuedInst fu_inst(inst);
                    thread.inFlightInsts.push_back(fu_inst);

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

                        /* Track this instruction until it is ready to push onward. */
                        thread.inFlightInsts.push_back(fu_inst);

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
Execute::trytoPush(ThreadID tid){
    ExecuteThreadInfo &thread = executeInfo[tid];

    if (thread.inFlightInsts.empty())
        return;

    for (auto it = thread.inFlightInsts.begin();
        it != thread.inFlightInsts.end(); )
    {
        CClassDynInstPtr inst = it->inst;

        if (inst->isBubble()) {
            it = thread.inFlightInsts.erase(it);
            continue;
        }

        if (inst->isFault() || inst->isNoCostInst()) {
            if (pushInstToLatch(tid, inst)) {
                it = thread.inFlightInsts.erase(it);
                continue;
            }

            ++it;
            continue;
        }

        if (inst->fuIndex >= numFuncUnits) {
            DPRINTF(CClassCPU,
                "Skipping unallocated in-flight inst: %s fuIndex=%u\n",
                *inst, inst->fuIndex);
            it->inst = CClassDynInst::bubble();
            ++it;
            continue;
        }

        FUPipeline *fu = funcUnits[inst->fuIndex];
        QueuedInst &fu_head = fu->front();
        bool inst_ready = !fu_head.inst->isBubble() &&
            fu_head.inst->id.execSeqNum == inst->id.execSeqNum &&
            fu_head.inst->id == inst->id;

        if (inst_ready) {
            if (inst->isInst() && inst->staticInst->isControl())
                handleBranch(tid, inst);
            // control instruction handling...
            if (inst->isMemRef()) {
                if (!out_MEMORY.inputWire->isBubble()) {
                    ++it;
                    continue;
                }

                auto request_key = std::make_pair(tid, inst->id.execSeqNum);
                ExecRequestPtr mem_request;
                auto request_it = pendingMemRequests.find(request_key);
                // if key isn't found it goes to end of the map.
                if (request_it != pendingMemRequests.end()) {
                    mem_request = request_it->second;
                } else {
                    Fault fault = initiateMemAccess(inst, mem_request);
                    if (fault != NoFault) {
                        inst->fault = fault;
                        DPRINTF(CClassCPU, "Memory issue fault: %s inst: %s\n",
                            fault->name(), *inst);
                    }

                    if (mem_request)
                        pendingMemRequests[request_key] = mem_request;
                }
                //fault
                if (inst->fault != NoFault) {
                    pendingMemRequests.erase(request_key);
                    if (pushInstToLatch(tid, inst)) {
                        fu->stalled = false;
                        it = thread.inFlightInsts.erase(it);
                        continue;
                    }
                } // mem request failed 
                else if (mem_request && mem_request->failed()) {
                    inst->fault = mem_request->fault;
                    pendingMemRequests.erase(request_key);
                    if (pushInstToLatch(tid, inst)) {
                        fu->stalled = false;
                        it = thread.inFlightInsts.erase(it);
                        continue;
                    }
                } else if (mem_request &&
                    (mem_request->sent() || mem_request->complete()))
                {
                    if (pushMemReqToLatch(tid, mem_request)) {
                        pendingMemRequests.erase(request_key);
                        fu->stalled = false;
                        it = thread.inFlightInsts.erase(it);
                        continue;
                    }
                } else if (mem_request) {
                    DPRINTF(CClassCPU,
                        "Memory request not ready for memory ISB yet: "
                        "inst=%s state=%d\n",
                        *inst, mem_request->state);
                }
            }// mem inst handling..
            else if (pushInstToLatch(tid, inst)) {
                fu->stalled = false;
                it = thread.inFlightInsts.erase(it);
                continue;
            }// any other instruction handling...
        } else if (fu->stalled) {
            DPRINTF(CClassCPU,
                "FU %u is stalled by head inst: %s execSeq=%llu; "
                "in-flight inst not ready yet: %s execSeq=%llu\n",
                inst->fuIndex,
                fu_head.inst->isBubble() ? "BUBBLE" :
                    csprintf("%s", *fu_head.inst).c_str(),
                fu_head.inst->isBubble() ? 0 : fu_head.inst->id.execSeqNum,
                *inst,
                inst->id.execSeqNum);
        }

        ++it;
    }

    //cleanupInFlightInsts(tid);
}

void
Execute::cleanupInFlightInsts(ThreadID tid)
{
    ExecuteThreadInfo &thread = executeInfo[tid];

    thread.inFlightInsts.erase(
        std::remove_if(thread.inFlightInsts.begin(), thread.inFlightInsts.end(),
            [this](const QueuedInst &entry) {
                CClassDynInstPtr inst = entry.inst;

                if (inst->isBubble())
                    return true;

                return !inst->isFault() &&
                    !inst->isNoCostInst() &&
                    inst->fuIndex >= numFuncUnits;
            }),
        thread.inFlightInsts.end());
}

void
Execute::resetISBOutputIndexes(ThreadID tid)
{
    ExecuteThreadInfo &thread = executeInfo[tid];

    thread.baseOutputIndex = 0;
    thread.memoryOutputIndex = 0;
    thread.trapOutputIndex = 0;
    thread.mboxOutputIndex = 0;
    thread.fboxOutputIndex = 0;
}

bool
Execute::pushInstToLatch(ThreadID tid, CClassDynInstPtr inst)
{
    ExecuteThreadInfo &thread = executeInfo[tid];
    Latch<ForwardInstData>::Input *out = &out_BASE;
    unsigned int *output_index = &thread.baseOutputIndex;

    if (inst->isFault()) {
        out = &out_TRAP;
        output_index = &thread.trapOutputIndex;
    } else if (inst->isInst()) {
        OpClass op_class = inst->staticInst->opClass();

        if (op_class == enums::IntMult || op_class == enums::IntDiv) {
            out = &out_MBOX;
            output_index = &thread.mboxOutputIndex;
        } else if (inst->staticInst->isFloating() ||
                   inst->staticInst->isVector()) {
            out = &out_FBOX;
            output_index = &thread.fboxOutputIndex;
        }
    }

    ForwardInstData &insts_out = *out->inputWire;
    unsigned int width = thread.instsBeingCommitted.width();

    if (insts_out.isBubble()) {
        insts_out = ForwardInstData(width, tid);
        insts_out.threadId = tid;
    }

    if (*output_index < insts_out.width()) {
        insts_out.insts[*output_index] = inst;
        DPRINTF(CClassCPU, "Pushed inst %s to ISB slot %u\n",
            *inst, *output_index);
        (*output_index)++;
        return true;
    }

    return false;
}

bool
Execute::pushMemReqToLatch(ThreadID tid, ExecRequestPtr request)
{
    ExecuteThreadInfo &thread = executeInfo[tid];
    ForwardMemData &mem_out = *out_MEMORY.inputWire;

    if (!mem_out.isBubble())
        return false;

    mem_out = ForwardMemData(request, tid);
    thread.memoryOutputIndex++;

    DPRINTF(CClassCPU, "Pushed memory request for inst %s to memory latch\n",
        *request->inst);
    return true;
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

    for (const auto &entry : executeInfo[inst->id.threadId].inFlightInsts) {
        if (!entry.inst->isBubble() && entry.inst->id == inst->id) {
            ret = true;
            break;
        }
    }

    return ret;
}

bool
Execute::isDrained()
{

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        if (!inputBuffer[tid].empty() ||
            !executeInfo[tid].inFlightInsts.empty()) {

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
Execute::handleBranch(ThreadID tid, CClassDynInstPtr inst)
{
    ThreadContext *thread = cpu.getContext(tid);
    std::unique_ptr<PCStateBase> sequential_pc(inst->pc->clone());
    sequential_pc->advance();

    ExecContext context(cpu, *cpu.threads[tid], *this, inst);
    Fault fault = inst->staticInst->execute(&context, inst->traceData);

    if (fault != NoFault) {
        inst->fault = fault;
        DPRINTF(CClassCPU, "Branch execution fault: %s inst: %s\n",
            fault->name(), *inst);
        return;
    }

    const PCStateBase &target = thread->pcState();
    bool taken = target.instAddr() != sequential_pc->instAddr();
    BranchData::Reason reason = taken ?
        BranchData::UnpredictedBranch :
        BranchData::CorrectlyPredictedBranch;

    BranchData &branch_fetch1 = *out_fetch1.inputWire;

    updateBranchData(tid, reason, inst, target, branch_fetch1);
    *out_fetch2.inputWire = branch_fetch1;
    *out_decode.inputWire = branch_fetch1;

    DPRINTF(CClassCPU, "Handled branch inst: %s taken=%d target=%s\n",
        *inst, taken, target);
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

        DPRINTF(CClassCPU, "Branch data signalled: %s\n", branch);
    }
}

Fault
Execute::initiateMemAccess(CClassDynInstPtr inst, ExecRequestPtr &request)
{
    assert(inst);
    assert(inst->isMemRef());

    requestBeingIssued.reset();

    ExecContext context(cpu, *cpu.threads[inst->id.threadId], *this, inst);
    Fault fault = inst->staticInst->initiateAcc(&context, inst->traceData);

    request = requestBeingIssued;
    requestBeingIssued.reset();

    if (fault != NoFault)
        return fault;

    if (request)
        return request->fault;

    return NoFault;
}

Fault
Execute::initiateMemRead(CClassDynInstPtr inst, Addr addr, unsigned int size,
    Request::Flags flags, const std::vector<bool> &byte_enable)
{   //data here is nullptr
    auto request = std::make_shared<ExecRequest>(
        *this, inst, true, nullptr, size, nullptr);

    if (inst->traceData)
        inst->traceData->setMem(addr, size, flags);

    ThreadContext *tc = cpu.getContext(inst->id.threadId);
    request->request->setContext(tc->contextId());
    request->request->setVirt(addr, size, flags, cpu.dataRequestorId(),
        inst->pc->instAddr());
    request->request->setByteEnable(byte_enable);

    requestBeingIssued = request;
    request->markInTranslation();

    DPRINTF(CClassCPU,
        "DTLB translation request: inst=%s type=load vaddr=%#x size=%u\n",
        *inst, addr, size);

    cpu.threads[inst->id.threadId]->mmu->translateTiming(
        request->request, tc, request.get(), BaseMMU::Read);

    return request->fault;
}

Fault
Execute::writeMem(CClassDynInstPtr inst, uint8_t *data, unsigned int size,
    Addr addr, Request::Flags flags, uint64_t *res,
    const std::vector<bool> &byte_enable)
{
    auto request = std::make_shared<ExecRequest>(
        *this, inst, false, data, size, res);

    if (inst->traceData)
        inst->traceData->setMem(addr, size, flags);

    ThreadContext *tc = cpu.getContext(inst->id.threadId);
    request->request->setContext(tc->contextId());
    request->request->setVirt(addr, size, flags, cpu.dataRequestorId(),
        inst->pc->instAddr());
    request->request->setByteEnable(byte_enable);

    requestBeingIssued = request;
    request->markInTranslation();

    DPRINTF(CClassCPU,
        "DTLB translation request: inst=%s type=store vaddr=%#x size=%u\n",
        *inst, addr, size);

    cpu.threads[inst->id.threadId]->mmu->translateTiming(
        request->request, tc, request.get(), BaseMMU::Write);

    return request->fault;
}

Fault
Execute::initiateMemAMO(CClassDynInstPtr inst, Addr addr, unsigned int size,
    Request::Flags flags, AtomicOpFunctorPtr amo_op)
{
    panic("CClass Execute AMO memory requests are not implemented yet\n");
    return NoFault;
}

void
ExecRequest::finish(const Fault &fault_, const RequestPtr &req,
    ThreadContext *tc, BaseMMU::Mode mode)
{
    execute.finishMemTranslation(this, fault_);
}

void
Execute::finishMemTranslation(ExecRequest *request, const Fault &fault)
{
    if (fault != NoFault) {
        request->markFault(fault);
        DPRINTF(CClassCPU, "DTLB translation fault: inst=%s type=%s fault=%s\n",
            *request->inst, request->isLoad ? "load" : "store",
            fault->name());
    } else {
        request->markTranslated();
        DPRINTF(CClassCPU, "DTLB translation complete: inst=%s type=%s\n",
            *request->inst, request->isLoad ? "load" : "store");
        sendTimingMemReq(request);
    }

    cpu.wakeupOnEvent(Pipeline::ExecuteStageId);
}

bool
Execute::sendTimingMemReq(ExecRequest *request)
{
    assert(request);

    if (!request->packet)
        request->makePacket();

    DPRINTF(CClassCPU, "Dcache request: inst=%s type=%s\n",
        *request->inst, request->isLoad ? "load" : "store");

    if (dcachePort.sendTimingReq(request->packet)) {
        DPRINTF(CClassCPU, "Dcache request accepted: inst=%s type=%s\n",
            *request->inst, request->isLoad ? "load" : "store");
        request->packet = nullptr;
        request->markSent();
        return true;
    }

    retryRequest = request;
    request->markRetry();
    DPRINTF(CClassCPU, "Dcache request blocked: inst=%s type=%s\n",
        *request->inst, request->isLoad ? "load" : "store");
    return false;
}

bool
Execute::recvTimingResp(PacketPtr pkt)
{
    ExecRequest *request = safe_cast<ExecRequest *>(pkt->popSenderState());

    DPRINTF(CClassCPU, "Dcache response: inst=%s type=%s\n",
        *request->inst, request->isLoad ? "load" : "store");

    request->markComplete(pkt);

    cpu.wakeupOnEvent(Pipeline::ExecuteStageId);
    return true;
}

void
Execute::recvReqRetry()
{
    if (!retryRequest)
        return;

    ExecRequest *request = retryRequest;
    retryRequest = nullptr;

    if (!sendTimingMemReq(request))
        return;

    cpu.wakeupOnEvent(Pipeline::ExecuteStageId);
}

Execute::~Execute()
{
    for (unsigned int i = 0; i < numFuncUnits; i++)
        delete funcUnits[i];

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
