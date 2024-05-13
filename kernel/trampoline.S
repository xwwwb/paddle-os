#include "includes/riscv.h"
#include "includes/memlayout.h"

.section trampsec
.global trampoline
trampoline:
.align 4
.globl uservec
.globl userret
uservec: 
    # 当用户陷入发生的时候 在这里保存现场 
    # 并根据trapframe里面保存的参数进入内核进程
    # 临时征用a0 先把a0存起来
    csrw sscratch, a0

    # 定义在memlayout中 trapframe在虚拟地址最高位trampoline下面
    # proc_pagetable 这个函数创建进程页表的时候会负责映射
    li a0, TRAPFRAME 

    # 此时a0就是->trapframe
    # 保存现场
    sd ra, 40(a0)
    sd sp, 48(a0)
    sd gp, 56(a0)
    sd tp, 64(a0)
    sd t0, 72(a0)
    sd t1, 80(a0)
    sd t2, 88(a0)
    sd s0, 96(a0)
    sd s1, 104(a0)
    sd a1, 120(a0)
    sd a2, 128(a0)
    sd a3, 136(a0)
    sd a4, 144(a0)
    sd a5, 152(a0)
    sd a6, 160(a0)
    sd a7, 168(a0)
    sd s2, 176(a0)
    sd s3, 184(a0)
    sd s4, 192(a0)
    sd s5, 200(a0)
    sd s6, 208(a0)
    sd s7, 216(a0)
    sd s8, 224(a0)
    sd s9, 232(a0)
    sd s10, 240(a0)
    sd s11, 248(a0)
    sd t3, 256(a0)
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)

    # 保存a0
    csrr t0, sscratch
    sd t0, 112(a0)

    # 初始化内核栈 p->trapframe->kernel_sp
    ld sp, 8(a0)

    # 保持tp拿的是正确的hartid p->trapframe->kernel_hartid
    ld tp, 32(a0)
    
    # 拿usertrap函数地址 C语言编写的陷入处理函数 from p->trapframe->kernel_trap
    ld t0, 16(a0)
    
    # 拿内核页表 p->trapframe->kernel_satp
    ld t1, 0(a0)

    # 等待内存读写进行完 切换页表
    sfence.vma zero, zero

    # 换内核页表
    csrw satp, t1

    # 清除快表
    sfence.vma zero, zero

    # 去usertrap
    jr t0

userret:
    # userret(pagetable)
    # 切换页表
    sfence.vma zero, zero
    csrw satp, a0
    sfence.vma zero, zero

    li a0, TRAPFRAME

    # 恢复所有寄存器 除了a0
    ld ra, 40(a0)
    ld sp, 48(a0)
    ld gp, 56(a0)
    ld tp, 64(a0)
    ld t0, 72(a0)
    ld t1, 80(a0)
    ld t2, 88(a0)
    ld s0, 96(a0)
    ld s1, 104(a0)
    ld a1, 120(a0)
    ld a2, 128(a0)
    ld a3, 136(a0)
    ld a4, 144(a0)
    ld a5, 152(a0)
    ld a6, 160(a0)
    ld a7, 168(a0)
    ld s2, 176(a0)
    ld s3, 184(a0)
    ld s4, 192(a0)
    ld s5, 200(a0)
    ld s6, 208(a0)
    ld s7, 216(a0)
    ld s8, 224(a0)
    ld s9, 232(a0)
    ld s10, 240(a0)
    ld s11, 248(a0)
    ld t3, 256(a0)
    ld t4, 264(a0)
    ld t5, 272(a0)
    ld t6, 280(a0)

    #  恢复a0
    ld a0, 112(a0)

    // 恢复到用户态 usertrapret会设置sstatus和sepc
    sret
