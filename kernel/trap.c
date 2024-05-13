#include "types.h"
#include "riscv.h"
#include "spinlock.h"
#include "params.h"
#include "proc.h"
#include "memlayout.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;  // 用于睡眠系统调用

// 这三个符号在trampoline.S中
extern char trampoline[], uservec[], userret[];

// 定义在kernelvec.S 中 会调用kerneltrap
void kernelvec();

extern int devintr();

// 初始化陷入
void trapinit(void) {
  // 初始化 定时器锁
  initlock(&tickslock, "time");
  printf("trap init:\t\t\t done!\n");
}

// 初始化陷入处理函数
void trapinithart() {
  // 写入陷入处理函数
  w_stvec((uint64)kernelvec);

  if (cpuid() == 0) {
    printf("trap vector init:\t\t done!\n");
  }
}

// 陷入处理函数 kernelvec会调用这边
// M模式的定时器处理函数会手动触发软中断 也会进来
void kerneltrap() {
  // 保存中断发生源
  int which_dev = 0;
  // sepc存的是陷入发生时的pc指针
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0) {
    // 如果发生陷入之前的模式不是S mode 报错
    panic("kerneltrap: not from supervisor mode");
  }
  if (intr_get() != 0) {
    // 说明中断处于开启状态
    // 陷入发生时硬件会关中断 如果中断仍然开启 出错
    panic("kerneltrap: interrupts enabled");
  }

  if ((which_dev = devintr()) == 0) {
    // 不是外部中断也不是软中断
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // 如果是内核进程发生中断了 myproc()为0
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) {
    yield();
  }

  w_sepc(sepc);
  w_sstatus(sstatus);
}

// 时钟中断发生时 执行这里
void clockintr() {
  // 这里wakeup唤醒的是sleep系统调用
  // 系统调用在比较一下当前时间和发生系统调用时的时间
  // 然后选择是继续睡眠还是退出睡眠
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// 从trampoline.S 进来 处理系统调用 异常和中断
// trampoline.S已经保存了用户态的寄存器
void usertrap(void) {
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0) {
    // 不是用户来的异常或中断却进入了这里
    panic("usertrap: not from user mode");
  }

  // 陷入后 由内核陷入处理函数处理陷入
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // 保存陷入发生时用户态的程序计数器
  p->trapframe->epc = r_sepc();
  
  if (r_scause() == 8) {
    // 系统调用
    // 如果当前进程是被标记为killed 就在此时处理
    // printf("进程%s 系统调用了，调用号：%d\n", p->name, p->trapframe->a7);
    if (killed(p)) {
      exit(-1);
    }

    // 系统调用需要让epc向下移动32位 否则会重复触发系统调用
    p->trapframe->epc += 4;

    intr_on();
    // 调用系统调用处理函数
    syscall();
  } else if ((which_dev = devintr()) != 0) {
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  // 如果当前进程是被标记为killed 就在此时处理
  if (killed(p)) {
    exit(-1);
  }

  // 放弃CPU 任务调度
  if (which_dev == 2) {
    yield();
  }

  // 返回用户态
  usertrapret();
}

// 返回用户态
void usertrapret(void) {
  struct proc *p = myproc();

  // 在切换陷入处理函数未完成的时候 暂时关掉中断
  // 要切换回usertrap了
  intr_off();

  // 算出trampoline.S中uservec的地址 算式后面的减法是差值
  // TRAMPOLINE是虚拟地址最高位 加上uservec的差值就是uservec的虚拟地址
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  // 设置中断处理函数为uservec
  w_stvec(trampoline_uservec);

  // 下次从内核态进入用户态的时候 也就是 uservec中
  // 会用到这些
  p->trapframe->kernel_satp = r_satp();          // 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE;  // 内核栈
  p->trapframe->kernel_trap = (uint64)usertrap;  // C语言编写的陷入处理函数
  p->trapframe->kernel_hartid = r_tp();          // CPU hart id

  // 设置SPP是0 下次sret会进入U模式
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP;  // 让SPP位为0 下次sret进入U模式
  x |= SSTATUS_SPIE;  // 用户模式开启中断
  w_sstatus(x);

  // 进入陷入的时候 在usertrap存下了进入陷入时的计数器
  // 这里恢复
  w_sepc(p->trapframe->epc);

  // 进程页表的计算 传入userret函数 在userret中写道satp中
  uint64 satp = MAKE_SATP(p->pagetable);

  // 计算userret的虚拟地址然后执行这个函数 传入的参数是satp
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// 检测中断的发生是外部中断还是软中断
// 返回2说明是定时器中断 1是其他设备 0是不认识
int devintr() {
  uint64 scause = r_scause();
  // 最高位是1 且原因是9的情况下 是S模式的外部中断 比如 PLIC产生的
  if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
    // PLIC 处理
    int irq = plic_claim();

    if (irq == UART0_IRQ) {
      uartintr();
    } else if (irq == VIRTIO0_IRQ) {
      virtio_disk_intr();
    } else if (irq) {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // 通知PLIC处理完了 要不然下一次中断不会进来
    if (irq) {
      plic_complete(irq);
    }

    return 1;

  }
  // 这里的符号是 是否等于 不是与号
  else if (scause == 0x8000000000000001L) {
    // S模式软中断
    // 能触发这里 说明是从M模式的定时器中断处理函数里面来的
    if (cpuid() == 0) {
      clockintr();
    }

    // 通知已处理完成软中断 // 清除sip寄存器的SSIP位
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}