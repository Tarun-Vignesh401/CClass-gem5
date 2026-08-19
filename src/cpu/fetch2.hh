
#ifndef __CPU_CCLASS_FETCH2_HH__
#define __CPU_CCLASS_FETCH2_HH__

#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/pipe_data.hh"
#include "cpu/cclass/buffers.hh"
#include "arch/riscv/pcstate.hh"

#include "cpu/cclass/dyn_inst.hh"
#include <vector>

namespace gem5
{
    namespace cclass{

    class Fetch2 : public Named{
        public:

        std::vector<InputBuffer<Fetch1ThreadInfo>> inputBuffer;

        Fetch2(const std::string &name_, CClassCPU &cpu_,
            const BaseCClassCPUParams &params,
            Latch<BranchData>::Output branch_,
            Latch<Fetch1ThreadInfo>::Output in_thread_,
            Latch<ForwardLineData>::Input out_,
            std::vector<InputBuffer<ForwardLineData>> &next_stage_input_buffer);

        void finaldebugprint(ThreadID tid,const Fetch1ThreadInfo* thread);
        
        // infrastucture for mispredction flush.
        //bool flush();

        protected:
        CClassCPU &cpu;

        Latch<Fetch1ThreadInfo>::Output in_thread;

        Latch<BranchData>::Output branch;

        Latch<ForwardLineData>::Input out;

        std::vector<InputBuffer<ForwardLineData>> &nextStageReserve;
        
        unsigned int fetchLimit;
        
        unsigned int maxLineWidth;

        unsigned int lineSnap;

        unsigned int numFetchesInITLB;

        unsigned int numFetchesInMemorySystem;

        InstSeqNum lineSeqNum;

        ThreadID threadPriority;

        std::vector<InstSeqNum> streamSeqNum;
        std::vector<InstSeqNum> predictionSeqNum;
        //I don't need it right now let's see if I need it later.
        const Fetch1ThreadInfo* fetch2_thread;

        enum Fetch2State{
                FetchRunning,
                FetchHalt,
                FetchWaiting             
        };

        Fetch2State fetchState;

        void fetchLine(ThreadID tid, const Fetch1ThreadInfo* thread);
        
        public:

        class FetchRequest :
            public BaseMMU::Translation, /* For TLB lookups */
            public Packet::SenderState /* For packing into a Packet */
            {
            protected:
            /** Owning fetch unit */
            Fetch2 &fetch;

            public:
            /** Progress of this request through address translation and
             *  memory */
            enum FetchRequestState
            {
                NotIssued, /* Just been made */
                InTranslation, /* Issued to ITLB, must wait for reqply */
                Translated, /* Translation complete */
                RequestIssuing, /* Issued to memory, must wait for response */
                Complete /* Complete.  Either a fault, or a fetched line */
            };

            FetchRequestState state;

            /** Identity of the line that this request will generate */
            InstId id;

            /** FetchRequests carry packets while they're in the requests and
             * transfers responses queues.  When a Packet returns from the memory
             * system, its request needs to have its packet updated as this may
             * have changed in flight */
            PacketPtr packet;

            /** The underlying request that this fetch represents */
            RequestPtr request;

            /** PC to fixup with line address */
            Addr pc;

            /** Fill in a fault if one happens during fetch, check this by
             *  picking apart the response packet */
            Fault fault;

            /** Make a packet to use with the memory transaction */
            void makePacket();

            /** Report interface */
            //void reportData(std::ostream &os) const;

            /** Is this line out of date with the current stream/prediction
             *  sequence and can it be discarded without orphaning in flight
             *  TLB lookups/memory accesses? */
            bool isDiscardable() const;

            /** Is this a complete read line or fault */
            bool isComplete() const { return state == Complete; }

            protected:
            /** BaseMMU::Translation interface */

            /** Interface for ITLB responses.  We can handle delay, so don't
             *  do anything */
            void markDelayed() { }

            /** Interface for ITLB responses.  Populates self and then passes
             *  the request on to the ports' handleTLBResponse member
             *  function */
            void finish(const Fault &fault_, const RequestPtr &request_,
                        ThreadContext *tc, BaseMMU::Mode mode);

            public:
            FetchRequest(Fetch2 &fetch_, InstId id_, Addr pc_) :
                SenderState(),
                fetch(fetch_),
                state(NotIssued),
                id(id_),
                packet(NULL),
                request(),
                pc(pc_),
                fault(NoFault)
            {
                request = std::make_shared<Request>();
            }

            ~FetchRequest();
            };
            
        typedef FetchRequest *FetchRequestPtr;

        void evaluate();

      protected:
        typedef Queue<FetchRequestPtr,
        ReportTraitsPtrAdaptor<FetchRequestPtr>,
        NoBubbleTraits<FetchRequestPtr> >
        FetchQueue;

        FetchQueue requests;

        FetchQueue transfers;

        enum IcacheState
            {
                IcacheRunning, /* Default. Step icache queues when possible */
                IcacheNeedsRetry /* Request rejected, will be asked to retry */
            };

        /** Retry state of icache_port */
        IcacheState icacheState;
        
        
        void processResponse(FetchRequestPtr response, ForwardLineData &line,const Fetch1ThreadInfo* thread);

        void popAndDiscard(FetchQueue &queue);

        void stepQueues();

        void popInput(ThreadID tid);

        bool checkRedirect();

        const Fetch1ThreadInfo* getInput(ThreadID tid);

        //void handleTLBResponse(FetchRequestPtr response);

          /** Interface for ITLB responses.  We can handle delay, so don't
         *  do anything */
        void markDelayed() { }

        /** Interface for ITLB responses.  Populates self and then passes
         *  the request on to the ports' handleTLBResponse member
         *  function */
        //void finish(const Fault &fault_, const RequestPtr &request_,
                    //ThreadContext *tc, BaseMMU::Mode mode);

        
        //friend std::ostream &operator <<(std::ostream &os,
        //IcacheState state);

        unsigned int numInFlightFetches();

        void handleTLBResponse(FetchRequestPtr response);

        void tryToSendToTransfers(FetchRequestPtr request);

        void moveFromRequestsToTransfers(FetchRequestPtr request);

        bool tryToSend(FetchRequestPtr request);

    public:

    class IcachePort : public CClassCPU::CClassCPUPort
        {
        protected:
        /** My owner */
        Fetch2 &fetch2;

        public:
            IcachePort(std::string name, Fetch2 &fetch2, CClassCPU &cpu) :
            CClassCPU::CClassCPUPort(name, cpu), fetch2(fetch2)
            { }

        protected:
        bool recvTimingResp(PacketPtr pkt){return fetch2.recvTimingResp(pkt);}
        void recvReqRetry() { fetch2.recvReqRetry(); }
    };

    IcachePort icachePort;

    //protected:
    virtual bool recvTimingResp(PacketPtr pkt);
    virtual void recvReqRetry();
        
};


}
}

#endif // __CPU_CCLASS_FETCH2_HH__
