// 初始化操作系统
#include "types.h"
#include "riscv.h"
#include "defs.h"

int main() {
  // 初始化第一个CPU
  if (cpuid() == 0) {
    consoleinit();   // 初始化控制台
    printfinit();    // 初始化printf
    bootinfo();      // 打印启动信息
    kinit();         // 物理内存页分配初始化
    kvminit();       // 内核的页表初始化
    kvminithart();   // 开启内核页表
    procinit();      // 进程描述表初始化
    trapinit();      // 初始化陷入
    trapinithart();  // 初始化陷入处理函数
    plicinit();      // 设置uart和虚拟io的plic优先级
    plicinithart();  // 让PLIC等待设备中断
    binit();         // 初始化IO缓存双向循环链表
    iinit();         // inode 初始化
    fileinit();      // 初始化文件表
    userinit();      // 初始化第一个程序 init
  } else {
  }

  scheduler();
}