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
	$K/fs.c \
	$K/virtio_disk.c \
	$K/log.c \
	$K/syscall.c \
	$K/sysproc.c \
	$K/exec.c \
	$K/sysfile.c \
	$K/pipe.c

# 用户态APP
SRC = \
	

# 建立目标文件
OBJS = ${SRCS_ASM:.S=.o}
OBJS += ${SRCS_C:.c=.o}

USER_APPS = ${SRC:.c=.paddle}
USER_OBJS = ${SRC:.c=.o}

# 默认启动命令
.DEFAULT_GOAL := all

# 生成磁盘镜像
disk.img: mkfs/mkfs README ${USER_APPS}
	mkfs/mkfs disk.img README ${USER_APPS}

# 生成目标文件
all: kernel user disk.img

# 生成内核
kernel: ${OBJS}
	@${CC} ${CFLAGS} -T $K/kernel.ld -o kernel.elf $^

# 生成用户态APP
# 遍历USER_OBJS 根据user/user.ld 连接为.paddle文件
user: ${USER_APPS}

%.paddle: %.c
	${CC} ${CFLAGS} -T $U/user.ld -o $@ $<

# 启动虚拟机
run: all
	${QEMU} ${QFLAGS} -kernel kernel.elf

# 生成格式化程序
mkfs/mkfs: mkfs/mkfs.c
	gcc -Werror -Wall -o mkfs/mkfs -I. mkfs/mkfs.c

# 生成汇编
code : ${OBJS}
	${OBJDUMP} -S ${OBJS} | less

# 调试
debug: all
	${QEMU} ${QFLAGS} -kernel kernel.elf -s -S &
	${GDB} kernel.elf -q -x ./gdbinit

# 供给vscode用
qemu-debug: all
	@echo "start qemu debug"
	${QEMU} ${QFLAGS} -kernel kernel.elf -s -S

%.o : %.c
	${CC} ${CFLAGS} -c -o $@ $<

%.o : %.S
	${CC} ${CFLAGS} -c -o $@ $<


# 清理文件
clean:
	rm -f ${OBJS} kernel.elf disk.img mkfs/mkfs ${USER_APPS} ${USER_OBJS}
