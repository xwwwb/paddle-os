#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "defs.h"

pagetable_t kernel_pagetable;

// 为什么类型是char[]
extern char etext[];

// PTE是虚拟地址和物理地址的映射
// 虚拟地址通过12-39 找到PTE
// PTE记录当前页的物理地址 和权限
// 虚拟地址的前12位是页内偏移 物理地址+偏移就是虚拟地址

// 内核 虚拟内存页表
pagetable_t kvmmake(void) {
  pagetable_t kpgtbl;
  // 给页表分配一个页的大小
  kpgtbl = (pagetable_t)kalloc();
  // 全部清0
  memset(kpgtbl, 0, PGSIZE);

  // 注册uart的虚拟地址和物理地址的映射
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  // 虚拟硬盘内存地址映射
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  // 中断控制器
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  // 内核代码段 只读
  // 如果etext没有4k对齐
  // 在当前初始化时 最后一个页
  // end = start + size -1 = kernel_base + etext -kernel_base -1
  // end = etext - 1  end = 对齐(etext-1)
  // 下面的紧接着的代码的start就是 对齐(etext)
  // 对齐(etext-1) = 对齐(etext) 首尾重复
  // 当etext4k对齐时 对齐(etext-1) != 对齐(etext) 差一个页 正好正确
  kvmmap(kpgtbl, KERNEL_BASE, KERNEL_BASE, (uint64)etext - KERNEL_BASE,
         PTE_R | PTE_X);
  // 除去代码段其他的地址
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYMEMSTOP - (uint64)etext,
         PTE_R | PTE_W);

  // kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // // allocate and map a kernel stack for each process.
  // proc_mapstacks(kpgtbl);

  return kpgtbl;
}

// 初始化内核页表
void kvminit(void) {
  // 拿到页表的地址
  kernel_pagetable = kvmmake();
  printf("kernel virtual memroy init: \t done!\n");
}

// 开启分页
// 硬件的页表寄存器存入内核页表
void kvminithart() {
  // sfence_vma();
  // 写satp寄存器
  w_satp(MAKE_SATP(kernel_pagetable));
  // sfence_vma();
  printf("memory paging init: \t\t done!\n ");
};

// 内核 建立物理地址和虚拟地址的映射 并且设置当前虚拟地址的权限
void kvmmap(pagetable_t pagetable, uint64 virtual_address,
            uint64 physical_address, uint64 size, int flags) {
  if (mappages(pagetable, virtual_address, physical_address, size, flags) !=
      0) {
    panic("kvmmap error!");
  }
}

// 创建内存映射 主要创建PTE pagetable entry 也就是页表项
// 在这之后 请求虚拟地址 地址转换硬件会返回虚拟地址
// 这里的地址必须4k对齐 否则会出现前一个地址的尾部和当前地址的首部重合
int mappages(pagetable_t pagetable, uint64 virtual_address,
             uint64 physical_address, uint64 size, int flags) {
  uint64 start, end;
  pte_t* pte;

  // 大小不能是0
  if (size == 0) {
    panic("mappages size error!");
  }
  // 内存地址取整
  start = PGROUNDDOWN(virtual_address);
  end = PGROUNDDOWN(virtual_address + size - 1);
  // 开始循环 直到这部分的内存映射完成
  for (;;) {
    // 首先拿到当前虚拟内存的PTE
    if ((pte = walk(pagetable, start, 1)) == 0) {
      // 说明walk中出现了问题
      return -1;
    }
    // start 2147487744 pte 0x87ff9008
    // PTE 写入物理内存号
    if (*pte & PTE_V) {
      // pte有效 重复映射 出错
      // 0x87ff9008
      panic("mappages: remap");
    }
    *pte = PA2PTE(physical_address) | flags | PTE_V;
    if (start == end) {
      // 当前页全部映射完成
      break;
    }
    start += PGSIZE;             // 监控映射进度
    physical_address += PGSIZE;  // 地址自增
  }
  return 0;
}

// walk模仿硬件 传入虚拟内存地址和根页表 找到相应的PTE
// 如果alloc是1 当找不到相应子页表的时候 可以创建子页表
pte_t* walk(pagetable_t pagetable, uint64 virtual_address, int alloc) {
  if (virtual_address >= MAXVA) {
    panic("walk error!");
  }
  // 两层循环 找到第三层页表中的PTE
  for (int level = 2; level > 0; level--) {
    // 按当前层级取虚拟地址的9位
    pte_t* pte = &pagetable[PX(level, virtual_address)];
    // 如果当前的PTE是有效值 则说明 pte指向的下层的页表是有效的
    // 让pagetable等于下一级页表
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      // 当前PTE指向的下一层页表是无效的
      // alloc是0 则直接退出 不进行下一层页表初始化
      // alloc是1 且分配失败 也退出
      // alloc是1 分配成功 初始化新的页表 并且让当前层级的PTE指过去
      // 并设置为有效
      if (!alloc || (pagetable = (pde_t*)kalloc()) == 0) {
        return 0;
      }
      // 清空新的页表
      memset(pagetable, 0, PGSIZE);
      // 设置当前层级的PTE指向该页表 并且设置有效
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  // 返回第三层页表的PTE的地址
  return &pagetable[PX(0, virtual_address)];
}