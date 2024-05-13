// 磁盘事务
#include "includes/types.h"
#include "includes/params.h"
#include "includes/spinlock.h"
#include "includes/fs.h"
#include "includes/riscv.h"
#include "includes/defs.h"
#include "includes/sleeplock.h"
#include "includes/buf.h"
// logheader放在log区的第一个块里面
struct logheader {
  int n;  // n记录了当前有几个块在log区
  int block[LOGSIZE];
};

// 内存中维护的log变量
struct log {
  struct spinlock lock;  // 操作这个结构的锁
  int start;             // log区的开始block
  int size;              // log区block数
  int outstanding;       // 多少系统调用在执行
  int committing;        // 标识是不是正在提交
  int dev;               // 设备号
  struct logheader lh;   // 磁盘中的logheader 内存中存一份
};

struct log log;

static void recover_from_log(void);
static void commit();

// 初始化日志系统 被fsinit调用 第一次创建进程的时候执行
void initlog(int dev, struct superblock *sb) {
  // logheader一定要小于一个块
  if (sizeof(struct logheader) >= BSIZE) {
    panic("initlog: too big logheader");
  }

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  // 如果有没写回的块 在此处恢复
  recover_from_log();
}

// 提交事务 将log区内容拷贝到原本的位置
static void install_trans(int recovering) {
  int tail;
  for (tail = 0; tail < log.lh.n; tail++) {
    // 根据logheader中的n提交
    // 利用buf把log区的block加载进来
    struct buf *lbuf = bread(log.dev, log.start + tail + 1);
    // 利用buf载入实际需要写入的block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);
    // 写入buf
    memmove(dbuf->data, lbuf->data, BSIZE);
    // buf写回
    bwrite(dbuf);
    if (recovering == 0) {
      bunpin(dbuf);
    }
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 读磁盘的log header到内存
static void read_head(void) {
  // 读日志区的第一个block
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)(buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// 写磁盘header
static void write_head(void) {
  struct buf *buf = bread(log.dev, log.start);

  struct logheader *hb = (struct logheader *)(buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  // 为数不多的bwrite
  bwrite(buf);
  brelse(buf);
}

// 从故障恢复
static void recover_from_log() {
  // 读入log header到内存
  read_head();
  install_trans(1);  // 如果有未处理提交 log区数据拷贝到磁盘
  log.lh.n = 0;
  write_head();  // 清理日志
}

// 开始文件系统的操作
void begin_op() {
  acquire(&log.lock);

  while (1) {
    if (log.committing) {
      // 有别的进程在提交
      sleep(&log, &log.lock);
    } else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE) {
      // log区没空间了
      sleep(&log, &log.lock);
    } else {
      // outstanding加一
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// 结束文件系统的操作
void end_op(void) {
  int do_commit = 0;
  acquire(&log.lock);
  log.outstanding -= 1;
  if (log.committing) {
    panic("log.committing");
  }
  if (log.outstanding == 0) {
    do_commit = 1;
    log.committing = 1;
  } else {
    // 如果log.outstanding不为0 说明可能有begin_op在等
    // 减少了outstanding后 可以唤醒begin_op
    wakeup(&log);
  }
  release(&log.lock);

  if (do_commit) {
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// 先把cache中的数据写到log
// 这里打开的from 应该在buf中有缓存
// 且其中的buf已经被修改了
static void write_log(void) {
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start + tail + 1);
    struct buf *from = bread(log.dev, log.lh.block[tail]);
    memmove(to->data, from->data, BSIZE);
    bwrite(to);
    brelse(from);
    brelse(to);
  }
}

// 执行提交
static void commit() {
  if (log.lh.n > 0) {
    write_log();       // 写回cache到日志层数据块
    write_head();      // 写磁盘的logheader
    install_trans(0);  // 写回数据到block
    log.lh.n = 0;      // 清空log
    write_head();      // 把清空的log写入header
  }
}

// 系统调用使用这个函数写入
// bp = bread(...)
// 修改bp->data
// log_write(bp)
// brelse(bp)
// 其实只提了一个commit 修改了log header
// buf块的引用次数也自增了
// 实际上修改的是buf
void log_write(struct buf *b) {
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1) {
    panic("too big a transaction");
  }
  if (log.outstanding < 1) {
    panic("log_write outside of trans");
  }

  // 修改log header的block区
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno) {
      break;
    }
  }
  log.lh.block[i] = b->blockno;
  // 自增log header 的n
  if (i == log.lh.n) {
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}
