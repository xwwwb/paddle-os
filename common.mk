# 定义工具链的信息
CROSS_COMPILE = riscv64-unknown-elf-

# 编译器参数
# -nostdlib 不连接系统标准启动文件和标准库文件
# -fno-builtin 禁止GCC内联更有效率的代码
# -march -mabi https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gcc/RISC-V-Options.html
# -g 生成调试信息
# -Wall 开启所有警告信息
CFLAGS = -nostdlib -fno-builtin -march=rv64g -g -Wall
# https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gcc/RISC-V-Options.html#index-mcmodel_003dmedany
CFLAGS += -mcmodel=medany
CFLAGS += -I./includes

QEMU = /opt/qemu-riscv64/bin/qemu-system-riscv64 # qemu 9.0.0

CPUS := 1

QFLAGS = -nographic -smp ${CPUS} -machine virt -bios none

GDB = gdb-multiarch
CC = ${CROSS_COMPILE}gcc
OBJCOPY = ${CROSS_COMPILE}objcopy
OBJDUMP = ${CROSS_COMPILE}objdump