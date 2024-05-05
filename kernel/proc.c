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

// 第一个进程的进程号
struct proc* initproc;

extern char trampoline[];  // trampoline.S中定义了这个标签
extern void forkret(void);
static void freeproc(struct proc* p);
// 保护 操作内存描述符的线程安全
int nextpid = 1;
struct spinlock pid_lock;

// 保护wait队列的线程安全
struct spinlock wait_lock;

// 预先给所有进程分配虚拟地址
// 主要分配两个页的内核栈
// 一个有效页 一个无效页 防止栈溢出影响其他进程 地址空间是内核页表
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
    // 因为此前做了映射 内核页表中虚拟地址和实际地址是对应的
    // 这里的kstack就是物理地址
    p->kstack = KSTACK((int)(p - proc));
  }

  printf("proccess table init:\t\t done!\n");
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

// 返回当前的进程结构
struct proc* myproc(void) {
  push_off();
  struct cpu* c = mycpu();
  struct proc* p = c->proc;
  pop_off();
  return p;
}

// 分配pid号
int allocpid() {
  int pid;
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);
  return pid;
}

// 分配进程结构体
// 返回进程的时候 进程的锁还是持有状态
static struct proc* allocproc() {
  struct proc* p;
  // 遍历进程列表 找到UNUSED
  for (p = proc; p < &proc[NPROC]; p++) {
    // 修改进程结构体的内容需要上锁
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  // 分配进程号和进程状态
  p->pid = allocpid();
  p->state = USED;

  // 分配trapframe页
  if ((p->trapframe = (struct trapframe*)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  // 分配用户页表
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  // context全部置零
  memset(&p->context, 0, sizeof(p->context));
  // 设置内核栈顶指针和下一次ret的时候跳转的地址
  // forkret用于返回用户态 这里叫forkret可能是因为该allocproc由fork系统调用使用
  p->context.ra = (uint64)forkret;
  // 该地址指向的是内核页表所管理的内核栈 在此前已经初始化过了
  // 紧贴着内核地址的trampoline
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// 清除进程结构
static void freeproc(struct proc* p) {
  // 清除trapframe
  if (p->trapframe) {
    kfree((void*)p->trapframe);
  }
  p->trapframe = 0;
  // 清除用户页表
  if (p->pagetable) {
    proc_freepagetable(p->pagetable, p->sz);
  }
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// 创建用户页表
pagetable_t proc_pagetable(struct proc* p) {
  pagetable_t pagetable;
  // 创建空用户页表
  pagetable = uvmcreate();
  if (pagetable == 0) {
    return 0;
  }

  // 映射 trampoline代码到用户虚拟地址顶端
  // 但是这些是给S模式用的 U访问不到
  if (mappages(pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE,
               PTE_R | PTE_X) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }
  // 映射trapframe页 在trampoline下面
  if (mappages(pagetable, TRAPFRAME, (uint64)(p->trapframe), PGSIZE,
               PTE_R | PTE_W) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// 清除用户页表 清除物理空间
// 先清除用户页表高层的trampoline和trapframe
// 在清除低地址的页表实际映射的地址
void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// 一个简单的用户程序 执行exec("/init")
// 这是他的汇编代码
uchar initcode[] = {0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02, 0x97,
                    0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02, 0x93, 0x08,
                    0x70, 0x00, 0x73, 0x00, 0x00, 0x00, 0x93, 0x08, 0x20,
                    0x00, 0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f, 0xff,
                    0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x24, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 设置第一个用户程序
void userinit(void) {
  struct proc* p;

  p = allocproc();
  initproc = p;

  // 调用uvmfirst创建一个页的内存并且拷贝代码进去
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // 设置trapframe 这样在第一次回到用户态的时候用得到
  p->trapframe->epc = 0;  // 第一次sret去内存地址为0的地方找代码
  p->trapframe->sp = PGSIZE;  // 用户栈顶在4096处 在第一个页顶部

  safestrcpy(p->name, "initcode", sizeof(p->name));
  // p->cmd = namei("/");
  // 为调度做准备
  p->state = RUNNABLE;
  release(&p->lock);
}

// 扩大或者缩小进程的内存n个字节
int growproc(int n) {
  uint64 sz;
  struct proc* p = myproc();

  sz = p->sz;
  if (n > 0) {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if (n < 0) {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// 每一个CPU都要执行的调度
// 在运行到当前函数的swtch的时候 会保存上下文到c->context中
// 保存的时候 ra记录的地址是swtch下面的那一句 c->proc = 0;
// 所以CPU中的context存的ra就是指向c->proc = 0;
// 在swtch中有ret 执行ret的时候的ra已经是p->context中的ra了
// 而这个ra指向的是sched函数的swtch的下一句!!!
// 调度算法很简单 每次调度完都沿着当前的进度继续调度 否则pid号小的会频繁调度
// 即 旧进程通过sched的swtch进来 走到c->proc = 0; 执行循环找到新进程
// 新进程通过swtch进入到sched的swtch的下一句话 结束sched并走向usertrapret
// 进入用户空间
// 当进程是第一次创建的时候 这里的swtch不会跳入sched 会跳入forkret
// 仍然走向usertrapret
void scheduler(void) {
  struct proc* p;
  struct cpu* c = mycpu();

  c->proc = 0;
  for (;;) {
    // 必须打开中断 防止死锁
    intr_on();

    for (p = proc; p < &proc[NPROC]; p++) {
      // 这里加的锁 在新进程的yield里面释放 （如果当前p参与了调度而不是没进if）
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        // 找到进程后 切换到这个进程的内核态上下文
        p->state = RUNNING;
        c->proc = p;
        // swtch的ret会保证接下来程序进入到p->context的ra
        // 一般来说指向sched的swtch下一句话
        // 如果是第一个任务 指向的是forkret
        swtch(&c->context, &p->context);

        // 如果走到这里 说明可能是从sched的swtch来的
        // 说明当前进程运行完了 再次进入循环 调度下一个进程
        c->proc = 0;
      }
      // 这里放的锁 是旧进程在yield里面加的锁  如果当前p参与了调度而不是没进if）
      release(&p->lock);
    }
  }
}

// 切换到scheduler 上次从scheduler出来的时候 保存到CPU的上下文中
// ra指向scheduler的swtch的下一句
// 从这里进入scheduler的时候保存到进程的context的ra指向的是
// 这里的swtch的下一句
// 保存开关中断是进程的状态不是CPU的状态 这里要保存下来
void sched(void) {
  int intena;
  struct proc* p = myproc();

  if (!holding(&p->lock)) {
    panic("sched p->lock");
  }
  if (mycpu()->noff != 1) {
    panic("sched locks");
  }
  // 进入到这里的只能是RUNNABLE
  if (p->state == RUNNING) {
    panic("sched running");
  }
  // 调度时是不可能开中断的 因为陷入的时候会关掉
  if (intr_get()) {
    panic("sched interruptible");
  }
  // 记录当前CPU原始的中断开关状态
  // 新进程可能会影响这个标志
  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  // 下一次再次返回时恢复进程在此CPU的原始中断开关状态
  mycpu()->intena = intena;
}

// 放弃CPU 执行任务切换 由中断处理程序调用
void yield(void) {
  struct proc* p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// 一个被fork的子程序第一次被调度的时候 会指向这里 因为allocproc的时候
// context->pc指向这里
void forkret(void) {
  static int first = 1;

  // 释放scheduler拿到的锁
  // 如果正常调度 这个锁由yield释放
  // 但是首次调度的程序走不到yield
  release(&myproc()->lock);

  // if (first) {
  //   first = 0;
  //   fsinit(ROOTDEV);
  // }

  // 进入到用户陷入恢复程序 主要用于进入用户空间
  // 正常的系统调用或者时间片轮转 都会执行到这里
  usertrapret();
}
