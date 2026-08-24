
/**
 * @file
 *
 * The stats for CClassCPU separated from the CPU definition.
 */

#ifndef __CPU_CCLASS_STATS_HH__
#define __CPU_CCLASS_STATS_HH__

#include "base/statistics.hh"
#include "cpu/base.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

namespace cclass
{

/** Currently unused stats class. */
struct CClassStats : public statistics::Group
{
    CClassStats(BaseCPU *parent);

    /** Number of cycles in quiescent state */
    statistics::Scalar quiesceCycles;

};

} // namespace cclass
} // namespace gem5

#endif /* __CPU_CCLASS_STATS_HH__ */
