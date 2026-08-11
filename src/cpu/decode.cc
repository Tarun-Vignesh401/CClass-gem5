#include "cpu/cclass/decode.hh"

#include "arch/generic/decoder.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "cpu/cclass/pipeline.hh"



namespace gem5
{

namespace cclass
{

Decode::Decode(const std::string &name,
    CClassCPU &cpu_,
    const BaseCClassCPUParams &params,
    Latch<ForwardLineData>::Output inp_,
    Latch<ForwardInstData>::Input out_,
    std::vector<InputBuffer<ForwardInstData>> &next_stage_input_buffer) :
    Named(name),
    cpu(cpu_),
    out(out_),
    inp(inp_),
    nextStageReserve(next_stage_input_buffer),
    threadPriority(0),
    processMoreThanOneInput(params.decodeCycleInput),
    outputWidth(params.decodeInputWidth),
    decodeInfo(params.numThreads)
    //DecodeStats(cpu_)
    {
     if (outputWidth < 1)
        fatal("%s: decodeInputWidth must be >= 1 (%d)\n", name, outputWidth);

    if (params.decodeInputBufferSize < 1) {
        fatal("%s: decodeInputBufferSize must be >= 1 (%d)\n", name,
        params.decodeInputBufferSize);
    }
    /* Per-thread input buffers */
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        inputBuffer.push_back(
            InputBuffer<ForwardLineData>(
                name + ".inputBuffer" + std::to_string(tid), "lines",
                params.decodeInputBufferSize));
    }
}

const ForwardLineData *
Decode::getInput(ThreadID tid)
{
    /* Get a line from the inputBuffer to work with */
    if (!inputBuffer[tid].empty()) {
        return &(inputBuffer[tid].front());
    } else {
        return NULL;
    }
}

void
Decode::popInput(ThreadID tid)
{
    if (!inputBuffer[tid].empty()) {
        inputBuffer[tid].front().freeLine();
        inputBuffer[tid].pop();
    }

    decodeInfo[tid].inputIndex = 0;
}


void 
Decode::evaluate(){
    
if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->id.threadId].setTail(*inp.outputWire);
        
    ForwardInstData &insts_out = *out.inputWire;
    


 for (ThreadID tid = 0; tid < cpu.numThreads; tid++) {
        DecodeThreadInfo &thread = decodeInfo[tid];

        thread.blocked = !nextStageReserve[tid].canReserve();

        const ForwardLineData *line_in = getInput(tid);

        while (line_in &&
            thread.expectedStreamSeqNum == line_in->id.streamSeqNum &&
            thread.predictionSeqNum != line_in->id.predictionSeqNum)
        {
            DPRINTF(CClassCPU, "Discarding line %s"
                " due to predictionSeqNum mismatch (expected: %d)\n",
                line_in->id, thread.predictionSeqNum);

            popInput(tid);
            decodeInfo[tid].havePC = false;

            if (processMoreThanOneInput) {
                DPRINTF(CClassCPU, "Wrapping\n");
                line_in = getInput(tid);
            } else {
                line_in = NULL;
            }
        }
    }

    ThreadID tid = getScheduledThread();
    //DPRINTF(CClassCPU, "Scheduled Thread: %d\n", tid);

    assert(insts_out.isBubble());

    if (tid != InvalidThreadID) {
        DecodeThreadInfo &decode_info = decodeInfo[tid];
        unsigned wordOffset = decode_info.inputIndex & ~0x3;


        const ForwardLineData *line_in = getInput(tid);

        unsigned int output_index = 0;

        /* Pack instructions into the output while we can.  This may involve
         * using more than one input line.  Note that lineWidth will be 0
         * for faulting lines */
        while (line_in &&
            (line_in->isFault() ||
                decode_info.inputIndex < line_in->lineWidth) && /* More input */
            output_index < outputWidth /* More output to fill */)        {
            ThreadContext *thread = cpu.getContext(line_in->id.threadId);
            InstDecoder *decoder = thread->getDecoderPtr();

            /* Discard line due to prediction sequence number being wrong but
             * without the streamSeqNum number having changed */
            bool discard_line =
                decode_info.expectedStreamSeqNum == line_in->id.streamSeqNum &&
                decode_info.predictionSeqNum != line_in->id.predictionSeqNum;

            /* Set the PC if the stream changes.  Setting havePC to false in
             *  a previous cycle handles all other change of flow of control
             *  issues */
           bool set_pc = decode_info.lastStreamSeqNum != line_in->id.streamSeqNum;
// I don't think we would need this because we don't have pc until stream changes in fetch1

            if (!discard_line && (!decode_info.havePC || set_pc)) {
                /* Set the inputIndex to be the MachInst-aligned offset
                 *  from lineBaseAddr of the new PC value */
                
                 decode_info.inputIndex =
                    (line_in->pc->instAddr() /*& decoder->pcMask()*/) -
                    line_in->lineBaseAddr;
                
                DPRINTF(CClassCPU, "Setting new PC value: %s inputIndex: 0x%x"
                    " lineBaseAddr: 0x%x lineWidth: 0x%x\n",
                    *line_in->pc, decode_info.inputIndex, line_in->lineBaseAddr,
                    line_in->lineWidth);

                set(decode_info.pc, line_in->pc);
                decode_info.havePC = true;
                decoder->reset();
            }

            /* The generated instruction.  Leave as NULL if no instruction
             *  is to be packed into the output */
            CClassDynInstPtr dyn_inst = NULL;

            if (discard_line) {
                /* Rest of line was from an older prediction in the same
                 *  stream */
                DPRINTF(CClassCPU, "Discarding line %s (from inputIndex: %d)"
                    " due to predictionSeqNum mismatch (expected: %d)\n",
                    line_in->id, decode_info.inputIndex,
                    decode_info.predictionSeqNum);
            } else if (line_in->isFault()) {
                /* Pack a fault as a CClassDynInst with ->fault set */

                /* Make a new instruction and pick up the line, stream,
                 *  prediction, thread ids from the incoming line */
                dyn_inst = new CClassDynInst(nullStaticInstPtr, line_in->id);

                /* CClassCPU and prediction sequence numbers originate here */
                dyn_inst->id.fetchSeqNum = decode_info.fetchSeqNum;
                dyn_inst->id.predictionSeqNum = decode_info.predictionSeqNum;
                /* To complete the set, test that exec sequence number has
                 *  not been set */
                assert(dyn_inst->id.execSeqNum == 0);
                
                set(dyn_inst->pc, decode_info.pc);

                /* Pack a faulting instruction but allow other
                 *  instructions to be generated. (Decodes makes no
                 *  immediate judgement about streamSeqNum) */
                dyn_inst->fault = line_in->fault;
                DPRINTF(CClassCPU, "Fault being passed output_index: "
                    "%d: %s\n", output_index, dyn_inst->fault->name());

            } else {
                uint8_t *line = line_in->line;

                /* The instruction is wholly in the line, can just copy. */
                memcpy(decoder->moreBytesPtr(), line + wordOffset /*decode_info.inputIndex*/,
                        decoder->moreBytesSize());
                
                DPRINTF(CClassCPU,
                "DEC IN pc=%#lx lineBase=%#lx inputIndex=%u lineWidth=%u "
                "addr=%#lx bytes=%02x %02x %02x %02x\n",
                line_in->pc->instAddr(),
                line_in->lineBaseAddr,
             decode_info.inputIndex,
                line_in->lineWidth,
                line_in->lineBaseAddr + decode_info.inputIndex,
                line_in->line[0], line_in->line[1],
                line_in->line[2], line_in->line[3]);

                if (!decoder->instReady()) {
                    decoder->moreBytes(*decode_info.pc,
                        line_in->lineBaseAddr + wordOffset);
                    DPRINTF(CClassCPU, "Offering MachInst to decoder addr: 0x%x\n",
                            line_in->lineBaseAddr + decode_info.inputIndex);
                }

                DPRINTF(CClassCPU, "Decoding the line at the tick: %llu\n",
                        (unsigned long long)curTick());

                /* Maybe make the above a loop to accomodate ISAs with
                 *  instructions longer than sizeof(MachInst) */

                if (decoder->instReady()) {
                    /* Note that the decoder can update the given PC.
                     *  Remember not to assign it until *after* calling
                     *  decode */
                    StaticInstPtr decoded_inst =
                        decoder->decode(*decode_info.pc);
                    

                    /* Make a new instruction and pick up the line, stream,
                     *  prediction, thread ids from the incoming line */
                    dyn_inst = new CClassDynInst(decoded_inst, line_in->id);

                    /* CClassCPU and prediction sequence numbers originate here */
                    dyn_inst->id.fetchSeqNum = decode_info.fetchSeqNum;
                    dyn_inst->id.predictionSeqNum = decode_info.predictionSeqNum;
                    /* To complete the set, test that exec sequence number
                     *  has not been set */
                    assert(dyn_inst->id.execSeqNum == 0);
                    set(dyn_inst->pc, decode_info.pc);
                    DPRINTF(CClassCPU, "decoder inst %s\n", *dyn_inst);

                    // Collect some basic inst class stats
                    /*
                    if (decoded_inst->isLoad()) {
                        stats.loadInstructions++;
                    } else if (decoded_inst->isStore()) {
                        stats.storeInstructions++;
                    } else if (decoded_inst->isAtomic()) {
                        stats.amoInstructions++;
                    } else if (decoded_inst->isVector()) {
                        stats.vecInstructions++;
                    } else if (decoded_inst->isFloating()) {
                        stats.fpInstructions++;
                    } else if (decoded_inst->isInteger()) {
                        stats.intInstructions++;
                    }

                    stats.totalInstructions++;
                    cpu.fetchStats[tid]->numInsts++;*/

                    DPRINTF(CClassCPU, "Instruction extracted from line %s"
                        " lineWidth: %d output_index: %d inputIndex: %d"
                        " pc: %s inst: %s\n",
                        line_in->id,
                        line_in->lineWidth, output_index, decode_info.inputIndex,
                        *decode_info.pc, *dyn_inst);
                                


               //advance the pc for the next instruction in the line that is extracted...         
                    decoded_inst->advancePC(*decode_info.pc);


                } else {
                    DPRINTF(CClassCPU, "Inst not ready yet\n");
                }

                /* Step on the pointer into the line if there's no
                 *  complete instruction waiting */
                if(decode_info.inputIndex < line_in->lineWidth){
                    if (decoder->needMoreBytes()) {
                    decode_info.inputIndex += decoder->moreBytesSize();
                    }
                }
                else // move onto the next incoming line for decoding...
                    decode_info.inputIndex = 0;
              

                //if it is done in one go it is set to lineWidth f

            }

            if (dyn_inst) {
                /* Step to next sequence number */
                decode_info.fetchSeqNum++;

                /* Correctly size the output before writing */
                if (output_index == 0) {
                    insts_out.resize(outputWidth);
                }
                /* Pack the generated dynamic instruction into the output */
                insts_out.insts[output_index] = dyn_inst;
                output_index++;

                /* Output MinorTrace instruction info for
                 *  pre-microop decomposition macroops 
                if (debug::MinorTrace && !dyn_inst->isFault() &&
                    dyn_inst->staticInst->isMacroop()) {
                    dyn_inst->minorTraceInst(*this);
                }*/
            }

            /* Remember the streamSeqNum of this line so we can tell when
             *  we change stream */
            decode_info.lastStreamSeqNum = line_in->id.streamSeqNum;

            /* Asked to discard line or there was a branch or fault */
            if (line_in->isFault() /* A line which is just a fault */)
            {
                DPRINTF(CClassCPU, "Discarding all input on fault\n");
                dumpAllInput(tid);
                decode_info.havePC = false;
                line_in = NULL;
            } else if (discard_line) {
                /* Just discard one line, one's behind it may have new
                 *  stream sequence numbers.  There's a DPRINTF above
                 *  for this event */
                popInput(tid);
                decode_info.havePC = false;
                line_in = NULL;
            } else if (decode_info.inputIndex >= line_in->lineWidth) {
                /* Got to end of a line, pop the line but keep PC
                 *  in case this is a line-wrapping inst. */
                // Input index doesn't increment if it is decoded in one go!!! thats why
                popInput(tid);
                line_in = NULL;
            }

            if (!line_in && processMoreThanOneInput) {
                DPRINTF(CClassCPU, "Wrapping\n");
                line_in = getInput(tid);
            }
        }
            }
    if (tid == InvalidThreadID) {
        assert(insts_out.isBubble());
    }
    /* If we generated output, reserve space for the result in the next stage
     *  and mark the stage as being active this cycle */
    if (!insts_out.isBubble()) {
        insts_out.threadId = tid;
        nextStageReserve[tid].reserve();
    }    

    for (ThreadID i = 0; i < cpu.numThreads; i++)
    {
        if (getInput(i) && nextStageReserve[i].canReserve()) {
            cpu.wakeupOnEvent(Pipeline::DecodeStageId);
            break;
        }
    }


    if (!inp.outputWire->isBubble())
        inputBuffer[inp.outputWire->id.threadId].pushTail();
}
void
Decode::dumpAllInput(ThreadID tid)
{
    DPRINTF(CClassCPU, "Dumping whole input buffer\n");
    while (!inputBuffer[tid].empty())
        popInput(tid);

    decodeInfo[tid].inputIndex = 0;
}



/*
Decode::DecodeStats::DecodeStats(CClassCPU *cpu)
    : statistics::Group(cpu, "fetch2"),
      ADD_STAT(totalInstructions, statistics::units::Count::get(),
               "Total number of instructions successfully decoded"),
      ADD_STAT(intInstructions, statistics::units::Count::get(),
               "Number of integer instructions successfully decoded"),
      ADD_STAT(fpInstructions, statistics::units::Count::get(),
               "Number of floating point instructions successfully decoded"),
      ADD_STAT(vecInstructions, statistics::units::Count::get(),
               "Number of SIMD instructions successfully decoded"),
      ADD_STAT(loadInstructions, statistics::units::Count::get(),
               "Number of memory load instructions successfully decoded"),
      ADD_STAT(storeInstructions, statistics::units::Count::get(),
               "Number of memory store instructions successfully decoded"),
      ADD_STAT(amoInstructions, statistics::units::Count::get(),
               "Number of memory atomic instructions successfully decoded")
{
    totalInstructions.flags(statistics::total);
    intInstructions.flags(statistics::total);
    fpInstructions.flags(statistics::total);
    vecInstructions.flags(statistics::total);
    loadInstructions.flags(statistics::total);
    storeInstructions.flags(statistics::total);
    amoInstructions.flags(statistics::total);
}
*/

ThreadID Decode::getScheduledThread(){

        //for now only one thread can be extended for multithreading support later
    return 0;
}




}//namespace cclass
}//namespace gem5