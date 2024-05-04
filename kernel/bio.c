// IO缓存 缓存磁盘
#include "types.h"
#include "params.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "defs.h"

struct {
  struct spinlock lock;  // 操作缓存双向链表是原子的
  // 循环链表
  struct buf buf[NBUF];

  // 链表头
  struct buf head;
} bcache;

void binit(void) {
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // 创建双向循环链表
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  printf("block buffer linked list init:\t done!\n");
}