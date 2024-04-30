// 初始化操作系统
#include "defs.h"

int main() {
  // 初始化第一个CPU
  if (cpuid() == 0) {
    consoleinit();  // 初始化控制台
    printfinit();   // 初始化printf
    printf("\n");
    printf("paddle-os kernel is booting!\n");
    asm volatile("wfi");
  } else {
    while (1) {
    }
  }
}