// 物理内存页分配
#include "types.h"
#include "proc.h"
#include "spinlock.h"
#include "memlayout.h"
#include "defs.h"
#include "riscv.h"
// 由链接脚本提供的地址
extern char end[];

void freerange(void *, void *);

// 空闲链表结构体
struct run {
  struct run *next;
};

// 空闲链表 和 控制空闲链表的锁
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 初始化物理内存页
void kinit() {
  // 初始化锁
  initlock(&kmem.lock, "kmem");
  // 抹掉堆空间
  freerange(end, (void *)PHYMEMSTOP);
}

// 抹掉空间
void freerange(void *pa_start, void *pa_end) {
  // 拿到对齐后的地址 要参与大小比较 所以用char*表示地址
  char *addr_p = (char *)PGROUNDUP((uint64)pa_start);
  for (; addr_p + PGSIZE <= (char *)pa_end; addr_p += PGSIZE) {
    kfree(addr_p);
  }
  printf("memory init done!\n");
}

// 清除一个页的物理内存 并且更新空闲链表
void kfree(void *pa) {
  struct run *r;
  // 如果没对齐 或者清除的内存是系统内存或者超出物理内存 报错
  if (((uint64)pa % PGSIZE != 0) || ((char *)pa < end) ||
      ((uint64)pa >= PHYMEMSTOP)) {
    panic("free memory error!");
  }
  // 清除一个页
  memset(pa, 1, PGSIZE);
  // 更新空闲链表
  r = (struct run *)pa;

  // 每一个页的前64位都存储的是run的next
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// 分配一个页 也就是4096字节的物理内存
void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    // 删掉空闲链表最上层的
    kmem.freelist = r->next;
  }
  release(&kmem.lock);
  if (r) {
    memset(r, 3, PGSIZE);  // 用垃圾填充
  }
  return r;
}