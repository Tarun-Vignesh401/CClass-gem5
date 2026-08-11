from m5.defines import buildEnv
from m5.objects.FuncUnit import OpClass
from m5.objects.TimingExpr import TimingExpr
from m5.objects.BaseCPU import BaseCPU
from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject



class CClassOpClass(SimObject):
    """Boxing of OpClass to get around build problems and provide a hook for
    future additions to OpClass checks"""

    type = "CClassOpClass"
    cxx_header = "cpu/cclass/func_unit.hh"
    cxx_class = "gem5::CClassOpClass"

    opClass = Param.OpClass("op class to match")


class CClassOpClassSet(SimObject):
    """A set of matchable op classes"""

    type = "CClassOpClassSet"
    cxx_header = "cpu/cclass/func_unit.hh"
    cxx_class = "gem5::CClassOpClassSet"

    opClasses = VectorParam.CClassOpClass(
        [], "op classes to be matched.  An empty list means any class"
    )


class CClassFUTiming(SimObject):
    type = "CClassFUTiming"
    cxx_header = "cpu/cclass/func_unit.hh"
    cxx_class = "gem5::CClassFUTiming"

    mask = Param.UInt64(0, "mask for testing ExtMachInst")
    match = Param.UInt64(
        0,
        "match value for testing ExtMachInst:"
        " (ext_mach_inst & mask) == match",
    )
    suppress = Param.Bool(
        False, "if true, this inst. is not executed by this FU"
    )
    extraCommitLat = Param.Cycles(
        0, "extra cycles to stall commit for this inst."
    )
    extraCommitLatExpr = Param.TimingExpr(
        NULL, "extra cycles as a run-time evaluated expression"
    )
    extraAssumedLat = Param.Cycles(
        0,
        "extra cycles to add to scoreboard"
        " retire time for this insts dest registers once it leaves the"
        " functional unit.  For mem refs, if this is 0, the result's time"
        " is marked as unpredictable and no forwarding can take place.",
    )
    srcRegsRelativeLats = VectorParam.Cycles(
        "the maximum number of cycles"
        " after inst. issue that each src reg can be available for this"
        " inst. to issue"
    )
    opClasses = Param.CClassOpClassSet(
        CClassOpClassSet(),
        "op classes to be considered for this decode.  An empty set means any"
        " class",
    )
    description = Param.String(
        "", "description string of the decoding/inst class"
    )


def cclassMakeOpClassSet(op_classes):
    """Make a CClassOpClassSet from a list of OpClass enum value strings"""

    def boxOpClass(op_class):
        return CClassOpClass(opClass=op_class)

    return CClassOpClassSet(opClasses=[boxOpClass(o) for o in op_classes])


class CClassFU(SimObject):
    type = "CClassFU"
    cxx_header = "cpu/cclass/func_unit.hh"
    cxx_class = "gem5::CClassFU"

    opClasses = Param.CClassOpClassSet(
        CClassOpClassSet(),
        "type of operations allowed on this functional unit",
    )
    opLat = Param.Cycles(1, "latency in cycles")
    issueLat = Param.Cycles(
        1, "cycles until another instruction can be issued"
    )
    timings = VectorParam.CClassFUTiming([], "extra decoding rules")

    cantForwardFromFUIndices = VectorParam.Unsigned(
        [],
        "list of FU indices from which this FU can't receive and early"
        " (forwarded) result",
    )


class CClassFUPool(SimObject):
    type = "CClassFUPool"
    cxx_header = "cpu/cclass/func_unit.hh"
    cxx_class = "gem5::CClassFUPool"

    funcUnits = VectorParam.CClassFU("functional units")


class CClassDefaultIntFU(CClassFU):
    opClasses = cclassMakeOpClassSet(["IntAlu"])
    timings = [CClassFUTiming(description="Int", srcRegsRelativeLats=[2])]
    opLat = 3

class CClassDefaultIntMulFU(CClassFU):
    opClasses = cclassMakeOpClassSet(["IntMult"])
    timings = [CClassFUTiming(description="Mul", srcRegsRelativeLats=[0])]
    opLat = 3


class CClassDefaultIntDivFU(CClassFU):
    opClasses = cclassMakeOpClassSet(["IntDiv"])
    issueLat = 9
    opLat = 9


class CClassDefaultFloatFU(CClassFU):
    opClasses = cclassMakeOpClassSet(
        [
            "FloatAdd",
            "FloatCmp",
            "FloatCvt",
            "FloatMisc",
            "FloatMult",
            "FloatMultAcc",
            "FloatDiv",
            "FloatSqrt",
        ]
    )

    timings = [CClassFUTiming(description="Float", srcRegsRelativeLats=[2])]
    opLat = 6



class CClassDefaultMemFU(CClassFU):
    opClasses = cclassMakeOpClassSet(
        [
            "MemRead",
            "MemWrite",
            "FloatMemRead",
            "FloatMemWrite",
        ]
    )
    timings = [
        CClassFUTiming(
            description="Mem", srcRegsRelativeLats=[1], extraAssumedLat=2
        )
    ]
    opLat = 1

class CClassDefaultFUPool(CClassFUPool):
    funcUnits = [
        CClassDefaultIntFU(),
        CClassDefaultIntFU(),
        CClassDefaultIntMulFU(),
        CClassDefaultIntDivFU(),
        CClassDefaultFloatFU(),
        CClassDefaultMemFU(),
    ]


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
    fetch2LineSnapWidth = Param.Unsigned( 0, "Fetch1 'line' fetch snap size in bytes (0 means use system cache line size)")
    fetch2LineWidth = Param.Unsigned(0,"Fetch1 maximum fetch size in bytes (0 means use system cache line size)")
    fetch2ToDecodeForwardDelay = Param.Cycles(1, "reserved for later stages")
    decodeToExecuteForwardDelay = Param.Cycles(1, "reserved for later stages")
    executeBranchDelay = Param.Cycles(1, "reserved for later execute redirect")
    fetch2InputBufferSize = Param.Unsigned(2, "Size of input buffer to Fetch2 in cycles-worth of insts.")
    decodeInputBufferSize = Param.Unsigned(3, "Size of input buffer to Decode in cycles-worth of insts.")
    executeInputBufferSize = Param.Unsigned(3, "Size of input buffer to Decode in cycles-worth of insts.")
    decodeCycleInput = Param.Bool(True,
        "Allow Fetch2 to cross input lines to generate full output each"
        " cycle")
    decodeInputWidth = Param.Unsigned(2,
        "Width (in instructions) of input to Decode (and implicitly"
        " Decode's own width)")
    executeInputBufferLimit =  Param.Unsigned(7,
        "Width (in instructions) of input from exec to mem")
    executeIssueLimit = Param.Unsigned(2,
        "No. of Instructions that can be issued to FU's at once")
    executeFuncUnits = Param.CClassFUPool(
        CClassDefaultFUPool(), "FUlines for this processor"
    )
    executeAllowEarlyMemoryIssue = Param.Bool(0,"Allowing early issue of memory instructions")
    executeMemoryIssueLimit = Param.Unsigned(1, "Maximum amount of memory issues in one cycle")
    executeToMemoryForwardDelay = Param.Cycles(1, "Execute to memory forward delay")
    def addCheckerCpu(self):
        print("Checker not supported by CClassCPU")
        exit(1)
