#include "cpu/cclass/decode.hh"

#include "arch/generic/decoder.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "cpu/cclass/pipeline.hh"
#include "debug/Decode.hh"

namespace gem5
{

namespace cclass
{

Decode::Decode(const std::string &name,
    CClassCPU &cpu_,
    const BaseCClassCPUParams &params,
    Latch<ForwardLineData>::Output inp_):
    Named(name),
    cpu(cpu_),
    inp(inp_),
    threadPriority(0)
{
    /* Per-thread input buffers */
    for (ThreadID tid = 0; tid < params.numThreads; tid++) {
        inputBuffer.push_back(
            InputBuffer<ForwardLineData>(
                name + ".inputBuffer" + std::to_string(tid), "lines",
                params.decodeInputBufferSize));
    }
}

}
}