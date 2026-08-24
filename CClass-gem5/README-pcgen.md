```sh
rm -rf gem5/src/cpu/cclass && cp -r CClass-gem5/src/cpu gem5/src/cpu/cclass
rm -f gem5/build/RISCV/cpu/cclass/BaseMinorCPU.py.cc gem5/build/RISCV/cpu/cclass/BaseMinorCPU.py.o gem5/build/RISCV/cpu/cclass/BaseMinorCPU.py.pyo
cd gem5 && scons build/RISCV/gem5.opt -j$(nproc) && cd ..
./gem5/build/RISCV/gem5.opt CClass-gem5/src/configs/simple-riscv.py

./gem5/build/RISCV/gem5.opt     ./gem5/configs/learning_gem5/part1/simple-riscv.py

./gem5/build/RISCV/gem5.opt \
    --debug-flags=CClassCPU,CClassFetch,CClassPipeline \
    --debug-file=cclass.log \
    ./gem5/configs/learning_gem5/part1/simple-riscv.py

# fallback only if generated params race on an unpatched tree: cd gem5 && scons build/RISCV/params/System.hh -j1
```
