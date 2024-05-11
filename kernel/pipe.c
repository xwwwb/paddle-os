#include "types.h"
#include "riscv.h"
#include "params.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "defs.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  // 读写头不限制于512 会持续增加 但是取值的时候会取模
  uint nread;     // 读头
  uint nwrite;    // 写头
  int readopen;   // 多少进程在读
  int writeopen;  // 多少进程在写
};

// 分配一个管道
// 传入两个file*指针
int pipealloc(struct file **f0, struct file **f1) {
  struct pipe *pi;

  pi = 0;
  *f0 = *f1 = 0;
  // 分配两个file结构体
  if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0) {
    goto bad;
  }
  // 分配一个管道
  if ((pi = (struct pipe *)kalloc()) == 0) {
    goto bad;
  }
  // 管道的初始化
  // 进程数都是1 读写都是0
  pi->readopen = 1;
  pi->writeopen = 1;
  pi->nwrite = 0;
  pi->nread = 0;
  // 初始化管道的锁
  initlock(&pi->lock, "pipe");
  // 初始化file结构体为管道
  // 一个可读 一个可写
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = pi;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = pi;
  return 0;

bad:
  if (pi) {
    kfree((char *)pi);
  }
  if (*f0) {
    fileclose(*f0);
  }
  if (*f1) {
    fileclose(*f1);
  }
  return -1;
}

// 关闭管道
void pipeclose(struct pipe *pi, int writable) {
  acquire(&pi->lock);
  // 如果当前是可写的管道
  if (writable) {
    // 不允许写
    pi->writeopen = 0;
    // 唤醒所有等待读的进程
    // 因为有可能有进程在等待读 如果不唤醒会一直等待
    wakeup(&pi->nread);
  } else {
    // 不允许读
    pi->readopen = 0;
    // 唤醒所有等待写的进程
    // 因为有可能有进程在等待写 如果不唤醒会一直等待
    wakeup(&pi->nwrite);
  }
  // 删除管道的物理页
  if (pi->readopen == 0 && pi->writeopen == 0) {
    release(&pi->lock);
    kfree((char *)pi);
  } else
    release(&pi->lock);
}

// 向管道里面写入数据
// 管道结构 用户地址 写入的字节数
int pipewrite(struct pipe *pi, uint64 addr, int n) {
  int i = 0;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  while (i < n) {
    // 如果管道不可写或者进程被杀死 直接退出 因为管道关闭的时候 有唤醒进程的操作
    // 唤醒后的进程可以直接退出
    if (pi->readopen == 0 || killed(pr)) {
      release(&pi->lock);
      return -1;
    }
    // 如果管道满了
    if (pi->nwrite == pi->nread + PIPESIZE) {
      // 唤醒要读的进程
      wakeup(&pi->nread);
      // 等待读进程唤醒本次写进程
      sleep(&pi->nwrite, &pi->lock);
    } else {
      char ch;
      // 逐字节拷贝用户空间的数据到内核空间
      if (copyin(pr->pagetable, &ch, addr + i, 1) == -1) {
        break;
      }
      // 写入管道 并且写头+1
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  wakeup(&pi->nread);  // 写完了唤醒读进程
  release(&pi->lock);

  return i;
}

// 管道读
int piperead(struct pipe *pi, uint64 addr, int n) {
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  // 如果管道没有数据可读 并且管道还是可写的 睡眠等待写进程写入数据
  while (pi->nread == pi->nwrite && pi->writeopen) {
    // 睡醒之后要判断进程是否被杀死 因为可能会被close
    if (killed(pr)) {
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock);
  }
  // 逐字节拷贝数据到用户空间
  for (i = 0; i < n; i++) {
    if (pi->nread == pi->nwrite) {
      break;
    }
    ch = pi->data[pi->nread++ % PIPESIZE];
    if (copyout(pr->pagetable, addr + i, &ch, 1) == -1) {
      break;
    }
  }
  wakeup(&pi->nwrite);  // 读完了唤醒写进程
  release(&pi->lock);
  return i;
}
