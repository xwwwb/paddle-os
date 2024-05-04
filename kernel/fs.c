#include "types.h"
#include "params.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "defs.h"

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

void iinit() {
  int i = 0;
  initlock(&itable.lock, "itable");

  // 初始化所有的INODE项
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }

  printf("inode table init:\t\t done!\n");
}