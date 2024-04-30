# qemu的virt机器 会将kernel加载到0x80000000处
# 链接脚本处已经制定了 _entry的运行地址为0x80000000
.text
.global _entry

# 程序入口
_entry:
    # 设置C语言的函数栈
    # 遵循是ABI 每个核心的sp寄存器存入的是栈顶地址
    # stack0定义在start.c中
    la		sp, stack0

	# a0 = 4096
    li		a0, 1024 * 4
	csrr	a1, mhartid
	addi	a1, a1, 1
	# 如果是0号CPU a0 = 4096 * 1 = 4096
	mul		a0, a0, a1
	# 此时sp是stack0 + 4096 也就是第一块CPU的
	add		sp, sp, a0

	# 每一块CPU再执行完上面的代码后 都会将他自己的sp寄存器初始化完成
	
	# 进入start函数 定义在start.c中
	call	start