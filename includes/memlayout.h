// qemu virt uart配置
#define UART0 0x10000000L
#define UART0_IRQ 10

// qemu virt ram配置
#define KERNEL_BASE 0x80000000L
#define PHYMEMSTOP (KERNEL_BASE + 128 * 1024 * 1024)