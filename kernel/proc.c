// 进程相关
#include "riscv.h"
#include "types.h"

int cpuid() {
  int id = r_tp();
  return id;
}