
#ifndef __CPU_CCLASS_FETCH2_HH__
#define __CPU_CCLASS_FETCH2_HH__

#include "cpu/cclass/cpu.hh"
#include "cpu/cclass/pipe_data.hh"
#include "cpu/cclass/buffers.hh"
#include <vector>

namespace gem5
{
    namespace cclass{
    class Fetch2 : public Named{
        public:

        std::vector<InputBuffer<Fetch1ThreadInfo>> inputBuffer;

        Fetch2(const std::string &name_, CClassCPU &cpu_,
            const BaseCClassCPUParams &params,
            Latch<Fetch1ThreadInfo>::Output in_thread_);

        void finaldebugprint();
        
        // infrastucture for mispredction flush.
        //bool flush();

        protected:
        CClassCPU &cpu;

        Latch<Fetch1ThreadInfo>::Output in_thread;

        void fetchLine(ThreadID tid);
        
        public:

        class FetchRequest :
            public BaseMMU::Translation, /* For TLB lookups */
            public Packet::SenderState /* For packing into a Packet */
            {
            protected:
            /** Owning fetch unit */
            Fetch1 &fetch;

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
            void reportData(std::ostream &os) const;

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
            FetchRequest(Fetch1 &fetch_, InstId id_, Addr pc_) :
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

        enum IcacheState
            {
                IcacheRunning, /* Default. Step icache queues when possible */
                IcacheNeedsRetry /* Request rejected, will be asked to retry */
            };

        /** Retry state of icache_port */
        IcacheState icacheState;

        /** Sequence number for line fetch used for ordering lines to flush */
        InstSeqNum lineSeqNum;

        /** Count of the number fetches which have left the transfers queue
        *  and are in the 'wild' in the memory system.  Try not to rely on
        *  this value, it's better to code without knowledge of the number
        *  of outstanding accesses */
        unsigned int numFetchesInMemorySystem;

        /** Number of requests inside the ITLB rather than in the queues.
        *  All requests so located *must* have reserved space in the
        *  transfers queue */
        unsigned int numFetchesInITLB;
        
        void handleTLBResponse(FetchRequestPtr response);

          /** Interface for ITLB responses.  We can handle delay, so don't
         *  do anything */
        void markDelayed() { }

        /** Interface for ITLB responses.  Populates self and then passes
         *  the request on to the ports' handleTLBResponse member
         *  function */
        void finish(const Fault &fault_, const RequestPtr &request_,
                    ThreadContext *tc, BaseMMU::Mode mode);

        
        friend std::ostream &operator <<(std::ostream &os,
        IcacheState state);


        typedef Queue<FetchRequestPtr,
        ReportTraitsPtrAdaptor<FetchRequestPtr>,
        NoBubbleTraits<FetchRequestPtr> >
        FetchQueue;

        FetchQueue requests;

        FetchQueue transfers;

        unsigned int maxLineWidth;

        unsigned int lineSnap;

        unsigned int numFetchesInITLB;

        unsigned int numFetchesInMemorySystem;
        };


}
}

#endif // __CPU_CCLASS_FETCH2_HH__
