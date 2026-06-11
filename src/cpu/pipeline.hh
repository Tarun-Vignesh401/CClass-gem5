/**
 * @file
 *
 *  The constructed pipeline.  Kept out of MinorCPU to keep the interface
 *  between the CPU and its grubby implementation details clean.
 */

#ifndef __CPU_CCLASS_PIPELINE_HH__
#define __CPU_CCLASS_PIPELINE_HH__

//#include "cpu/cclass/activity.hh"
#include "cpu/cclass/cpu.hh"
//#include "cpu/cclass/decode.hh"
//#include "cpu/cclass/execute.hh"
#include "cpu/cclass/fetch1.hh"
#include "cpu/cclass/fetch2.hh"
#include "params/BaseCClassCPU.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

namespace cclass
{

/**
 * @namespace cclass
 *
 * Minor contains all the definitions within the MinorCPU apart from the CPU
 * class itself
 */

/** The constructed pipeline.  Kept out of MinorCPU to keep the interface
 *  between the CPU and its grubby implementation details clean. */
class Pipeline : public Ticked
{
  protected:
    CClassCPU &cpu;

    /** Allow cycles to be skipped when the pipeline is idle */
    //bool allow_idling;

    Latch<Fetch1ThreadInfo> f1ToF2;
    //Latch<BranchData> f2ToF1;
   // Latch<ForwardInstData> f2ToD;
    //Latch<ForwardInstData> dToE;
    //Latch<BranchData> eToF1;

    //Execute execute;
    //Decode decode;
    Fetch2 fetch2;
    Fetch1 fetch1;

    /** Activity recording for the pipeline.  This is access through the CPU
     *  by the pipeline stages but belongs to the Pipeline as it is the
     *  cleanest place to initialise it */
    //MinorActivityRecorder activityRecorder;

    class IcachePort : public CClassCPU::CClassCPUPort
    {
      protected:
        /** My owner */
      Pipeline &pipeline;

      public:
        IcachePort(std::string name, Pipeline &pipeline, CClassCPU &cpu) :
            CClassCPU::CClassCPUPort(name, cpu), pipeline(pipeline)
        { }

      protected:
        bool recvTimingResp(PacketPtr pkt);

        void recvReqRetry() override;
    };

    class DcachePort : public CClassCPU::CClassCPUPort
    {
      protected:
        /** My owner */
      Pipeline &pipeline;

      public:
        DcachePort(std::string name, Pipeline &pipeline, CClassCPU &cpu) :
            CClassCPU::CClassCPUPort(name, cpu), pipeline(pipeline)
        { }

      protected:
        bool recvTimingResp(PacketPtr pkt) override;

        void recvReqRetry() override;

        bool isSnooping() const override;

        void recvTimingSnoopReq(PacketPtr pkt) override;

        void recvFunctionalSnoop(PacketPtr pkt) override;
    };

    DcachePort dcachePort;
    IcachePort icachePort;

  public:
    /** Enumerated ids of the 'stages' for the activity recorder */
    enum StageId
    {
        /* A stage representing wakeup of the whole processor */
        CPUStageId = 0,
        /* Real pipeline stages */
        Fetch1StageId, Fetch2StageId, DecodeStageId, ExecuteStageId,
        Num_StageId /* Stage count */
    };

    /** True after drain is called but draining isn't complete */
    //bool needToSignalDrained;

  public:
    Pipeline(CClassCPU &cpu_, const BaseCClassCPUParams &params);

  public:
    /** Wake up the Fetch unit.  This is needed on thread activation esp.
     *  after quiesce wakeup */
    void wakeupFetch(ThreadID tid);

    /** Try to drain the CPU */
    //bool drain();

    //void drainResume();

    /** Test to see if the CPU is drained */
    //bool isDrained();

    /** A custom evaluate allows report in the right place (between
     *  stages and pipeline advance) */
    void evaluate() override;

    //void minorTrace() const;

    /** Functions below here are BaseCPU operations passed on to pipeline
     *  stages */

    /** Return the IcachePort belonging to Fetch1 for the CPU */
    CClassCPU::CClassCPUPort &getInstPort();
    /** Return the DcachePort belonging to Execute for the CPU */
    CClassCPU::CClassCPUPort &getDataPort();

    /** To give the activity recorder to the CPU */
    //MinorActivityRecorder *getActivityRecorder() { return &activityRecorder; }
};

} // namespace cclass
} // namespace gem5

#endif /* __CPU_CCLASS_PIPELINE_HH__ */
