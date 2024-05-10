#include "types.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "fs.h"
#include "file.h"
#include "stat.h"
#include "defs.h"
#include "proc.h"
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

// 分配一个文件结构体
struct file* filealloc(void) {
  struct file* f;

  acquire(&ftable.lock);
  for (f = ftable.file; f < ftable.file + NFILE; f++) {
    if (f->ref == 0) {
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// 增加文件的引用数量
struct file* filedup(struct file* f) {
  acquire(&ftable.lock);
  // 不能平白无故的增加引用
  if (f->ref < 1) {
    panic("filedup");
  }
  f->ref++;
  release(&ftable.lock);
  return f;
}

// 关闭文件 减少引用数
void fileclose(struct file* f) {
  struct file ff;
  acquire(&ftable.lock);
  if (f->ref < 1) {
    panic("fileclose");
  }
  // 减少引用 如果还有引用 直接退出
  if (--f->ref > 0) {
    release(&ftable.lock);
    return;
  }
  // 这里说明引用没了
  // 备份当前file
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  // 关闭文件
  if (ff.type == FD_PIPE) {
  } else if (ff.type == FD_INODE || ff.type == FD_DEVICE) {
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// 获得文件的原数据
// addr是用户态虚拟地址 指向stat结构
int filestat(struct file* f, uint64 addr) {
  struct proc* p = myproc();
  struct stat st;

  // 如果是inode文件和设备文件
  if (f->type == FD_INODE || f->type == FD_DEVICE) {
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    // 把stat表拷贝到用户空间
    if (copyout(p->pagetable, addr, (char*)&st, sizeof(st)) < 0) {
      return -1;
    }
    return 0;
  }
  return -1;
}

// 读文件到用户空间
int fileread(struct file* f, uint64 addr, int n) {
  int r = 0;
  if (f->readable == 0) {
    return -1;
  }
  if (f->type == FD_PIPE) {
    // r =
  } else if (f->type == FD_DEVICE) {
    // 设备读
    if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read) {
      return -1;
    }
    // 调用devsw的read函数
    r = devsw[f->major].read(1, addr, n);
  } else if (f->type == FD_INODE) {
    // 文件读
    ilock(f->ip);
    if ((r = readi(f->ip, 1, addr, f->off, n)) > 0) {
      f->off += r;
    }
    iunlock(f->ip);
  } else {
    panic("fileread");
  }
  return r;
}

int filewrite(struct file* f, uint64 addr, int n) {
  int r, ret = 0;

  if (f->writable == 0) {
    return -1;
  }

  if (f->type == FD_PIPE) {
    // ret = pipewrite(f->pipe, addr, n);
  } else if (f->type == FD_DEVICE) {
    if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write) {
      return -1;
    }
    ret = devsw[f->major].write(1, addr, n);
  } else if (f->type == FD_INODE) {
    // 预留一些块
    int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
    int i = 0;
    while (i < n) {
      int n1 = n - i;
      // 计算出写入的大小是n1
      if (n1 > max) {
        n1 = max;
      }
      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0) {
        f->off += r;
      }
      iunlock(f->ip);
      end_op();

      if (r != n1) {
        // 写入大小不匹配
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}
