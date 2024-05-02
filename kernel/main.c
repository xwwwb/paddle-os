// 初始化操作系统
#include "types.h"
#include "riscv.h"
#include "defs.h"

int main() {
  // 初始化第一个CPU
  if (cpuid() == 0) {
    consoleinit();  // 初始化控制台
    printfinit();   // 初始化printf
    printf("paddle-os kernel is booting......\n");
    kinit();        // 物理内存页分配初始化
    kvminit();      // 内核的页表初始化
    kvminithart();  // 开启内核页表
    asm volatile("wfi");
  } else {
    while (1) {
    }
  }
}