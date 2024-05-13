// 初始化操作系统
#include "includes/types.h"
#include "includes/riscv.h"
#include "includes/defs.h"

// 标记0号CPU的启动状态
volatile static int started = 0;

int main() {
  // 初始化第一个CPU
  if (cpuid() == 0) {
    consoleinit();       // 初始化控制台
    printfinit();        // 初始化printf
    bootinfo();          // 打印启动信息
    kinit();             // 物理内存页分配初始化
    kvminit();           // 内核的页表初始化
    kvminithart();       // 开启内核页表
    procinit();          // 进程描述表初始化
    trapinit();          // 初始化陷入
    trapinithart();      // 初始化陷入处理函数
    plicinit();          // 设置uart和虚拟io的plic优先级
    plicinithart();      // 让PLIC等待设备中断
    binit();             // 初始化IO缓存双向循环链表
    iinit();             // inode 初始化
    fileinit();          // 初始化文件表
    virtio_disk_init();  // 初始化虚拟硬盘
    userinit();          // 初始化第一个程序 init
    printf("hart %d starting:\t\t done!\n", cpuid());
    // 一定程度保证了原子化
    __sync_synchronize();
    started = 1;
  } else {
    while (started == 0);
    __sync_synchronize();
    kvminithart();   // 开启内核页表
    trapinithart();  // 初始化陷入处理函数
    plicinithart();  // 让PLIC等待设备中断
    printf("hart %d starting:\t\t done!\n", cpuid());
  }
  // 开启进程调度
  scheduler();
}