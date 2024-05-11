// 文件系统相关系统调用
#include "types.h"
#include "params.h"
#include "syscall.h"
#include "riscv.h"
#include "defs.h"
uint64 sys_exec(void) {
  char path[MAXPATH], *argv[MAXARG];

  int i;
  uint64 uargv, uarg;

  // 取参数中的指针
  argaddr(1, &uargv);
  // 取参数中的路径
  if (argstr(0, path, MAXPATH) < 0) {
    return -1;
  }
  // 先清空argv
  memset(argv, 0, sizeof(argv));
  // 塞入argv
  for (i = 0;; i++) {
    if (i >= NELEM(argv)) {
      goto bad;
    }
    // 从uargv中取当前字符串地址取到uarg里面
    if (fetchaddr(uargv + sizeof(uint64) * i, (uint64*)&uarg) < 0) {
      goto bad;
    }
    // 地址为0 说明取完了
    if (uarg == 0) {
      argv[i] = 0;
      break;
    }
    // 开辟一个物理空间存字符串
    argv[i] = kalloc();
    if (argv[i] == 0) {
      goto bad;
    }
    // 把uarg指向的字符串复制到物理页中
    if (fetchstr(uarg, argv[i], PGSIZE) < 0) {
      goto bad;
    }
  }
  // argv是一个char**
  int ret = exec(path, argv);
  // exec已经拷贝参数到用户栈了
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++) {
    kfree(argv[i]);
  }
  // 这里的ret写入trampframe的a0了 ret是argc 这样exec实际上执行的
  // main(argc, argv) 参数都齐了
  return ret;
bad:
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++) {
    kfree(argv[i]);
  }
  return -1;
}