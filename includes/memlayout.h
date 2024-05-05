// qemu virt uart配置
#define UART0 0x10000000L
#define UART0_IRQ 10

// qemu virt ram配置
#define KERNEL_BASE 0x80000000L
#define PHYMEMSTOP (KERNEL_BASE + 128 * 1024 * 1024)

// qemu 虚拟IO
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart) * 0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart) * 0x100)
// 这里其实是阈值
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart) * 0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart) * 0x2000)

// CLINT的配置 核心本地中断控制器
#define CLINT_BASE 0x2000000L
/** 这两个寄存器写在了riscv特权指令标准 mtime会持续自增
 *  大于timecmp的时候发生timer 中断 timer的使能就是mie的mtie位
 *  这里的具体地址映射找不到依据
 *  https://github.com/qemu/qemu/blob/62dbe54c24dbf77051bafe1039c31ddc8f37602d/hw/riscv/virt.c#L1487
 *  只能理解为 virt采用了sifive的然后通过宏的大小计算出来的mtimecmp和mtime地址
 *  前0x4000是给软中断用的吧
 */
#define CLINT_MTIMECMP(hartid) (CLINT_BASE + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT_BASE + 0xBFF8)  // 启动后按照一定频率增加

// 让跳板代码在虚拟地址的最高层 无论是用户页表还是内核页表都是
#define TRAMPOLINE (MAXVA - PGSIZE)

// 在用户页表中 trapframe在trampoline下面
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// 根据进程的索引映射出进程的内核栈
// 每一个内核栈分两页 一页有效 一页是无效的guard page 当栈溢出时 不会覆盖其他栈
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PGSIZE)