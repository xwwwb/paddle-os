// CPU相关的代码
#include "types.h"
#include "riscv.h"
#include "proc.h"
#include "params.h"

struct cpu cpus[NCPU];

// start.c 将hartid存入了tp
int cpuid() {
  int id = r_tp();
  return id;
}

// 返回当前的CPU结构体
struct cpu* mycpu() {
  int id = cpuid();
  struct cpu* c = &cpus[id];
  return c;
}