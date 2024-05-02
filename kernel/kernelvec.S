# S模式下的异常和中断会进入这里
# 调用陷入处理函数前 保存寄存器的值到当前栈中

.global kerneltrap # 定义在trap.c中
.global kernelvec
.align 4
# 这个函数需要4字节对齐 已让地址的尾部2位是0
# 这样是让stvec的模式是直接模式
kernelvec:
    # 开辟栈 
    addi    sp, sp, -256
    # 保存上下文
    sd ra, 0(sp)
    sd sp, 8(sp)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    call kerneltrap

    # 恢复上下文
    ld ra, 0(sp)
    ld sp, 8(sp)
    ld gp, 16(sp)
    # 不恢复tp 因为可能会更改当前CPU的tp值
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)

    addi sp, sp , 256

    sret
.global timervec
.align 4
timervec:
    # 从msratch拿到时钟配置信息
    csrrw a0, mscratch, a0
    # 因为要用到a1 a2 a3三个寄存器 将他们存入时钟配置的前三位
    sd a1, 0(a0)
    sd a2, 8(a0)
    sd a3, 16(a0)

    # 定时器重置mtimecmp寄存器
    ld a1, 24(a0) # mtimecmp内存地址
    ld a2, 32(a0) # 周期
    ld a3, 0(a1) # 因为a1存的是地址 不能对地址进行加减 要用这条语句仿存

    # mtimecmp = mtimecmp + intervel
    add a3, a3, a2
    sd a3, 0(a1) # 写回mtimecmp的内存

    # 触发软中断 让s模式处理时钟中断
    li a1, 2
    # sip寄存器的第一位是SSIP 写入1可以触发软中断也就是10
    csrw sip, a1
    
    # 恢复现场
    ld a3, 16(a0)
    ld a2, 8(a0)
    ld a1, 0(a0)
    csrrw a0, mscratch, a0

    mret
