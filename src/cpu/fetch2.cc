
#ifndef __CPU_CCLASS_FETCH2_CC__
#define __CPU_CCLASS_FETCH2_CC__


#include "cpu/cclass/fetch2.hh"

#include "cpu/cclass/fetch1.hh"
#include "cpu/cclass/pipe_data.hh"
#include "cpu/cclass/cpu.hh"
#include "debug/CClassCPU.hh"
#include "cpu/cclass/pipeline.hh"


namespace gem5
{
namespace cclass {

Fetch2::Fetch2(const std::string &name_, CClassCPU &cpu_,
    const BaseCClassCPUParams &params,
    Latch<Fetch1ThreadInfo>::Output in_thread_,
    Latch<ForwardLineData>::Input out_,
    std::vector<InputBuffer<ForwardLineData>> &next_stage_input_buffer) :
    Named(name_),
    cpu(cpu_),
    in_thread(in_thread_),
    out(out_),
    nextStageReserve(next_stage_input_buffer),
    lineSnap(params.fetch2LineSnapWidth),
    maxLineWidth(params.fetch2LineWidth),
    fetchLimit(params.fetch1FetchLimit),
    //fetchInfo(params.numThreads),
    threadPriority(0),
    requests(name_ + ".requests", "lines", params.fetch1FetchLimit),
    transfers(name_ + ".transfers", "lines", params.fetch1FetchLimit),
    icacheState(IcacheRunning),
    lineSeqNum(InstId::firstLineSeqNum),
    numFetchesInMemorySystem(0),
    numFetchesInITLB(0),
    icachePort(cpu.name() + ".icache_port", *this, cpu)

{    

    if (lineSnap == 0) {
        lineSnap = cpu.cacheLineSize();
        DPRINTF(CClassCPU,"lineSnap set to cache line size of: %d\n",
            lineSnap);
    }

    if (maxLineWidth == 0) {
        maxLineWidth = cpu.cacheLineSize();
        DPRINTF(CClassCPU, "maxLineWidth set to cache line size of: %d\n",
            maxLineWidth);
    }

    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        inputBuffer.push_back(
            InputBuffer<Fetch1ThreadInfo>(
                name_ + ".inputBuffer" + std::to_string(tid), "lines",
                params.fetch2InputBufferSize));
    }
}

void Fetch2::finaldebugprint(ThreadID tid,const Fetch1ThreadInfo* thread)
{ 
   DPRINTF(CClassCPU, "State: %d,streamSeqNum: %d, predictionSeqNum: %d,blocked: %d,FetchAddr: %#x, bubble:%d\n",
        thread->state,thread->streamSeqNum,thread->predictionSeqNum,thread->blocked,thread->FetchAddr,thread->isBubble());
     if (thread->pc) {
        DPRINTF(CClassCPU, "PC: %s\n", thread->pc->instAddr());
    }
    // latch output
    Fetch1ThreadInfo thread_info = *in_thread.outputWire;
    DPRINTF(CClassCPU,"state: %d, streamSeqNum: %d, predictionSeqNum: %d\n",thread_info.state,
    thread_info.streamSeqNum,thread_info.predictionSeqNum);
}

void
Fetch2::fetchLine(ThreadID tid,const Fetch1ThreadInfo* thread)
{
    /* If line_offset != 0, a request is pushed for the remainder of the
     * line. */
    /* Use a lower, sizeof(MachInst) aligned address for the fetch */
    Addr aligned_pc = thread->FetchAddr & ~((Addr) lineSnap - 1);
    unsigned int line_offset = aligned_pc % lineSnap;
    //unsigned int request_size = maxLineWidth - line_offset;
    unsigned int request_size = 4;

    /* Fill in the line's id */
    InstId request_id(tid,
        thread->streamSeqNum, thread->predictionSeqNum,
        lineSeqNum);

    FetchRequestPtr request = new FetchRequest(*this, request_id,
            thread->FetchAddr);

    //DPRINTF(CClassCPU, "Inserting fetch into the fetch queue "
    //    "%s addr: 0x%x pc: %s line_offset: %d request_size: %d\n",
      //  /*request_id*/ aligned_pc, thread.FetchAddr, line_offset, request_size);
//Instid op defn was missing this was the problem
    request->request->setContext(cpu.threads[tid]->getTC()->contextId());
    request->request->setVirt(
        aligned_pc, request_size, Request::INST_FETCH, cpu.instRequestorId(),
        /* I've no idea why we need the PC, but give it */
        thread->FetchAddr);

    DPRINTF(CClassCPU, "Submitting ITLB request,Size of request : %d\n" ,request_size);
    numFetchesInITLB++;

    request->state = FetchRequest::InTranslation;

    /* Reserve space in the queues upstream of requests for results */
    transfers.reserve();
    requests.push(request);

    /* Submit the translation request.  The response will come
     *  through finish/markDelayed on this request as it bears
     *  the Translation interface */
    cpu.threads[request->id.threadId]->mmu->translateTiming(
        request->request,
        cpu.getContext(request->id.threadId),
        request, BaseMMU::Execute);

    lineSeqNum++;

    /* Step the PC for the next line onto the line aligned next address.
     * Note that as instructions can span lines, this PC is only a
     * reliable 'new' PC if the next line has a new stream sequence number. */
    //I don't think we need to step cacheline addresses here!
    //thread.FetchAddr = aligned_pc + request_size;
}

void
Fetch2::FetchRequest::makePacket()
{
    /* Make the necessary packet for a memory transaction */
    packet = new Packet(request, MemCmd::ReadReq);
    packet->allocate();

    /* This FetchRequest becomes SenderState to allow the response to be
     *  identified */
    packet->pushSenderState(this);
}

void
Fetch2::FetchRequest::finish(const Fault &fault_, const RequestPtr &request_,
                             ThreadContext *tc, BaseMMU::Mode mode)
{
    fault = fault_;

    state = Translated;
    fetch.handleTLBResponse(this);

    /* Let's try and wake up the processor for the next cycle */
    fetch.cpu.wakeupOnEvent(Pipeline::Fetch2StageId);
}

void
Fetch2::handleTLBResponse(FetchRequestPtr response)
{
    numFetchesInITLB--;

    if (response->fault != NoFault) {
        DPRINTF(CClassCPU, "Fault in address ITLB translation: %s, "
            "paddr: 0x%x, vaddr: 0x%x\n",
            response->fault->name(),
            (response->request->hasPaddr() ?
                response->request->getPaddr() : 0),
            response->request->getVaddr());

       // if (debug::MinorTrace)
           // minorTraceResponseLine(name(), response);
    } else {
        DPRINTF(CClassCPU, "Got ITLB response\n");
    }

    response->state = FetchRequest::Translated;

    Fetch2::tryToSendToTransfers(response);
}

Fetch2::FetchRequest::~FetchRequest()
{
    if (packet)
        delete packet;
}


void
Fetch2::tryToSendToTransfers(FetchRequestPtr request)
{
    if (!requests.empty() && requests.front() != request) {
        DPRINTF(CClassCPU, "Fetch not at front of requests queue, can't"
            " issue to memory\n");
        return;
    }

    if (request->state == FetchRequest::InTranslation) {
        DPRINTF(CClassCPU, "Fetch still in translation, not issuing to"
            " memory\n");
        return;
    }

    if (request->isDiscardable() || request->fault != NoFault) {
        /* Discarded and faulting requests carry on through transfers
         *  as Complete/packet == NULL */

        request->state = FetchRequest::Complete;
        moveFromRequestsToTransfers(request);

        /* Wake up the pipeline next cycle as there will be no event
         *  for this queue->queue transfer */
        cpu.wakeupOnEvent(Pipeline::Fetch2StageId);
    } else if (request->state == FetchRequest::Translated) {
        if (!request->packet)
            request->makePacket();

        /* Ensure that the packet won't delete the request */
        assert(request->packet->needsResponse());

        if (tryToSend(request))
            Fetch2::moveFromRequestsToTransfers(request);
    } else {
        DPRINTF(CClassCPU, "Not advancing line fetch\n");
    }
}

void
Fetch2::moveFromRequestsToTransfers(FetchRequestPtr request)
{
    assert(!requests.empty() && requests.front() == request);

    requests.pop();
    transfers.push(request);
}


void
Fetch2::stepQueues()
{
    IcacheState old_icache_state = icacheState;

    switch (icacheState) {
      case IcacheRunning:
        /* Move ITLB results on to the memory system */
        if (!requests.empty()) {
            tryToSendToTransfers(requests.front());
        }
        break;
      case IcacheNeedsRetry:
        break;
    }

    if (icacheState != old_icache_state) {
        DPRINTF(CClassCPU, "Step in state %s moving to state %s\n",
            old_icache_state, icacheState);
    }
}

void
Fetch2::popAndDiscard(FetchQueue &queue)
{
    if (!queue.empty()) {
        delete queue.front();
        queue.pop();
    }
}

unsigned int
Fetch2::numInFlightFetches()
{
    return requests.occupiedSpace() +
        transfers.occupiedSpace();
}


bool
Fetch2::recvTimingResp(PacketPtr response)
{
    DPRINTF(CClassCPU, "recvTimingResp %d\n", numFetchesInMemorySystem);

    /* Only push the response if we didn't change stream?  No,  all responses
     *  should hit the responses queue.  It's the job of 'step' to throw them
     *  away. */
    FetchRequestPtr fetch_request = safe_cast<FetchRequestPtr>
        (response->popSenderState());

    /* Fixup packet in fetch_request as this may have changed */
    assert(!fetch_request->packet);
    fetch_request->packet = response;

    numFetchesInMemorySystem--;
    fetch_request->state = FetchRequest::Complete;

    if (response->isError()) {
        DPRINTF(CClassCPU, "Received error response packet: %s\n",
            fetch_request->id);
    }

    cpu.wakeupOnEvent(Pipeline::Fetch1StageId);
    cpu.wakeupOnEvent(Pipeline::Fetch2StageId);

    return true;

    //if (debug::MinorTrace)
}
    
void
Fetch2::recvReqRetry()
{
    DPRINTF(CClassCPU, "recvRetry\n");
    assert(icacheState == IcacheNeedsRetry);
    assert(!requests.empty());

    FetchRequestPtr retryRequest = requests.front();

    icacheState = IcacheRunning;

    if (tryToSend(retryRequest))
        moveFromRequestsToTransfers(retryRequest);
}


void
Fetch2::processResponse(Fetch2::FetchRequestPtr response,
    ForwardLineData &line, const Fetch1ThreadInfo* thread)
{
    PacketPtr packet = response->packet;

    /* Pass the prefetch abort (if any) on to Fetch2 in a ForwardLineData
     * structure */
    line.setFault(response->fault);
    /* Make sequence numbers valid in return */
    line.id = response->id;
    /* Set the PC in case there was a sequence change */
    set(line.pc, thread->pc);
    /* Set fetch address to virtual address */
    line.fetchAddr = response->pc;
    /* Set the lineBase, which is a sizeof(MachInst) aligned address <=
     *  pc.instAddr() */
    line.lineBaseAddr = response->request->getVaddr();

    if (response->fault != NoFault) {
        /* Stop fetching if there was a fault */
        /* Should probably try to flush the queues as well, but we
         * can't be sure that this fault will actually reach Execute, and we
         * can't (currently) selectively remove this stream from the queues */
        DPRINTF(CClassCPU, "Stopping line fetch because of fault: %s\n",
            response->fault->name());
        //thread->state = Fetch1State::PCWaitingForChange;
        line.setFault(response->fault);

        std::cout << "fault encountered!"<<std::endl;

   }else {
        //std::cout << "you've not encountered a fault!"<<std::endl;

        line.adoptPacketData(packet);
        /* Null the response's packet to prevent the response from trying to
         *  deallocate the packet */
        response->packet = NULL;
}
        std::cout << "im returning man" << std::endl;
}
void 
Fetch2::evaluate(){
    ThreadID fetch_tid = in_thread.outputWire->tid;
    const Fetch1ThreadInfo* thread = getInput(fetch_tid);

if (!in_thread.outputWire->isBubble()){
        inputBuffer[fetch_tid].setTail(*in_thread.outputWire);
        DPRINTF(CClassCPU,"you have set to the inputBuffer !!\n");
    }

if(thread)
{
    //DPRINTF(CClassCPU, "fetch2 is evaluating thread %d\n", fetch_tid);

    ForwardLineData &line_out = *out.inputWire;
  
    if (fetch_tid != InvalidThreadID) {
            //DPRINTF(CClassCPU, "Fetching from thread %d\n", fetch_tid);
            if(numInFlightFetches() < fetchLimit){
            /* Generate fetch to selected thread */
            finaldebugprint(fetch_tid,thread);
            fetchLine(fetch_tid,thread);
            popInput(fetch_tid);

            /* Take up a slot in the fetch queue */
            nextStageReserve[fetch_tid].reserve();
            }
    }

    stepQueues();

    
    if (!transfers.empty() &&
        transfers.front()->isComplete())
    {
        Fetch2::FetchRequestPtr response = transfers.front();
        DPRINTF(CClassCPU,"we are here stepping queues\n");

        if (response->isDiscardable()) {
            nextStageReserve[response->id.threadId].freeReservation();

            DPRINTF(CClassCPU, "Discarding translated fetch as it's for"
                " an old stream\n");

            /* Wake up next cycle just in case there was some other
             *  action to do */
            cpu.wakeupOnEvent(Pipeline::Fetch2StageId);

        } else {

            DPRINTF(CClassCPU, "Processing fetched line: %d\n"
               ,response->id);

            processResponse(response, line_out,thread);

        }
        popAndDiscard(transfers);
        //popInput(fetch_tid);
    }
}
if (!in_thread.outputWire->isBubble()){
        DPRINTF(CClassCPU,"you have pushed to the inputBuffer !!\n");
        inputBuffer[fetch_tid].pushTail();
        
}
}


bool
Fetch2::tryToSend(FetchRequestPtr request)
{
    bool ret = false;

    if (icachePort.sendTimingReq(request->packet)) {
        /* Invalidate the fetch_requests packet so we don't
         *  accidentally fail to deallocate it (or use it!)
         *  later by overwriting it */
        request->packet = NULL;
        request->state = FetchRequest::RequestIssuing;
        numFetchesInMemorySystem++;

        ret = true;

        DPRINTF(CClassCPU, "Issued fetch request to memory: %s\n",
            request->id);
    } else {
        /* Needs to be resent, wait for that */
        icacheState = IcacheNeedsRetry;

        DPRINTF(CClassCPU, "Line fetch needs to retry: %s\n",
            request->id);
    }

    return ret;
}


bool Fetch2::FetchRequest::isDiscardable() const
{
    const Fetch1ThreadInfo *thread = fetch.getInput(id.threadId);


    /* Can't discard lines in TLB/memory */
    return state != InTranslation && state != RequestIssuing &&
        (id.streamSeqNum != thread->streamSeqNum ||
        id.predictionSeqNum != thread->predictionSeqNum);
}

void
Fetch2::popInput(ThreadID tid)
{
    if (!inputBuffer[tid].empty()) {
        inputBuffer[tid].pop();
    }
    DPRINTF(CClassCPU,"have popped from isb\n");

    //fetchInfo[tid].inputIndex = 0;
}

const Fetch1ThreadInfo *
Fetch2::getInput(ThreadID tid)
{
    /* Get a line from the inputBuffer to work with */
    if(!inputBuffer[tid].empty()) {
        return &(inputBuffer[tid].front());
    }
    else 
    return NULL;
}
}//namespace cclass
}//namespace gem5
#endif /*__CPU_CCLASS_FETCH2_CC__*/
