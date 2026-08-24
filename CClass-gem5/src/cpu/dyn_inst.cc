
#include "cpu/cclass/dyn_inst.hh"

#include <iomanip>
#include <sstream>

#include "cpu/base.hh"
#include "cpu/cclass/trace.hh"
#include "cpu/null_static_inst.hh"
#include "cpu/reg_class.hh"
//#include "debug/MinorExecute.hh"
#include "enums/OpClass.hh"

namespace gem5
{

namespace cclass
{

const InstSeqNum InstId::firstStreamSeqNum;
const InstSeqNum InstId::firstPredictionSeqNum;
const InstSeqNum InstId::firstLineSeqNum;
const InstSeqNum InstId::firstFetchSeqNum;
const InstSeqNum InstId::firstExecSeqNum;

std::ostream &
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

CClassDynInstPtr CClassDynInst::bubbleInst = []() {
    auto *inst = new CClassDynInst(nullStaticInstPtr);
    assert(inst->isBubble());
    // Make bubbleInst immortal.
    inst->incref();
    return inst;
}();

bool
CClassDynInst::isLastOpInInst() const
{
    assert(staticInst);
    return !(staticInst->isMicroop() && !staticInst->isLastMicroop());
}

bool
CClassDynInst::isNoCostInst() const
{
    return isInst() && staticInst->opClass() == No_OpClass;
}

void
CClassDynInst::reportData(std::ostream &os) const
{
    if (isBubble())
        os << "-";
    else if (isFault())
        os << "F;" << id;
    else if (translationFault != NoFault)
        os << "TF;" << id;
    else
        os << id;
}

std::ostream &
operator <<(std::ostream &os, const CClassDynInst &inst)
{
    if (!inst.pc) {
        os << inst.id << " pc: 0x???????? (bubble)";
        return os;
    }

    os << inst.id << " pc: 0x"
        << std::hex << inst.pc->instAddr() << std::dec << " (";

    if (inst.isFault())
        os << "fault: \"" << inst.fault->name() << '"';
    else if (inst.translationFault != NoFault)
        os << "translation fault: \"" << inst.translationFault->name() << '"';
    else if (inst.staticInst)
        os << inst.staticInst->getName();
    else
        os << "bubble";

    os << ')';

    return os;
}

/** Print a register in the form r<n>, f<n>, m<n>(<name>) for integer,
 *  float, and misc given an 'architectural register number' */
static void
printRegName(std::ostream &os, const RegId& reg)
{
    switch (reg.classValue()) {
      case InvalidRegClass:
        os << 'z';
        break;
      case MiscRegClass:
        {
            RegIndex misc_reg = reg.index();
            os << 'm' << misc_reg << '(' << reg << ')';
        }
        break;
      case FloatRegClass:
        os << 'f' << reg.index();
        break;
      case VecRegClass:
        os << 'v' << reg.index();
        break;
      case VecElemClass:
        os << reg;
        break;
      case IntRegClass:
        os << 'r' << reg.index();
        break;
      case CCRegClass:
        os << 'c' << reg.index();
        break;
      default:
        panic("Unknown register class: %d", (int)reg.classValue());
    }
}

void
CClassDynInst::cclassTraceInst(const Named &named_object) const
{
    if (isFault()) {
        cclassInst(named_object, "id=F;%s addr=0x%x fault=\"%s\"\n",
            id, pc ? pc->instAddr() : 0, fault->name());
    } else {
        unsigned int num_src_regs = staticInst->numSrcRegs();
        unsigned int num_dest_regs = staticInst->numDestRegs();

        std::ostringstream regs_str;

        /* Format lists of src and dest registers for microops and
         *  'full' instructions */
        if (!staticInst->isMacroop()) {
            regs_str << " srcRegs=";

            unsigned int src_reg = 0;
            while (src_reg < num_src_regs) {
                printRegName(regs_str, staticInst->srcRegIdx(src_reg));

                src_reg++;
                if (src_reg != num_src_regs)
                    regs_str << ',';
            }

            regs_str << " destRegs=";

            unsigned int dest_reg = 0;
            while (dest_reg < num_dest_regs) {
                printRegName(regs_str, staticInst->destRegIdx(dest_reg));

                dest_reg++;
                if (dest_reg != num_dest_regs)
                    regs_str << ',';
            }

            ccprintf(regs_str, " extMachInst=%160x", staticInst->getEMI());
        }

        std::ostringstream flags;
        staticInst->printFlags(flags, " ");

        cclassInst(named_object, "id=%s addr=0x%x inst=\"%s\" class=%s"
            " flags=\"%s\"%s%s\n",
            id, pc ? pc->instAddr() : 0,
            (staticInst->opClass() == No_OpClass ?
                "(invalid)" : staticInst->disassemble(0,NULL)),
            enums::OpClassStrings[staticInst->opClass()],
            flags.str(),
            regs_str.str(),
            (predictedTaken ? " predictedTaken" : ""));
    }
}

CClassDynInst::~CClassDynInst()
{
    if (traceData)
        delete traceData;
}

} // namespace cclass
} // namespace gem5
