

/**
 * @file
 *
 *  
 */

#ifndef __CPU_CCLASS_CPU_HH__
#define __CPU_CCLASS_CPU_HH__

#include "base/compiler.hh"
#include "base/random.hh"
#include "cpu/base.hh"
//#include "cpu/minor/activity.hh"
//#include "cpu/minor/stats.hh"
#include "cpu/simple_thread.hh"
#include "enums/CClassThreadPolicy.hh"
#include "params/BaseCClassCPU.hh"

namespace gem5
{

namespace cclass
{
/** Forward declared to break the cyclic inclusion dependencies between
 *  pipeline and cpu */
class Pipeline;

/** Minor will use the SimpleThread state for now */
typedef SimpleThread CClassThread;
}


class CClassCPU : public BaseCPU
{
  protected:
    /** pipeline is a container for the clockable pipeline stage objects.
     *  Elements of pipeline call TheISA to implement the model. */
    cclass::Pipeline *pipeline;

    Random::RandomPtr rng = Random::genRandom();

  public:
    /** Activity recording for pipeline.  This belongs to Pipeline but
     *  stages will access it through the CPU as the CClassCPU object
     *  actually mediates idling behaviour */
   // cclass::CClassActivityRecorder *activityRecorder;

    /** These are thread state-representing objects for this CPU.  If
     *  you need a ThreadContext for *any* reason, use
     *  threads[threadId]->getTC() */
    std::vector<cclass::CClassThread*> threads;

  public:
    /** Provide a non-protected base class for Minor's Ports as derived
     *  classes are created by Fetch1 and Execute */
  
    class CClassCPUPort : public RequestPort
    {
      public:
        /** The enclosing cpu */
        CClassCPU &cpu;

      public:
        CClassCPUPort(const std::string& name_, CClassCPU &cpu_)
            : RequestPort(name_), cpu(cpu_)
        { }

    };

    /** Thread Scheduling Policy (RoundRobin, Random, etc) */
   enums::CClassThreadPolicy threadPolicy;
  protected:
     /** Return a reference to the data port. */
    Port &getDataPort() override;

    /** Return a reference to the instruction port. */
    Port &getInstPort() override;

  public:
    CClassCPU(const BaseCClassCPUParams &params);

    ~CClassCPU();

  public:
    /** Starting, waking and initialisation */
    void init() override;
    void startup() override;
    void wakeup(ThreadID tid) override;

    /** Processor-specific statistics */
    //cclass::CClassStats stats;

    /** Stats interface from SimObject (by way of BaseCPU) */
    void regStats() override;

    /** Simple inst count interface from BaseCPU */
    Counter totalInsts() const override;
    Counter totalOps() const override;

    void serializeThread(CheckpointOut &cp, ThreadID tid) const override;
    void unserializeThread(CheckpointIn &cp, ThreadID tid) override;

    /** Serialize pipeline data */
    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

    /** Drain interface */
    //DrainState drain() override;
    //void drainResume() override;
    /** Signal from Pipeline that MinorCPU should signal that a drain
     *  is complete and set its drainState */
    //void signalDrainDone();
    //void memWriteback() override;

    /** Switching interface from BaseCPU */
    //void switchOut() override;
    //void takeOverFrom(BaseCPU *old_cpu) override;

    /** Thread activation interface from BaseCPU. */
    void activateContext(ThreadID thread_id) override;
    void suspendContext(ThreadID thread_id) override;

    /** Thread scheduling utility functions */
    /*std::vector<ThreadID> roundRobinPriority(ThreadID priority)
    {
        std::vector<ThreadID> prio_list;
        for (ThreadID i = 1; i <= numThreads; i++) {
            prio_list.push_back((priority + i) % numThreads);
        }
        return prio_list;
    }

    std::vector<ThreadID> randomPriority()
    {
        std::vector<ThreadID> prio_list;
        for (ThreadID i = 0; i < numThreads; i++) {
            prio_list.push_back(i);
        }

        std::shuffle(prio_list.begin(), prio_list.end(),
                     rng->gen);

        return prio_list;
    } */

    /** The tick method in the MinorCPU is simply updating the cycle
     * counters as the ticking of the pipeline stages is already
     * handled by the Pipeline object.
     */
    void tick() { updateCycleCounters(BaseCPU::CPU_STATE_ON); }

    /** Interface for stages to signal that they have become active after
     *  a callback or eventq event where the pipeline itself may have
     *  already been idled.  The stage argument should be from the
     *  enumeration Pipeline::StageId */
    void wakeupOnEvent(unsigned int stage_id);
    EventFunctionWrapper *fetchEventWrapper;
};
} // namespace gem5

#endif /* __CPU_CCLASS_CPU_HH__ */
