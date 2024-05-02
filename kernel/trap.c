#include "types.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

// 定义在kernelvec.S 中 会调用kerneltrap
void kernelvec();

int devintr();

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
  printf("trap vector init:\t\t done!\n");
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

  // // give up the CPU if this is a timer interrupt.
  // if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) {
  //   yield();
  // }

  w_sepc(sepc);
  w_sstatus(sstatus);
}

// 时钟中断发生时 执行这里
void clockintr() {
  acquire(&tickslock);
  ticks++;
  // wakeup(&ticks);
  release(&tickslock);
}

// 检测中断的发生是外部中断还是软中断
// 返回2说明是定时器中断 1是其他设备 0是不认识
int devintr() {
  uint64 scause = r_scause();
  // 最高位是1 且原因是9的情况下 是S模式的外部中断 比如 PLIC产生的
  if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
    // PLIC 处理
    // int irq = plic_claim();

    // if (irq == UART0_IRQ) {
    //   uartintr();
    // } else if (irq == VIRTIO0_IRQ) {
    //   virtio_disk_intr();
    // } else if (irq) {
    //   printf("unexpected interrupt irq=%d\n", irq);
    // }

    // // the PLIC allows each device to raise at most one
    // // interrupt at a time; tell the PLIC the device is
    // // now allowed to interrupt again.
    // if (irq) plic_complete(irq);

    return 1;
  } else if (scause & 0x8000000000000001L) {
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