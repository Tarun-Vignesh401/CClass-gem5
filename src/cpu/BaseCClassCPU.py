from m5.objects.BaseCPU import BaseCPU
from m5.params import *


class CClassThreadPolicy(Enum):
    vals = ["SingleThread", "RoundRobin", "Random"]

class BaseCClassCPU(BaseCPU):
    type = "BaseCClassCPU"
    cxx_header = "cpu/cclass/cpu.hh"
    cxx_class = "gem5::CClassCPU"

    @classmethod
    def memory_mode(cls):
        return "timing"

    @classmethod
    def require_caches(cls):
        return True

    @classmethod
    def support_take_over(cls):
        return False

    threadPolicy = Param.CClassThreadPolicy(
        "SingleThread", "Thread scheduling policy"
    )
    fetch1FetchLimit = Param.Unsigned(1, "pcgen fetch requests per cycle")
    fetch1ToFetch2ForwardDelay = Param.Cycles(1, "pcgen output delay")
    fetch1ToFetch2BackwardDelay = Param.Cycles(1, "prediction feedback delay")
    fetch2ToDecodeForwardDelay = Param.Cycles(1, "reserved for later stages")
    decodeToExecuteForwardDelay = Param.Cycles(1, "reserved for later stages")
    executeBranchDelay = Param.Cycles(1, "reserved for later execute redirect")
    fetch2InputBufferSize = Param.Unsigned(2, "Size of input buffer to Fetch2 in cycles-worth of insts.")


    def addCheckerCpu(self):
        print("Checker not supported by CClassCPU")
        exit(1)
