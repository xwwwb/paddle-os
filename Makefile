include ./common.mk

K=kernel
U=user

# 汇编文件
SRCS_ASM = $K/entry.S
# C 语言文件
SRCS_C = \
	$K/main.c \
	$K/start.c \
	$K/main.c \
	$K/proc.c \
	$K/console.c \
	$K/spinlock.c



# 建立目标文件
OBJS = ${SRCS_ASM:.S=.o}
OBJS += ${SRCS_C:.c=.o}

# 默认启动命令
.DEFAULT_GOAL := all

# 生成目标文件
all: ${OBJS}
	@${CC} ${CFLAGS} -T $K/kernel.ld -o kernel.elf $^

# 启动
run: all
	${QEMU} ${QFLAGS} -kernel kernel.elf

# 调试
debug: all
	${QEMU} ${QFLAGS} -kernel kernel.elf -s -S &
	@${GDB} kernel.elf -q -x ./gdbinit

%.o : %.c
	${CC} ${CFLAGS} -c -o $@ $<

%.o : %.S
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f ${OBJS} kernel.elf