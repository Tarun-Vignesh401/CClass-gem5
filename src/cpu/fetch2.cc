
#ifndef __CPU_CCLASS_FETCH2_CC__
#define __CPU_CCLASS_FETCH2_CC__


#include "cpu/cclass/fetch2.hh"

#include "cpu/cclass/fetch1.hh"
#include "cpu/cclass/pipe_data.hh"
#include "cpu/cclass/cpu.hh"
#include "debug/CClassCPU.hh"


namespace gem5
{
namespace cclass {

Fetch2::Fetch2(const std::string &name, CClassCPU &cpu_,
    const BaseCClassCPUParams &params,
    Latch<Fetch1ThreadInfo>::Output in_thread_):
    Named(name),
    cpu(cpu_),
    in_thread(in_thread_)
{    
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
    inputBuffer.push_back(
    InputBuffer<Fetch1ThreadInfo>(
    name + ".inputBuffer" + std::to_string(tid), "lines",
    params.fetch2InputBufferSize));
    }

}

void Fetch2::finaldebugprint()
{
    const Fetch1ThreadInfo &thread = *in_thread.outputWire;

    DPRINTF(CClassCPU, "Data received in fetch2 !!\n");
    DPRINTF(CClassCPU, "State: %d\n", thread.state);
    if (thread.pc) {
        DPRINTF(CClassCPU, "PC: %s\n", *thread.pc);
    }
    DPRINTF(CClassCPU, "streamSeqNum: %d\n", thread.streamSeqNum);
    DPRINTF(CClassCPU, "predictionSeqNum: %d\n", thread.predictionSeqNum);
    DPRINTF(CClassCPU, "blocked: %d\n", thread.blocked);
    DPRINTF(CClassCPU, "FetchAddr: %#x\n", thread.FetchAddr);
}

void
Fetch2::fetchLine(ThreadID tid)
{
    /* Reference the currently used thread state. */
    Fetch1ThreadInfo &thread = *in_thread.outputWire;

    /* If line_offset != 0, a request is pushed for the remainder of the
     * line. */
    /* Use a lower, sizeof(MachInst) aligned address for the fetch */
    Addr aligned_pc = thread.fetchAddr & ~((Addr) lineSnap - 1);
    unsigned int line_offset = aligned_pc % lineSnap;
    unsigned int request_size = maxLineWidth - line_offset;

    /* Fill in the line's id */
    InstId request_id(tid,
        thread.streamSeqNum, thread.predictionSeqNum,
        lineSeqNum);

    FetchRequestPtr request = new FetchRequest(*this, request_id,
            thread.fetchAddr);

    DPRINTF(Fetch, "Inserting fetch into the fetch queue "
        "%s addr: 0x%x pc: %s line_offset: %d request_size: %d\n",
        request_id, aligned_pc, thread.fetchAddr, line_offset, request_size);

    request->request->setContext(cpu.threads[tid]->getTC()->contextId());
    request->request->setVirt(
        aligned_pc, request_size, Request::INST_FETCH, cpu.instRequestorId(),
        /* I've no idea why we need the PC, but give it */
        thread.fetchAddr);

    DPRINTF(Fetch, "Submitting ITLB request\n");
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
    //thread.fetchAddr = aligned_pc + request_size;
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
    fetch.cpu.wakeupOnEvent(Pipeline::Fetch1StageId);
}

void
Fetch2::handleTLBResponse(FetchRequestPtr response)
{
    numFetchesInITLB--;

    if (response->fault != NoFault) {
        DPRINTF(Fetch, "Fault in address ITLB translation: %s, "
            "paddr: 0x%x, vaddr: 0x%x\n",
            response->fault->name(),
            (response->request->hasPaddr() ?
                response->request->getPaddr() : 0),
            response->request->getVaddr());

        if (debug::MinorTrace)
            minorTraceResponseLine(name(), response);
    } else {
        DPRINTF(Fetch, "Got ITLB response\n");
    }

    response->state = FetchRequest::Translated;

    tryToSendToTransfers(response);
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
        DPRINTF(Fetch, "Fetch not at front of requests queue, can't"
            " issue to memory\n");
        return;
    }

    if (request->state == FetchRequest::InTranslation) {
        DPRINTF(Fetch, "Fetch still in translation, not issuing to"
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
            moveFromRequestsToTransfers(request);
    } else {
        DPRINTF(Fetch, "Not advancing line fetch\n");
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
        DPRINTF(Fetch, "Step in state %s moving to state %s\n",
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
    DPRINTF(Fetch, "recvTimingResp %d\n", numFetchesInMemorySystem);

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

    if (debug::MinorTrace)
        minorTraceResponseLine(name(), fetch_request);

    if (response->isError()) {
        DPRINTF(Fetch, "Received error response packet: %s\n",
            fetch_request->id);
    }

    /* We go to idle even if there are more things to do on the queues as
     *  it's the job of step to actually step us on to the next transaction */

    /* Let's try and wake up the processor for the next cycle to move on
     *  queues */
    cpu.wakeupOnEvent(Pipeline::Fetch1StageId);

    /* Never busy */
    return true;
}

void
Fetch2::recvReqRetry()
{
    DPRINTF(Fetch, "recvRetry\n");
    assert(icacheState == IcacheNeedsRetry);
    assert(!requests.empty());

    FetchRequestPtr retryRequest = requests.front();

    icacheState = IcacheRunning;

    if (tryToSend(retryRequest))
        moveFromRequestsToTransfers(retryRequest);
}


void
Fetch2::processResponse(Fetch2::FetchRequestPtr response,
    ForwardLineData &line)
{
    Fetch1ThreadInfo &thread = *in_thread.outputWire;
    PacketPtr packet = response->packet;

    /* Pass the prefetch abort (if any) on to Fetch2 in a ForwardLineData
     * structure */
    line.setFault(response->fault);
    /* Make sequence numbers valid in return */
    line.id = response->id;
    /* Set the PC in case there was a sequence change */
    set(line.pc, thread.pc);
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
        DPRINTF(Fetch, "Stopping line fetch because of fault: %s\n",
            response->fault->name());
        thread.state = Fetch1::FetchWaitingForPC;
    } else {
        line.adoptPacketData(packet);
        /* Null the response's packet to prevent the response from trying to
         *  deallocate the packet */
        response->packet = NULL;
    }
}







}//namespace cclass
}//namespace gem5
#endif /*__CPU_CCLASS_FETCH2_CC__*/
