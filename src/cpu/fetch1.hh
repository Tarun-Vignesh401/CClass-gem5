#ifndef __CPU_CCLASS_FETCH1_HH__
#define __CPU_CCLASS_FETCH1_HH__
/**
 * @file
 *
 *  Fetch1 is the pcgen stage, generate the pc send it out in fetch1threadinfo to stage1
 *  
 */
#include <vector>

// use it in stage1
//#include "arch/generic/mmu.hh"
#include "base/named.hh"
#include "cpu/base.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/dyn_inst.hh"
#include "cpu/cclass/pipe_data.hh"
//#include "cpu/minor/pipe_data.hh"
//use this in stage1
//#include "mem/packet.hh"

namespace gem5
{
    namespace cclass
    {
        class Fetch1 : public Named{  
            protected:
            CClassCPU &cpu;
            // have to make the class template for this
            std::vector<InputBuffer<Fetch1ThreadInfo>> &nextStageReserve;
            Latch<Fetch1ThreadInfo>::Input out;
            //idk maybe this should come in fetch2?
           // Addr lineSnap;
           // Addr maxLineWidth;            
            unsigned int fetchLimit;

            void processResponse(Fetch1ThreadInfo &out, Fetch1ThreadInfo &thread);
           
            public:
            Fetch1(const std::string &name_,CClassCPU &cpu_,const BaseCClassCPUParams &params,
                Latch<Fetch1ThreadInfo>::Input out_,std::vector<InputBuffer<Fetch1ThreadInfo>> &nextStageReserve_);
            void evaluate();

            void wakeupFetch(ThreadID tid);

            void advancepc(ThreadID tid);

            // this stuff will be of use when we complete the execute stage    
            bool isDrained();

            protected:
            //void changeStream(const BranchData &branch);

            //void updateExpectedSeqNums(const BranchData &branch);

            ThreadID getScheduledThread();

            //temporarily disbaled until we complete the pipeline    
            //branch_prediction::BPredUnit &branchPredictor;


            //multithreading support
            std::vector<Fetch1ThreadInfo> fetchInfo;
           
            //mulltithreading support
             ThreadID threadPriority = 0;
            };
        }
    }
#endif /* __CPU_MINOR_FETCH1_HH__ */
