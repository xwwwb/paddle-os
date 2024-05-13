# 用户态的第一个程序
# 该汇编直接被嵌入在kernel中
# make user/initcode

#include "includes/syscall.h"

# 直接运行名为 init 的程序
# argv = { "init", 0 }
# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, SYS_exec
        ecall

# for(;;) exit();
exit:
        li a7, SYS_exit
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0
