
#ifndef __CPU_CCLASS_DYN_INST_HH__
#define __CPU_CCLASS_DYN_INST_HH__

#include <iostream>

#include "arch/generic/isa.hh"
#include "base/named.hh"
#include "base/refcnt.hh"
#include "base/types.hh"
#include "cpu/inst_seq.hh"
#include "cpu/cclass/buffers.hh"
#include "cpu/static_inst.hh"
#include "cpu/timing_expr.hh"
#include "sim/faults.hh"
#include "sim/insttracer.hh"



namespace gem5
{   
    namespace cclass{
        class InstId
{
  public:
    /** First sequence numbers to use in initialisation of the pipeline and
     *  to be expected on the first line/instruction issued */
    static const InstSeqNum firstStreamSeqNum = 1;
    static const InstSeqNum firstPredictionSeqNum = 1;
    static const InstSeqNum firstLineSeqNum = 1;
    static const InstSeqNum firstFetchSeqNum = 1;
    static const InstSeqNum firstExecSeqNum = 1;

  public:
    /** The thread to which this line/instruction belongs */
    ThreadID threadId;

    /** The 'stream' this instruction belongs to.  Streams are interrupted
     *  (and sequence numbers increased) when Execute finds it wants to
     *  change the stream of instructions due to a branch. */
    InstSeqNum streamSeqNum;

    /** The predicted qualifier to stream, attached by Fetch2 as a
     *  consequence of branch prediction */
    InstSeqNum predictionSeqNum;

    /** Line sequence number.  This is the sequence number of the fetched
     *  line from which this instruction was fetched */
    InstSeqNum lineSeqNum;

    /** Fetch sequence number.  This is 0 for bubbles and an ascending
     *  sequence for the stream of all fetched instructions */
    InstSeqNum fetchSeqNum;

    /** 'Execute' sequence number.  These are assigned after micro-op
     *  decomposition and form an ascending sequence (starting with 1) for
     *  post-micro-op decomposed instructions. */
    InstSeqNum execSeqNum;

  public:
    /** Very boring default constructor */
    InstId(
        ThreadID thread_id = 0, InstSeqNum stream_seq_num = 0,
        InstSeqNum prediction_seq_num = 0, InstSeqNum line_seq_num = 0,
        InstSeqNum fetch_seq_num = 0, InstSeqNum exec_seq_num = 0) :
        threadId(thread_id), streamSeqNum(stream_seq_num),
        predictionSeqNum(prediction_seq_num), lineSeqNum(line_seq_num),
        fetchSeqNum(fetch_seq_num), execSeqNum(exec_seq_num)
    { }

  public:
    /* Equal if the thread and last set sequence number matches */
    bool
    operator== (const InstId &rhs)
    {
        /* If any of fetch and exec sequence number are not set
         *  they need to be 0, so a straight comparison is still
         *  fine */
        bool ret = (threadId == rhs.threadId &&
            lineSeqNum == rhs.lineSeqNum &&
            fetchSeqNum == rhs.fetchSeqNum &&
            execSeqNum == rhs.execSeqNum);

        /* Stream and prediction *must* match if these are the same id */
        if (ret) {
            assert(streamSeqNum == rhs.streamSeqNum &&
                predictionSeqNum == rhs.predictionSeqNum);
        }

        return ret;
    }


};

inline std::ostream &
operator <<(std::ostream &os, const InstId &id)
{
    os << id.threadId << '/' << id.streamSeqNum << '.'
        << id.predictionSeqNum << '/' << id.lineSeqNum;

    /* Not all structures have fetch and exec sequence numbers */
    if (id.fetchSeqNum != 0) {
        os << '/' << id.fetchSeqNum;
        if (id.execSeqNum != 0)
            os << '.' << id.execSeqNum;
    }

    return os;
}

} //namespace cclass
} //namespace gem5 

#endif /* __CPU_CCLASS_DYN_INST_HH__ */