

#include "cpu/cclass/pipe_data.hh"

#include <cstring>

namespace gem5
{

namespace cclass
{

ExecRequest::ExecRequest(Execute &execute_, CClassDynInstPtr inst_,
    bool is_load, uint8_t *store_data, unsigned int size, uint64_t *res_) :
    execute(execute_),
    inst(inst_),
    request(std::make_shared<Request>()),
    isLoad(is_load),
    isStore(!is_load),
    res(res_)
{
    assert(inst);
    assert(isLoad || isStore);

    if (isStore && size != 0) {
        data.resize(size);
        if (store_data) {
            std::memcpy(data.data(), store_data, size);
        } else {
            std::memset(data.data(), 0, size);
        }
    }
}

void
ExecRequest::markFault(Fault fault_)
{
    fault = fault_;
    state = Failed;
}

PacketPtr
ExecRequest::makePacket()
{
    assert(request);
    assert(!packet);

    packet = isLoad ? Packet::createRead(request)
                    : Packet::createWrite(request);
    packet->pushSenderState(this);

    if (isLoad) {
        packet->allocate();
    } else if (!request->isCacheMaintenance() && !data.empty()) {
        packet->dataStatic(data.data());
    }

    return packet;
}

void
ExecRequest::markComplete(PacketPtr response)
{
    packet = response;

    if (response->isError()) {
        state = Failed;
        return;
    }

    if (isLoad) {
        data.resize(response->getSize());
        response->writeData(data.data());
    }

    state = Complete;
}
void
ForwardLineData::setFault(Fault fault_)
{
    fault = fault_;
    if (isFault())
        bubbleFlag = false;
}

void
ForwardLineData::allocateLine(unsigned int width_)
{
    lineWidth = width_;
    bubbleFlag = false;

    assert(!isFault());
    assert(!line);

    line = new uint8_t[width_];
}

void
ForwardLineData::adoptPacketData(Packet *packet)
{
    this->packet = packet;
    lineWidth = packet->req->getSize();
    bubbleFlag = false;

    assert(!isFault());
    assert(!line);

    line = packet->getPtr<uint8_t>();
}

void
ForwardLineData::freeLine()
{
    /* Only free lines in non-faulting, non-bubble lines */
    if (!isFault() && !isBubble()) {
        assert(line);
        /* If packet is not NULL then the line must belong to the packet so
         *  we don't need to separately deallocate the line */
        if (packet) {
            delete packet;
        } else {
            delete [] line;
        }
        line = NULL;
        bubbleFlag = true;
    }
}

/*void
ForwardLineData::reportData(std::ostream &os) const
{
    if (isBubble())
        os << '-';
    else if (fault != NoFault)
        os << "F;" << id;
    else
        os << id;
}*/


ForwardInstData::ForwardInstData(unsigned int width, ThreadID tid) :
    numInsts(width), threadId(tid)
{
    bubbleFill();
}

ForwardInstData::ForwardInstData(const ForwardInstData &src)
{
    *this = src;
}

ForwardInstData &
ForwardInstData::operator =(const ForwardInstData &src)
{
    numInsts = src.numInsts;

    for (unsigned int i = 0; i < src.numInsts; i++)
        insts[i] = src.insts[i];

    return *this;
}

bool
ForwardInstData::isBubble() const
{
    return numInsts == 0 || insts[0]->isBubble();
}

void
ForwardInstData::bubbleFill()
{
    for (unsigned int i = 0; i < numInsts; i++)
        insts[i] = CClassDynInst::bubble();
}

void
ForwardInstData::resize(unsigned int width)
{
    assert(width < MAX_FORWARD_INSTS);
    numInsts = width;

    bubbleFill();
}
/*
void
ForwardInstData::reportData(std::ostream &os) const
{
    if (isBubble()) {
        os << '-';
    } else {
        unsigned int i = 0;

        os << '(';
        while (i != numInsts) {
            insts[i]->reportData(os);
            i++;
            if (i != numInsts)
                os << ',';
        }
        os << ')';
    }
}*/

std::ostream &
operator <<(std::ostream &os, BranchData::Reason reason)
{
    switch (reason)
    {
      case BranchData::NoBranch:
        os << "NoBranch";
        break;
      case BranchData::UnpredictedBranch:
        os << "UnpredictedBranch";
        break;
      case BranchData::BranchPrediction:
        os << "BranchPrediction";
        break;
      case BranchData::CorrectlyPredictedBranch:
        os << "CorrectlyPredictedBranch";
        break;
      case BranchData::BadlyPredictedBranch:
        os << "BadlyPredictedBranch";
        break;
      case BranchData::BadlyPredictedBranchTarget:
        os << "BadlyPredictedBranchTarget";
        break;
      case BranchData::Interrupt:
        os << "Interrupt";
        break;
      case BranchData::SuspendThread:
        os << "SuspendThread";
        break;
      case BranchData::HaltFetch:
        os << "HaltFetch";
        break;
    }

    return os;
}

bool
BranchData::isStreamChange(const BranchData::Reason reason)
{
    bool ret = false;

    switch (reason)
    {
        /* No change of stream (see the enum comment in pipe_data.hh) */
      case NoBranch:
      case CorrectlyPredictedBranch:
        ret = false;
        break;

        /* Change of stream (Fetch1 should act on) */
      case UnpredictedBranch:
      case BranchPrediction:
      case BadlyPredictedBranchTarget:
      case BadlyPredictedBranch:
      case SuspendThread:
      case Interrupt:
      case HaltFetch:
        ret = true;
        break;
    }

    return ret;
}

bool
BranchData::isBranch(const BranchData::Reason reason)
{
    bool ret = false;

    switch (reason)
    {
        /* No change of stream (see the enum comment in pipe_data.hh) */
      case NoBranch:
      case CorrectlyPredictedBranch:
      case SuspendThread:
      case Interrupt:
      case HaltFetch:
        ret = false;
        break;

        /* Change of stream (Fetch1 should act on) */
      case UnpredictedBranch:
      case BranchPrediction:
      case BadlyPredictedBranchTarget:
      case BadlyPredictedBranch:
        ret = true;
        break;
    }

    return ret;
}

void
BranchData::reportData(std::ostream &os) const
{
    if (isBubble()) {
        os << '-';
    } else {
        os << reason
            << ';' << newStreamSeqNum << '.' << newPredictionSeqNum
            << ";0x" << std::hex << target->instAddr() << std::dec
            << ';';
        inst->reportData(os);
    }
}

std::ostream &
operator <<(std::ostream &os, const BranchData &branch)
{
    os << branch.reason << " target: 0x"
        << std::hex << branch.target->instAddr() << std::dec
        << ' ' << *branch.inst
        << ' ' << branch.newStreamSeqNum << "(stream)."
        << branch.newPredictionSeqNum << "(pred)";

    return os;
}


} // namespace cclass
} // namespace gem5
