

#include "cpu/cclass/scoreboard.hh"

#include "cpu/reg_class.hh"
#include "debug/CClassCPU.hh"
#include "debug/CClassTiming.hh"

namespace gem5
{

namespace cclass
{

bool
Scoreboard::findIndex(const RegId& reg, Index &scoreboard_index)
{
    bool ret = false;

    switch (reg.classValue()) {
      case IntRegClass:
        scoreboard_index = reg.index();
        ret = true;
        break;
      case FloatRegClass:
        scoreboard_index = floatRegOffset + reg.index();
        ret = true;
        break;
      case InvalidRegClass:
        ret = false;
        break;
      default:
        panic("Unknown register class: %d", reg.classValue());
    }
    return ret;
}
/* anything that is accessed via index, means that the depth of that entry is the same as
* the number of architectural registers
* When we mark destinations we already mark with the instseqnums*/
void
Scoreboard::markupInstDests(CClassDynInstPtr inst,ThreadContext *thread_context)
{
    if (inst->isFault())
        return;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_dests = staticInst->numDestRegs();

    auto *isa = thread_context->getIsaPtr();

    /** Mark each destination register */
    for (unsigned int dest_index = 0; dest_index < num_dests; dest_index++)
    {
        RegId reg = staticInst->destRegIdx(dest_index).flatten(*isa);
        Index index;

        if (findIndex(reg, index)) {
            // to store the register index to clear in writeback
            inst->flatDestRegIdx[dest_index] = reg;

            if (inst->id.execSeqNum > rename_id[index] || rename_id[index] == 0)
                rename_id[index] = inst->id.execSeqNum;

            DPRINTF(CClassCPU, "Marking up inst: %s regIndex: %d\n", *inst, index);
        } else {
            /* Use an invalid ID to mark invalid/untracked dests */
            /* I have to see in wb to confirm this ! */
            inst->flatDestRegIdx[dest_index] = RegId();
        }
    }
}

InstSeqNum
Scoreboard::execSeqNumToWaitFor(CClassDynInstPtr inst,ThreadContext *thread_context)
{
    InstSeqNum ret = 0;

    if (inst->isFault())
        return ret;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_srcs = staticInst->numSrcRegs();

    auto *isa = thread_context->getIsaPtr();

    for (unsigned int src_index = 0; src_index < num_srcs; src_index++) {
        RegId reg = staticInst->srcRegIdx(src_index).flatten(*isa);
        unsigned short int index;

        if (findIndex(reg, index)) {
            if (rename_id[index] > ret)/*id > 0 return id* else return 0*/
                ret = rename_id[index];
        }
    }

    DPRINTF(CClassCPU, "Inst: %s depends on execSeqNum: %d\n",
        *inst, ret);

    return ret;
}

void
Scoreboard::clearScoreBoard(){
/* Set all rename_id's to 0*/
std::fill(rename_id.begin(), rename_id.end(), InstSeqNum{0});
}

void
Scoreboard::clearInstDests(CClassDynInstPtr inst)
{
    if (inst->isFault()){
        clearScoreBoard();
        return;
    }

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_dests = staticInst->numDestRegs();

    /** Mark each destination register */
    for (unsigned int dest_index = 0; dest_index < num_dests;
        dest_index++)
    {
        const RegId& reg = inst->flatDestRegIdx[dest_index];
        Index index;

        if (findIndex(reg, index)) {
                rename_id[index] = 0;

            DPRINTF(CClassCPU, "Clearing inst: %s"
                " regIndex: %d \n",
                *inst, index);
        }
    }
}


/*Scoreboard owns a copy of memory and wb classes to check if the seqnumber is present in
* the isb's or not*/
Scoreboard::forwardresult 
Scoreboard::checkExeIsbForId(ThreadID tid, InstSeqNum num)
{   
    forwardresult result = None;

    if (!baseBuf[tid].empty()){
        for(const auto& base : baseBuf[tid].getQueue()){
            if(base.containsExecSeqNum(tid, num)) result = Int;
        }
    }


    if (!mboxBuf[tid].empty()){
        for(const auto& mbox : mboxBuf[tid].getQueue()){
            if(mbox.containsExecSeqNum(tid, num)) result = None;
        }
    }


    if (!fboxBuf[tid].empty()){
        for(const auto& fbox : fboxBuf[tid].getQueue()){
            if(fbox.containsExecSeqNum(tid, num)) result = None;
        }
    }

    /*
    if (!baseBuf[tid].empty()){
        for(&auto base : baseBuf[tid])
            if(base.containsExecSeqNum(tid, num)) result = Int;
    }

    if (!mboxBuf[tid].empty() &&
        mboxBuf[tid].front().containsExecSeqNum(tid, num))
        result = None;

    if (!fboxBuf[tid].empty() &&
        fboxBuf[tid].front().containsExecSeqNum(tid, num))
        result = None;
    */
    /* this shouldn't happen, mem results are unpredictable
    *if (!memBuf[tid].empty() &&
    *    memBuf[tid].front().containsExecSeqNum(tid, num))
    *    return true;
    */
    return result;
}

Scoreboard::forwardresult
Scoreboard::checkMemIsbForId(ThreadID tid, InstSeqNum num)
{
    return None;
}

bool
Scoreboard::canInstIssue(CClassDynInstPtr inst,ThreadContext *thread_context)
{   
    /* for now I have to figure out to get the tid from thread_
    * context*/
    ThreadID tid = 0;
    /* Always allow fault to be issued */
    if (inst->isFault())
        return true;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_srcs = staticInst->numSrcRegs();

    /* Default to saying you can issue */
    bool ret = true;

    auto *isa = thread_context->getIsaPtr();

    /* For each source register, find the latest result */
    unsigned int src_index = 0;


    while (src_index < num_srcs && !ret)
    {      
        RegId reg = staticInst->srcRegIdx(src_index).flatten(*isa);
        unsigned short int index;
        
        if (findIndex(reg, index)) {
            if(rename_id[index] == 0){
                markupInstDests(inst ,thread_context);
                ret = true;
            }
            else{
                forwardresult can_forward = checkExeIsbForId(tid,rename_id[index]);
                if(can_forward != None)
                    ret = true;
                else 
                    ret = false;
            }
        }
        src_index++;
    }
    /*for jal auipc lui etc.. where num_srcs = 0*/
    /*if(num_srcs == 0 )
        ret = true;
    */

    return ret;
}

RegVal
Scoreboard::forwardRegResult(ThreadID tid, InstSeqNum num, const RegId &reg){
    if (!baseBuf[tid].empty()) {
        for (const auto &base : baseBuf[tid].getQueue()) {
            if (base.containsExecSeqNum(tid, num))
                return base.forwardRegResult(tid, num, reg);
        }
    }

    return 0;

}

/* result id to be extended to support float to float forwards in the future
* when we do float to float forwards we have to change the forwaded value to
* a vector of bytes instead of RegVal type*/

bool
Scoreboard::lookForForwards(ThreadID tid, const RegId &reg, RegVal& forwarded_value){

    Index index;
    if( findIndex(reg,index) ){
        if (rename_id[index] == 0)
            return false;
        forwardresult result_id = checkExeIsbForId(tid, rename_id[index]);
        if(result_id != None){
        forwarded_value = forwardRegResult( tid, rename_id[index], reg);
        return true;
        }
    }
    return false;

}

/*void
Scoreboard::minorTrace() const
{
    std::ostringstream result_stream;

    bool printed_element = false;

    unsigned int i = 0;
    while (i < numRegs) {
        unsigned short int num_results = numResults[i];
        unsigned short int num_unpredictable_results =
            numUnpredictableResults[i];

        if (!(num_results == 0 && num_unpredictable_results == Cycles(0))) {
            if (printed_element)
                result_stream << ',';

            result_stream << '(' << i << ','
                 << num_results << '/'
                 << num_unpredictable_results << '/'
                 << returnCycle[i] << '/'
                 << writingInst[i] << ')';

            printed_element = true;
        }

        i++;
    }

    //minor::minorTrace("busy=%s\n", result_stream.str());
}*/

} // namespace minor
} // namespace gem5
