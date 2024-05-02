// CPU相关的代码
#include "types.h"
#include "riscv.h"
#include "params.h"
#include "memlayout.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"

// CPU结构体数量
struct cpu cpus[NCPU];

struct proc proc[NPROC];

// 保护 操作内存描述符的线程安全
int nextpid = 1;
struct spinlock pid_lock;

// 保护wait队列的线程安全
struct spinlock wait_lock;

void proc_mapstacks(pagetable_t kpgtbl) {
  struct proc* p;

  for (p = proc; p < &proc[NPROC]; p++) {
    // 创建一个虚拟地址
    char* physical_address = kalloc();
    if (physical_address == 0) {
      panic("kalloc error!");
    }
    // 进程的内核栈是虚拟地址 trampoline下面
    // 每次分配两个页 一个有效页 一个保护页
    // 为了防止内存溢出的时候不覆盖其它栈内存
    uint64 virtual_address = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, virtual_address, (uint64)physical_address, PGSIZE,
           PTE_R | PTE_W);
  }
}

// 初始化进程描述表初始化
void procinit() {
  // 创建进程描述符指针
  struct proc* p;
  // 初始化两把锁
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");

  // 遍历进程描述符
  for (p = proc; p < &proc[NPROC]; p++) {
    // 初始化进程描述符内部的锁
    initlock(&p->lock, "process");
    p->state = UNUSED;
    // 在初始化内核页表的时候 已经初始化了kstack
    p->kstack = KSTACK((int)(p - proc));
  }

  printf("pagetable init:\t\t\t done!\n");
}

// start.c 将hartid存入了tp
int cpuid() {
  int id = r_tp();
  return id;
}

// 返回当前的CPU结构体
struct cpu* mycpu() {
  int id = cpuid();
  struct cpu* c = &cpus[id];
  return c;
}
