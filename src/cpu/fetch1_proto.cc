namespace gem5
{
    namespace cclass
    {

        Fetch1::Fetch1(const std::string &name_, 
            CClassCPU &cpu_,
            const BaseCClassCPUParams &params,
            Latch<Fetch1ThreadInfo>::Output out_,
            std::vector<InputBuffer<Fetch1ThreadInfo>> &next_stage_input_buffer) :
            Named(name_),
            cpu(cpu_),
            nextStageReserve(next_stage_input_buffer),
            out(out_),
            fetchLimit(params.fetch1FetchLimit)
        {
        for (auto &info: fetchInfo)
            info.pc.reset(params.isa[0]->newPCState());
            
            
        if (fetchLimit < 1) {
        fatal("%s: fetch1FetchLimit must be >= 1 (%d)\n", name_,
            fetchLimit);
        }
        
    }
    ThreadID Fetch1::getScheduledThread(){

        //for now only one thread can be extended for multithreading support later
        return 0;
    }

    void evaluate(){
        const BranchData &execute_branch = *inp.outputWire;
        const BranchData &fetch2_branch = *prediction.outputWire;




    }





















    }
}