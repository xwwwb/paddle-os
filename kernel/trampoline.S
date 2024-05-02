#include "riscv.h"
#include "memlayout.h"

.section trampsec
.global trampoline
trampoline:
.align 4
# 预留一个字节
.word 0