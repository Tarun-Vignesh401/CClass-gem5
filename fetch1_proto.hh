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
#include "cpu/minor/buffers.hh"
#include "cpu/minor/cpu.hh"
#include "cpu/minor/pipe_data.hh"
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
            Latch<BranchData>::Output inp;
            //idk maybe this should come in fetch2?
           // Addr lineSnap;
           // Addr maxLineWidth;
            
            unsigned int fetchLimit;
           
            public:
            Fetch1(const std::string &name_, CClassCPU &cpu_, std::vector<InputBuffer<Fetch1ThreadInfo>> out_);
            void evaluate();

            void wakeupFetch(ThreadID tid);

            // this stuff will be of use when we complete the execute stage    
            bool isDrained();

            protected:    
            //this enum is primarily for multithreading support.
            enum Fetch1State {
                PCGenHalted,
                PCGenerated,
                PCWaitingForChange,    
            };
            struct Fetch1ThreadInfo
            {
                Fetch1ThreadInfo() {}

                Fetch1ThreadInfo(const Fetch1ThreadInfo& other) :
                state(other.state),
                pc(other.pc->clone()),
                streamSeqNum(other.streamSeqNum),
                predictionSeqNum(other.predictionSeqNum),
                blocked(other.blocked)
                {}
                Fetch1State state = PCWaitingForChange;

                std::unique_ptr<PCStateBase> pc;

                InstSeqNum streamSeqNum = InstId::firstStreamSeqNum;

                InstSeqNum predictionSeqNum = InstId::firstPredictionSeqNum;

                bool blocked = false;
                //The Address we are fetching lines from
                Addr FetchAddr = 0;
            };
            protected:
            void changeStream(const BranchData &branch);

            void updateExpectedSeqNums(const BranchData &branch);

            ThreadID getScheduledThread();

            //temporarily disbaled until we complete the pipeline    
            //branch_prediction::BPredUnit &branchPredictor;


            //multithreading support
            std::vector<Fetch1ThreadInfo> fetchInfo;
           
            //mulltithreading support
             ThreadID threadPriority = 0;
            }
        }
    }
#endif /* __CPU_MINOR_FETCH1_HH__ */
