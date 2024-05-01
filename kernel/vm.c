#include "types.h"
#include "defs.h"
#include "riscv.h"

pagetable_t kernel_pagetable;

// 内核 虚拟内存页表
pagetable_t kvmmake(void) {
  pagetable_t kpgtbl;
  // 给页表分配一个页的大小
  kpgtbl = (pagetable_t)kalloc();
  // 全部清0
  memset(kpgtbl, 0, PGSIZE);
}

// 初始化内核页表
void kvminit(void) { kernel_pagetable = kvmmake(); }