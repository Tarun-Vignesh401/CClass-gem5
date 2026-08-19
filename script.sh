
 rm -rf gem5/src/cpu/cclass && cp -r CClass-gem5/src/cpu gem5/src/cpu/cclass
 cd gem5 && scons build/RISCV/gem5.opt -j$(nproc) && cd ..
  ./gem5/build/RISCV/gem5.opt     --debug-flags=CClassCPU     --debug-file=cclass.log     ./gem5/configs/learning_gem5/part1/simple-riscv.py


