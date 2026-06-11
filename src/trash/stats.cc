
#include "cpu/cclass/stats.hh"

namespace gem5
{

namespace cclass
{

CClassStats::CClassStats(BaseCPU *base_cpu)
    : statistics::Group(base_cpu),
    ADD_STAT(quiesceCycles, statistics::units::Cycle::get(),
             "Total number of cycles that CPU has spent quiesced or waiting "
             "for an interrupt")
{
    quiesceCycles.prereq(quiesceCycles);
}

} // namespace cclass
} // namespace gem5
