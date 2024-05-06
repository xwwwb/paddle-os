#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// 初始化plic优先级
void plicinit(void) {
  // 在PLIC端设置优先级 优先级非零说明是打开状态
  // https://github.com/qemu/qemu/blob/62dbe54c24dbf77051bafe1039c31ddc8f37602d/include/hw/riscv/virt.h#L91
  *(uint32 *)(PLIC + UART0_IRQ * 4) = 1;
  *(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;

  printf("uart & virtio plic init:\t done!\n");
}

void plicinithart() {
  int hart = cpuid();
  // hart端使能两个中断
  *(uint32 *)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);
  // hart端阈值为0
  *(uint32 *)PLIC_SPRIORITY(hart) = 0;

  if (cpuid() == 0) {
    printf("uart & virtio irq init:\t\t done!\n");
  }
}

// 询问中断设备是谁
int plic_claim(void) {
  int hart = cpuid();
  int irq = *(uint32 *)PLIC_SCLAIM(hart);
  return irq;
}

// 告诉PLIC处理完成了
void plic_complete(int irq) {
  int hart = cpuid();
  *(uint32 *)PLIC_SCLAIM(hart) = irq;
}