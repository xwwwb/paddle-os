// IO缓存 缓存磁盘
#include "includes/types.h"
#include "includes/params.h"
#include "includes/riscv.h"
#include "includes/spinlock.h"
#include "includes/sleeplock.h"
#include "includes/fs.h"
#include "includes/buf.h"
#include "includes/defs.h"

struct {
  struct spinlock lock;  // 操作缓存双向链表是原子的
  // 循环链表
  struct buf buf[NBUF];  // 利用CPU的三缓 程序的局部性

  // 链表头
  struct buf head;
} bcache;

void binit(void) {
  struct buf* b;

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

// 在特定设备查找缓存链表
// 没找到的话 分配缓存块
// 返回的buf是上锁的
static struct buf* bget(uint dev, uint blockno) {
  struct buf* b;
  acquire(&bcache.lock);

  // 查找当前块是不是已经被缓存了
  for (b = bcache.head.next; b != &bcache.head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      // 找到了
      b->refcnt++;
      release(&bcache.lock);
      // 对buffer块上锁
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 没缓存
  // 循环更新LRU块
  // 倒着查
  for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    if (b->refcnt == 0) {
      // 找到了没被用的块
      b->dev = dev;
      b->blockno = blockno;
      // 暂时没内容 所以是0
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// 返回一个带数据的buf
struct buf* bread(uint dev, uint blockno) {
  struct buf* b;

  b = bget(dev, blockno);
  if (!b->valid) {
    // 读入到buf
    virtio_disk_rw(b, 0);
    // 标记为有效位！
    b->valid = 1;
  }

  return b;
}

// 写回buf的内容到磁盘
// buf的睡眠锁要是持有状态
void bwrite(struct buf* b) {
  // 写回的时候 必须有进程拿着buf的睡眠锁
  if (!holdingsleep(&b->lock)) {
    panic("bwrite");
  }
  virtio_disk_rw(b, 1);
}

// 释放这个buffer的睡眠锁
// 放到LRU的头
void brelse(struct buf* b) {
  if (!holdingsleep(&b->lock)) {
    panic("brelse");
  }
  releasesleep(&b->lock);
  // 要读写链表了
  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 连接b的前后
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // 修改b自身的前后指向
    b->next = bcache.head.next;
    b->prev = &bcache.head;

    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  release(&bcache.lock);
}

// 增减进程对buf的引用数
void bpin(struct buf* b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void bunpin(struct buf* b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
