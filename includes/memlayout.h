// qemu virt uart配置
#define UART0 0x10000000L
#define UART0_IRQ 10

// qemu virt ram配置
#define KERNEL_BASE 0x80000000L
#define PHYMEMSTOP (KERNEL_BASE + 128 * 1024 * 1024)

// qemu virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart) * 0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart) * 0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart) * 0x2000)