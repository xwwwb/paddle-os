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
	$U/init.c \
	$U/sh.c \
	$U/helloworld.c \
	$U/ls.c \
	$U/cat.c
	

# 建立目标文件
OBJS = ${SRCS_ASM:.S=.o}
OBJS += ${SRCS_C:.c=.o}

USER_APPS = ${SRC:.c=.paddle}
USER_OBJS = ${SRC:.c=.o}

# 默认启动命令
.DEFAULT_GOAL := all

# 生成目标文件
all: kernel user disk.img

# 伪目标文件
kernel: kernel.elf

# 生成用户态APP
user: ${USER_APPS}

# 生成磁盘镜像
disk.img: mkfs/mkfs README ${USER_APPS}
	mkfs/mkfs disk.img README ${USER_APPS}

# 生成内核
kernel.elf: ${OBJS}
	${LD} ${LDFLAGS} -T $K/kernel.ld -o kernel.elf $^

# 启动虚拟机
run: all
	${QEMU} ${QFLAGS} -kernel kernel.elf

# 生成格式化程序
mkfs/mkfs: mkfs/mkfs.c
	gcc -Werror -I. -Wall -o mkfs/mkfs  mkfs/mkfs.c

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


# 清理文件
clean:
	rm -f ${OBJS} kernel.elf disk.img mkfs/mkfs user/*.o user/usys.S \
	${USER_APPS} $U/initcode $U/initcode.out

# 内核C到目标文件
${K}/%.o : ${K}/%.c
	${CC} ${CFLAGS} -c -o $@ $<

# 内核汇编到目标文件
${K}/%.o : ${K}/%.S
	${CC} ${CFLAGS} -I. -c -o $@ $<

# 用户态C到目标文件
${U}/%.o : ${U}/%.c
	${CC} ${CFLAGS} -c -o $@ $<

# 用户态汇编到目标文件
${U}/%.o : ${U}/%.S
	${CC} ${CFLAGS} -I. -c -o $@ $<

# initcode.S
$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) ${LDFLAGS} -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	@od -t xC ./user/initcode


# 用户态的静态链接库
ULIB = $U/usys.o $U/ulib.o $U/printf.o $U/umalloc.o

# 生成系统调用相关文件
$U/usys.S: $U/usys.py
	$(PYTHON) $U/usys.py > $U/usys.S

# 从.o 链接静态链接库 生成app文件
%.paddle: %.o ${ULIB}
	${LD} ${LDFLAGS} -T $U/user.ld -o $@ $^

.PRECIOUS: user/%.o
.PRECIOUS: kernel/%.o
