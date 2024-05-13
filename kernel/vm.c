#include "includes/types.h"
#include "includes/riscv.h"
#include "includes/memlayout.h"
#include "includes/defs.h"

pagetable_t kernel_pagetable;

// 为什么类型是char[]
extern char etext[];

// 跳板代码的地址
extern char trampoline[];

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

  // trampoline在虚拟地址的最高层
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // 给每一个进程的内核栈分配虚拟内存 虚拟内存在trampoline下面
  proc_mapstacks(kpgtbl);

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
  // 等待在此之前的内存读写操作完成 因为马上要改变仿存方式了
  // 特权指令集P65
  sfence_vma();
  // 写satp寄存器
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
  /**
   * 但如果修改了 satp
   * 寄存器，说明内核切换到了一个与先前映射方式完全不同的页表。
   * 此时快表里面存储的映射已经失效了，这种情况下内核要在修改 satp
   * 的指令后面马上使用 sfence.vma 指令刷新清空整个 TLB。
   * 同样，我们手动修改一个页表项之后，也修改了映射，但 TLB 并不会自动刷新清空，
   * 我们也需要使用 sfence.vma 指令刷新整个 TLB。
   * 注：可以在 sfence.vma 指令后面加上一个虚拟地址，
   * 这样 sfence.vma 只会刷新TLB中关于这个虚拟地址的单个映射项。
   * 特权指令集P65
   */
  if (cpuid() == 0) {
    printf("memory paging init: \t\t done!\n");
  }
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

// 通过虚拟地址找到物理地址
// 只能用于寻找用户页
uint64 walkaddr(pagetable_t pagetable, uint64 virtual_address) {
  pte_t* pte;
  uint64 physical_address;
  // 虚拟地址不能大于最大的虚拟地址
  if (virtual_address >= MAXVA) {
    return 0;
  }

  // 寻找pte 且不允许重新分配页表
  pte = walk(pagetable, virtual_address, 0);
  if (pte == 0) {
    return 0;
  }
  if ((*pte & PTE_V) == 0) {
    // pte无效
    return 0;
  }
  if ((*pte & PTE_U) == 0) {
    return 0;
  }
  physical_address = PTE2PA(*pte);
  return physical_address;
}

// 创建空页表 用户态 没有空间了返回0
pagetable_t uvmcreate() {
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0) {
    // 物理空间没了
    return 0;
  }
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 加载initcode在页表的地址0处
// 整个系统的第一个进程
// 代码长度要小于一页
void uvmfirst(pagetable_t pagetable, uchar* src, uint sz) {
  char* mem;

  if (sz >= PGSIZE) {
    panic("uvmfirst error: more than a page");
  }
  mem = kalloc();
  // 清空页
  memset(mem, 0, PGSIZE);
  // 映射虚拟地址的0地址到物理地址
  mappages(pagetable, 0, (uint64)mem, PGSIZE, PTE_W | PTE_R | PTE_X | PTE_U);
  // 内存拷贝
  memmove(mem, src, sz);
}

// 进程申请更多内存的时候 oldsz老内存大小 newsz新内存大小 这里不需要对齐到页大小
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) {
  char* mem;
  uint64 address;

  if (newsz < oldsz) {
    // 不需要分配
    return oldsz;
  }
  oldsz = PGROUNDUP(oldsz);
  for (address = oldsz; address < newsz; address += PGSIZE) {
    mem = kalloc();
    if (mem == 0) {
      // 释放空间
      uvmdealloc(pagetable, address, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, address, (uint64)mem, PGSIZE,
                 PTE_R | PTE_U | xperm) != 0) {
      kfree(mem);
      uvmdealloc(pagetable, address, oldsz);
      return 0;
    }
  }
  return newsz;
}

// 通过虚拟地址和页号移除内存映射关系 虚拟地址要是页对齐的
// 映射也必须是已经存在的 可选是否清除物理内存
void uvmunmap(pagetable_t pagetable, uint64 virtual_address, uint64 npages,
              int do_free) {
  uint64 a;
  pte_t* pte;
  if ((virtual_address % PGSIZE) != 0) {
    // 没对齐
    panic("uvmunmap: not aligned");
  }
  // 遍历所有已映射地址
  for (a = virtual_address; a < virtual_address + npages * PGSIZE;
       a += PGSIZE) {
    if ((pte = walk(pagetable, a, 0)) == 0) {
      // 没找到pte
      panic("uvmunmap: walk");
    }
    if ((*pte & PTE_V) == 0) {
      // 无效的pte
      panic("uvmunmap: not mapped");
    }
    if (PTE_FLAGS(*pte) == PTE_V) {
      // 当前PTE不是三级页表的PTE 这个错误很奇怪
      panic("uvmunmap: not a leaf");
    }
    if (do_free) {
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    // PTE清空 解除了映射关系
    *pte = 0;
  }
}

// 清除所有的页表页 不是每个PTE是装PTE的页
// 和uvmunmap成对出现
void freewalk(pagetable_t pagetable) {
  // 每个页表页都装了512个PTE 因为一个PTE64位 4k/64 = 512
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    // 如果当前PTE不是最低级页表的PTE 说明当前PTE指向下一个页表 要递归清除
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      // PTE有效 且 flag位不可读不可写不可执行 说明指向更低级页表
      // walk函数有 非最低级页表的PTE有效位是PTE_V
      // 且最低页表有效位肯定有其他的标志
      uint64 child = PTE2PA(pte);
      // 递归调用
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      // 如果走到这里 说明当前PTE已经是最低级页表的PTE了 且PTE不为空
      // 说明映射还在 报错
      panic("freewalk error: leaf");
    }
  }
  // 最后清除当前页表页的内存 递归的时候会清除内侧的页表页内存
  kfree((void*)pagetable);
}

// 清除用户态的内存页
// 清除页表页
void uvmfree(pagetable_t pagetable, uint64 sz) {
  if (sz > 0) {
    // 接触关联和清除页表
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  }
  freewalk(pagetable);
}

// 减少空间
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
  if (newsz >= oldsz) return oldsz;

  // 说明新的空间小于老的空间
  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
    // 计算出差的页面数
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    // 因为用户空间地址从0开始 所以newsz的地址就是新地址的尾部
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// 标记一个PTE是用户态不可访问的
// exec使用这个函数标记栈守护页
void uvmclear(pagetable_t pagetable, uint64 va) {
  pte_t* pte;
  pte = walk(pagetable, va, 0);
  if (pte == 0) {
    panic("uvmclear");
  }
  *pte &= ~PTE_U;
}

// 提供父进程页表 拷贝内存到子进程页表
// 拷贝页表和物理内存
// 返回0成功返回-1失败 并且失败时回收空间 sz在调用的时候会传入进程占用内存大小
// 即整个内存空间
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  pte_t* pte;
  uint64 physical_address, i;
  uint flags;
  char* mem;

  // 从虚拟地址0开始遍历
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walk(old, i, 0)) == 0) {
      panic("uvmcopy error: old pte should exist");
    }
    if ((*pte & PTE_V) == 0) {
      panic("uvmcopy: page not present");
    }
    // 拿到旧的物理地址和标志位
    physical_address = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0) {
      goto err;
    }
    // 老的内存全部拷贝到新的内存
    memmove(mem, (char*)physical_address, PGSIZE);
    // 建立新的虚拟地址(和老的一样用 i) 和新的物理地址之间的联系
    // 建立在新页表上
    if (mappages(new, i, (uint64)mem, PGSIZE, flags) != 0) {
      kfree(mem);
      goto err;
    };
  }
  return 0;

err:
  // 把当前页表的从0到出错的虚拟地址全部取消映射
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// 从用户态拷贝到内核态 该代码运行在内核态
// 为什么用户态内存地址用uint64表示 这是因为用户态的内存寻址本应该走MMU
// 但是当前在内核态所以要用walkaddress手动寻址
int copyin(pagetable_t pagetable, char* dst, uint64 srcva, uint64 len) {
  uint64 n, va0, pa0;
  while (len > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
      return -1;
    }
    // https://blog.csdn.net/zzy980511/article/details/129986954
    // 复制第一个块的时候 地址srcva不一定是对齐的
    // PGSIZE - (srcva - va0)是 需要拷贝的大小
    // n是实际拷贝的长度
    n = PGSIZE - (srcva - va0);

    // 防止多拷贝
    if (n > len) {
      n = len;
    }
    memmove(dst, (void*)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    // 当拷贝完第一块后 这里的srcva一定是页对齐的
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// 从用户态拷贝字符串到内核
// 拷贝到'\0'或者max为止
int copyinstr(pagetable_t pagetable, char* dst, uint64 srcva, uint64 max) {
  uint64 n, va0, pa0;
  // 标志位
  int got_null = 0;

  while (got_null == 0 && max > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
      return -1;
    }
    // 和copyin一个套路
    // n是要拷贝的长度 当srcva页对齐的时候 就是一个页
    n = PGSIZE - (srcva - va0);

    if (n > max) {
      n = max;
    }
    char* p = (char*)(pa0 + (srcva - va0));

    // 字符拷贝
    while (n > 0) {
      if (*p == '\0') {
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }
    srcva = va0 + PGSIZE;
  }
  if (got_null) {
    return 0;
  } else {
    // 可能是超过了max还没复制完
    return -1;
  }
}
// 从内核态拷贝到用户态 和copyin一模一样
int copyout(pagetable_t pagetable, uint64 dstva, char* src, uint64 len) {
  uint64 n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
      return -1;
    }
    n = PGSIZE - (dstva - va0);
    if (n > len) {
      n = len;
    }
    // 这里换了目标地址和原地址
    memmove((void*)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
