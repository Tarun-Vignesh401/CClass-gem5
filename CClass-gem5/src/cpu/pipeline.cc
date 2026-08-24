
#include "cpu/cclass/pipeline.hh"

#include <algorithm>

#include "cpu/cclass/decode.hh"
#include "cpu/cclass/execute.hh"
#include "cpu/cclass/fetch1.hh"
#include "cpu/cclass/fetch2.hh"
//#include "debug/Drain.hh" // auto - generated stuff
#include "debug/CClassCPU.hh"
//#include "debug/MinorTrace.hh"
#include "debug/Quiesce.hh"

namespace gem5
{

namespace cclass
{

Pipeline::Pipeline(CClassCPU &cpu_, const BaseCClassCPUParams &params) :
    Ticked(cpu_,&(cpu_.BaseCPU::baseStats.numCycles)),
    cpu(cpu_),
    //allow_idling(params.enableIdling),
    f1ToF2(cpu.name() + ".f1ToF2", "lines",params.fetch1ToFetch2ForwardDelay),
    f2ToD(cpu.name() + ".f2ToD", "insts",
        params.fetch2ToDecodeForwardDelay),
    dToE(cpu.name() + ".dToE", "insts",
        params.decodeToExecuteForwardDelay),
    eToF1(cpu.name() + ".eToF1", "branch",
        params.executeBranchDelay),
    eToF2(cpu.name() + ".eToF2", "branch",
        params.executeBranchDelay),
    eToD(cpu.name() + ".eToD", "branch",
        params.executeBranchDelay),
    eToBASE(cpu.name() + ".eToBASE", "insts",
        params.executeToMemoryForwardDelay),
    eToMEMORY(cpu.name() + ".eToMEMORY", "mem_reqs",
        params.executeToMemoryForwardDelay),
    eToTRAP(cpu.name() + ".eToTRAP", "insts",
        params.executeToMemoryForwardDelay),
    eToMBOX(cpu.name() + ".eToMBOX", "insts",
        params.executeToMemoryForwardDelay),
    eToFBOX(cpu.name() + ".eToFBOX", "insts",
        params.executeToMemoryForwardDelay),
    execute(cpu.name() + ".execute", cpu, params,
        dToE.output(), eToF1.input(), eToF2.input(), eToD.input(),
        eToBASE.input(), eToMEMORY.input(), eToTRAP.input(),
        eToMBOX.input(), eToFBOX.input()),
    decode(cpu.name() + ".decode", cpu, params, eToD.output(),
        f2ToD.output(), dToE.input(), execute.inputBuffer),
    fetch2(cpu.name() + ".fetch2", cpu, params, eToF2.output(), f1ToF2.output(),
        f2ToD.input(),decode.inputBuffer),
    fetch1(cpu.name() + ".fetch1", cpu, params, eToF1.output(),
        f1ToF2.input(), fetch2.inputBuffer)
    /*activityRecorder(cpu.name() + ".activity", Num_StageId,
      *   The max depth of inter-stage FIFOs 
      *   std::max(params.fetch1ToFetch2ForwardDelay,
      *  std::max(params.fetch2ToDecodeForwardDelay,
      * std::max(params.decodeToExecuteForwardDelay,
      * params.executeBranchDelay)))),
      *  needToSignalDrained(false)
      *  */
{
    if (params.fetch1ToFetch2ForwardDelay < 1) {
        fatal("%s: fetch1ToFetch2ForwardDelay must be >= 1 (%d)\n",
            cpu.name(), params.fetch1ToFetch2ForwardDelay);
    }

    if (params.fetch2ToDecodeForwardDelay < 1) {
        fatal("%s: fetch2ToDecodeForwardDelay must be >= 1 (%d)\n",
            cpu.name(), params.fetch2ToDecodeForwardDelay);
    }

    if (params.decodeToExecuteForwardDelay < 1) {
        fatal("%s: decodeToExecuteForwardDelay must be >= 1 (%d)\n",
            cpu.name(), params.decodeToExecuteForwardDelay);
    }

    if (params.executeBranchDelay < 1) {
        fatal("%s: executeBranchDelay must be >= 1\n",
            cpu.name(), params.executeBranchDelay);
    }
}
/*
void
Pipeline::minorTrace() const
{
    fetch1.minorTrace();
    f1ToF2.minorTrace();
    f2ToF1.minorTrace();
    fetch2.minorTrace();
    f2ToD.minorTrace();
    decode.minorTrace();
    dToE.minorTrace();
    execute.minorTrace();
    eToF1.minorTrace();
    activityRecorder.minorTrace();
}
*/

void
Pipeline::evaluate()
{   
    /** We tick the CPU to update the BaseCPU cycle counters */
    cpu.tick();
    /* Note that it's important to evaluate the stages in order to allow
     *  'immediate', 0-time-offset TimeBuffer activity to be visible from
     *  later stages to earlier ones in the same cycle */
    //execute.evaluate();
    fetch1.evaluate();
    //std::cout<<"I am in fetch1!\n";

    fetch2.evaluate();
    //std::cout<<"I am in fetch2!!\n";

    decode.evaluate();
     //std::cout<<"I am in decode !!\n";

    execute.evaluate();

    //std::cout << "current cycle : " << cpu.curCycle() << "\n";
    /*if (debug::MinorTrace)
        minorTrace();*/
    //Update the time buffers after the stages 
    f1ToF2.evaluate();
    f2ToD.evaluate();
    dToE.evaluate();
    eToF1.evaluate();
    eToF2.evaluate();
    eToD.evaluate();
    eToBASE.evaluate();
    eToMEMORY.evaluate();
    eToTRAP.evaluate();
    eToMBOX.evaluate();
    eToFBOX.evaluate();

    //fetch2.finaldebugprint();

    //std::cout << "you've entered evaluate of fetch2!"<<std::endl;


/* f2ToF1.evaluate();
    f2ToD.evaluate();
    dToE.evaluate();
    eToF1.evaluate();

     The activity recorder must be be called after all the stages and
     before the idler (which acts on the advice of the activity recorder 
    activityRecorder.evaluate();
    
    if (allow_idling) {
         Become idle if we can but are not draining 
        if (!activityRecorder.active() && !needToSignalDrained) {
            DPRINTF(Quiesce, "Suspending as the processor is idle\n");
            stop();
        }
        
         Deactivate all stages.  Note that the stages *could*
           activate and deactivate themselves but that's fraught
           with additional difficulty.
          As organised herre 
        activityRecorder.deactivateStage(Pipeline::CPUStageId);
        activityRecorder.deactivateStage(Pipeline::Fetch1StageId);
        activityRecorder.deactivateStage(Pipeline::Fetch2StageId);
        activityRecorder.deactivateStage(Pipeline::DecodeStageId);
        activityRecorder.deactivateStage(Pipeline::ExecuteStageId);
    }

    if (needToSignalDrained)  Must be draining 
    {
        DPRINTF(Drain, "Still draining\n");
        if (isDrained()) {
            DPRINTF(Drain, "Signalling end of draining\n");
            cpu.signalDrainDone();
            needToSignalDrained = false;
            stop();
        }
    }*/
}

CClassCPU::CClassCPUPort &
Pipeline::getInstPort()
{
    return fetch2.icachePort;
}

CClassCPU::CClassCPUPort &
Pipeline::getDataPort()
{
    return execute.dcachePort;
}

void
Pipeline::wakeupFetch(ThreadID tid)
{
    fetch1.wakeupFetch(tid);
}
/*
bool
Pipeline::drain()
{
    DPRINTF(MinorCPU, "Draining pipeline by halting inst fetches. "
        " Execution should drain naturally\n");

    execute.drain();

     Make sure that needToSignalDrained isn't accidentally set if we
       are 'pre-drained' 
    bool drained = isDrained();
    needToSignalDrained = !drained;

    return drained;
}

void
Pipeline::drainResume()
{
    DPRINTF(Drain, "Drain resume\n");

    for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        fetch1.wakeupFetch(tid);
    }

    execute.drainResume();
}

bool
Pipeline::isDrained()
{
    bool fetch1_drained = fetch1.isDrained();
    bool fetch2_drained = fetch2.isDrained();
    bool decode_drained = decode.isDrained();
    bool execute_drained = execute.isDrained();

    bool f1_to_f2_drained = f1ToF2.empty();
    bool f2_to_f1_drained = f2ToF1.empty();
    bool f2_to_d_drained = f2ToD.empty();
    bool d_to_e_drained = dToE.empty();

    bool ret = fetch1_drained && fetch2_drained &&
        decode_drained && execute_drained &&
        f1_to_f2_drained && f2_to_f1_drained &&
        f2_to_d_drained && d_to_e_drained;

    DPRINTF(MinorCPU, "Pipeline undrained stages state:%s%s%s%s%s%s%s%s\n",
        (fetch1_drained ? "" : " Fetch1"),
        (fetch2_drained ? "" : " Fetch2"),
        (decode_drained ? "" : " Decode"),
        (execute_drained ? "" : " Execute"),
        (f1_to_f2_drained ? "" : " F1->F2"),
        (f2_to_f1_drained ? "" : " F2->F1"),
        (f2_to_d_drained ? "" : " F2->D"),
        (d_to_e_drained ? "" : " D->E")
        );

    return ret;
}*/
} // namespace cclass
} // namespace gem5
