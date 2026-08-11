

/**
 * @file
 *
 *  This file contains miscellaneous classes and functions for formatting
 *  general trace information and also CClassTrace information.
 *
 */

#ifndef __CPU_CCLASS_TRACE_HH__
#define __CPU_CCLASs_TRACE_HH__

#include <string>

#include "base/named.hh"
#include "base/trace.hh"
#include "debug/CClassTrace.hh"

namespace gem5
{

namespace cclass
{

/** DPRINTFN for CClassTrace reporting */
template <class ...Args>
inline void
cclassTrace(const char *fmt, Args ...args)
{
    DPRINTF(CClassTrace, (std::string("CClassTrace: ") + fmt).c_str(), args...);
}

/** DPRINTFN for CClassTrace CClassInst line reporting */
template <class ...Args>
inline void
cclassInst(const Named &named, const char *fmt, Args ...args)
{
    DPRINTFS(CClassTrace, &named, (std::string("CClassInst: ") + fmt).c_str(),
             args...);
}

/** DPRINTFN for CClassTrace CClassLine line reporting */
template <class ...Args>
inline void
cclassLine(const Named &named, const char *fmt, Args ...args)
{
    DPRINTFS(CClassTrace, &named, (std::string("CClassLine: ") + fmt).c_str(),
             args...);
}

} // namespace cclass
} // namespace gem5

#endif /* __CPU_CCLASS_TRACE_HH__ */
