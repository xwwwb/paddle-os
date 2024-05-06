include ./common.mk

K=kernel
U=user

# 汇编文件
SRCS_ASM = \
	$K/entry.S \
	$K/trampoline.S \
	$K/kernelvec.S \
	$K/swtch.S
# C 语言文件
SRCS_C = \
	$K/main.c \
	$K/start.c \
	$K/main.c \
	$K/proc.c \
	$K/console.c \
	$K/spinlock.c \
	$K/uart.c \
	$K/file.c \
	$K/printf.c \
	$K/kalloc.c \
	$K/string.c \
	$K/vm.c \
	$K/trap.c \
 	$K/plic.c \
	$K/bio.c \
	$K/sleeplock.c \
	$K/fs.c



# 建立目标文件
OBJS = ${SRCS_ASM:.S=.o}
OBJS += ${SRCS_C:.c=.o}

# 默认启动命令
.DEFAULT_GOAL := all

# 生成目标文件
all: ${OBJS}
	@${CC} ${CFLAGS} -T $K/kernel.ld -o kernel.elf $^

# 生成汇编
code : ${OBJS}
	@${OBJDUMP} -S ${OBJS} | less

# 启动
run: all
	@${QEMU} ${QFLAGS} -kernel kernel.elf

# 调试
debug: all
	${QEMU} ${QFLAGS} -kernel kernel.elf -s -S &
	@${GDB} kernel.elf -q -x ./gdbinit

qemu-debug: all
	@echo "start qemu debug"
	${QEMU} ${QFLAGS} -kernel kernel.elf -s -S

%.o : %.c
	${CC} ${CFLAGS} -c -o $@ $<

%.o : %.S
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f ${OBJS} kernel.elf