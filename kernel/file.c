#include "types.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "fs.h"
#include "file.h"
#include "defs.h"

#include "params.h"

struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

struct devsw devsw[NDEV];

void fileinit(void) {
  // 文件表
  initlock(&ftable.lock, "ftable");
  printf("file table init:\t\t done!\n");
}