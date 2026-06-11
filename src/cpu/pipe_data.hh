/*
 * Copyright (c) 2013-2014 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *
 *  Contains class definitions for data flowing between pipeline stages in
 *  the top-level structure portion of this model.  Latch types are also
 *  defined which pair forward/backward flowing data specific to each stage
 *  pair.
 *
 *  No post-configuration inter-stage communication should *ever* take place
 *  outside these classes (except for reservation!)
 */

#ifndef __CPU_CCLASS_PIPE_DATA_HH__
#define __CPU_CCLASS_PIPE_DATA_HH__

#include "cpu/cclass/buffers.hh"
#include "cpu/cclass/dyn_inst.hh"
#include "cpu/base.hh"

namespace gem5
{

namespace cclass
{
    
   //this enum is primarily for multithreading support.
  enum Fetch1State {
                PCGenHalted,
                PCGenRunning,
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

    bool wakeupGuard = false;
    //The Address we are fetching lines from
    Addr FetchAddr = 0;
    };

    class ForwardLineData{
        private:
    /** This line is a bubble.  No other data member is required to be valid
     *  if this is true
     *  Make lines bubbles by default */
    bool bubbleFlag = true;

  public:
    /** First byte address in the line.  This is allowed to be
     *  <= pc.instAddr() */
    Addr lineBaseAddr = 0;

    /** PC of the first inst within this sequence */
    std::unique_ptr<PCStateBase> pc;

    /** Address of this line of data */
    Addr fetchAddr;

    /** Explicit line width, don't rely on data.size */
    unsigned int lineWidth = 0;

  public:
    /** This line has a fault.  The bubble flag will be false and seqNums
     *  will be valid but no data will */
    Fault fault = NoFault;

    /** Thread, stream, prediction ... id of this line */
    InstId id;

    /** Line data.  line[0] is the byte at address pc.instAddr().  Data is
     *  only valid upto lineWidth - 1. */
    uint8_t *line = nullptr;

    /** Packet from which the line is taken */
    Packet *packet = nullptr;

  public:
    ForwardLineData() {}
    ForwardLineData(const ForwardLineData &other) :
        bubbleFlag(other.bubbleFlag), lineBaseAddr(other.lineBaseAddr),
        pc(other.pc->clone()), fetchAddr(other.fetchAddr),
        lineWidth(other.lineWidth), fault(other.fault), id(other.id),
        line(other.line), packet(other.packet)
    {}
    ForwardLineData &
    operator=(const ForwardLineData &other)
    {
        bubbleFlag = other.bubbleFlag;
        lineBaseAddr = other.lineBaseAddr;
        set(pc, other.pc);
        fetchAddr = other.fetchAddr;
        lineWidth = other.lineWidth;
        fault = other.fault;
        id = other.id;
        line = other.line;
        packet = other.packet;
        return *this;
    }

    ~ForwardLineData() { line = NULL; }

  public:
    /** This is a fault, not a line */
    bool isFault() const { return fault != NoFault; }

    /** Set fault and possible clear the bubble flag */
    void setFault(Fault fault_);

    /** In-place initialise a ForwardLineData, freeing and overridding the
     *  line */
    void allocateLine(unsigned int width_);

    /** Use the data from a packet as line instead of allocating new
     *  space.  On destruction of this object, the packet will be destroyed */
    void adoptPacketData(Packet *packet);

    /** Free this ForwardLineData line.  Note that these are shared between
     *  line objects and so you must be careful when deallocating them.
     *  Copying of ForwardLineData can, therefore, be done by default copy
     *  constructors/assignment */
    void freeLine();

    /** BubbleIF interface */
    static ForwardLineData bubble() { return ForwardLineData(); }
    bool isBubble() const { return bubbleFlag; }

    /** ReportIF interface */
    void reportData(std::ostream &os) const;
};

/** Maximum number of instructions that can be carried by the pipeline. */
const unsigned int MAX_FORWARD_INSTS = 16;






    }
} // namespace minor
} // namespace gem5

#endif /* __CPU_MINOR_PIPE_DATA_HH__ */
