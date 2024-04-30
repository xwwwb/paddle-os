// 进程相关
#include "types.h"
#include "riscv.h"

int cpuid() {
  int id = r_tp();
  return id;
}