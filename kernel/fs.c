#include "types.h"
#include "params.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "buf.h"
#include "stat.h"
#include "defs.h"
#define min(a, b) ((a) < (b) ? (a) : (b))

// 操作系统只挂载了一个磁盘 这里也只设置一个超级块
struct superblock sb;

// 读取超级块
// readsb由fsinit调用 fsinit必须在进程初始化后使用
// 因为读取buf用到了睡眠锁
static void readsb(int dev, struct superblock *sb) {
  struct buf *b;
  b = bread(dev, 1);
  memmove(sb, b->data, sizeof(*sb));
  brelse(b);
}

// 文件系统初始化
// 需要在第一个进程启动的时候 初始化 因为用到了睡眠锁
void fsinit(int dev) {
  readsb(dev, &sb);
  if (sb.magic != FSMAGIC) {
    panic("invalid file system");
  }
  initlog(dev, &sb);
}

// 清空一个块 整个块写0
static void bzero(int dev, int bno) {
  struct buf *bp;

  // 打开buf后 需要brelse
  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  // 写块用log写
  log_write(bp);
  brelse(bp);
}

// 分配一个block
// 主要就是修改位图
static uint balloc(uint dev) {
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  // 说明内部循环 一次会处理一个位图块 一个位图块可以表示的块大小 1024*8
  // 但是这里size小于BPB 说明一个块存的位图就够表示整个磁盘了
  for (b = 0; b < sb.size; b += BPB) {
    // bp是拿到当前位图块
    bp = bread(dev, BBLOCK(b, sb));
    // 遍历当前位图块
    // b + bi是当前位表示的块号
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
      // 当前位的掩码
      m = 1 << (bi / 8);
      if ((bp->data[bi / 8] & m) == 0) {
        // 当前位是空的
        bp->data[bi / 8] |= m;  // 标记已使用
        // 修改位图块的block
        log_write(bp);
        brelse(bp);
        // 清空新块
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

// 清除块
// 修改位图
static void bfree(int dev, uint b) {
  struct buf *bp;
  int bi, m;

  // 读位图块
  bp = bread(dev, BBLOCK(b, sb));
  // 拿到具体的字节
  bi = b % BPB;
  // 掩码
  m = 1 << (bi % 8);
  if ((bp->data[bi / 8] & m) == 0) {
    panic("freeing free block");
  }
  // 清除位图
  bp->data[bi / 8] &= ~m;
  log_write(bp);
  brelse(bp);
}

/**
 * ip = iget(dev, inum)先获得该inode的内存镜像，方便对Inode操作
 * ilock(ip)申请该Inode的睡眠锁
 * examine and modify ip->xxx ...可能的iupdate，writei，readi
 * 调用对Inode进行操作，数据读写等
 * iunlock(ip)释放该Inode的睡眠锁
 * iput(ip)若后续不再操作该文件，将Inode放回队列
 */

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

// 初始化内存中的inode表
void iinit() {
  int i = 0;
  initlock(&itable.lock, "itable");

  // 初始化所有的INODE项
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }

  printf("inode table init:\t\t done!\n");
}

static struct inode *iget(uint dev, uint inum);

// 分配一个inode
struct inode *ialloc(uint dev, short type) {
  int inum;
  struct buf *bp;
  struct dinode *dip;

  // 遍历inodes区
  for (inum = 1; inum < sb.ninodes; inum++) {
    // 拿到inode存放在哪个块上
    bp = bread(dev, IBLOCK(inum, sb));
    // 拿到磁盘上的inode结构
    dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type == 0) {
      // 空的inode
      // inode结构体写0
      memset(dip, 0, sizeof(*dip));
      // 设置类型
      dip->type = type;
      // 写到磁盘 标记当前inode被分配了
      log_write(bp);
      brelse(bp);
      // 返回inode结构体
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}

// 内存中的inode信息被修改后 同步到磁盘
// 调用者必须有ip->lock锁
void iupdate(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;
  // 读内存中的inode块
  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode *)bp->data + ip->inum % IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  // 拷贝地址映射
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// 从内存inode table中遍历
// 根据设备号和inode号寻找缓存 没有则从table中找到一个可用项并设置其信息
// iget不从硬盘读取该inode数据 不申请该项睡眠锁。
static struct inode *iget(uint dev, uint inum) {
  struct inode *ip, *empty;

  acquire(&itable.lock);
  empty = 0;

  // 查看inode在不在内存的inode表中
  for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0) {
      // 从inodes表中找一个空槽
      empty = ip;
    }
  }

  if (empty == 0) {
    panic("iget: no inodes");
  }

  // 设置空闲inode 并设置valid为0 表示未被读进
  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
}

// 增加引用数量
struct inode *idup(struct inode *ip) {
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// 锁定当前的inode 如果有必要从磁盘中读进内存
void ilock(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1) {
    panic("ilock");
  }

  // 这里锁定
  acquiresleep(&ip->lock);

  // 如果需要的话 读入
  if (ip->valid == 0) {
    // 找到磁盘的inode结构
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    // 拷贝到内存inode
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    // 设置inode已经读入
    ip->valid = 1;
    if (ip->type == 0) {
      panic("ilock: no type");
    }
  }
}

// 释放睡眠锁
void iunlock(struct inode *ip) {
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1) {
    panic("iunlock");
  }
  releasesleep(&ip->lock);
}

// 减少内存中inode的引用次数
// 如果inode没有引用了 在inode表中释放
// 如果inode也没有硬链接了 释放空间
// 必须运行在事务中 因为有写磁盘操作
void iput(struct inode *ip) {
  acquire(&itable.lock);
  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    // 没有硬链接且没有引用了
    // ref == 1 当前函数目的就是为了减少引用
    // ref == 1 说明进程已经放弃了最后一个引用
    // 这里拿锁也不会死锁了
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }
  ip->ref--;

  release(&itable.lock);
}

// 执行unlock和放弃inode
void iunlockput(struct inode *ip) {
  iunlock(ip);
  iput(ip);
}

// 读取inode中记录的映射的地址
// 传入的bn是block number
static uint bmap(struct inode *ip, uint bn) {
  uint addr, *a;
  struct buf *bp;

  // 如果block number是直接映射
  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0) {
      // 如果这个块是空的 分配一下
      addr = balloc(ip->dev);
      if (addr == 0) {
        return 0;
      }
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  // 如果走到这里 说明是间接映射
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
    // 寻找间接映射块 如果没间接映射块 分配一个
    if ((addr = ip->addrs[NDIRECT]) == 0) {
      addr = balloc(ip->dev);
      if (addr == 0) return 0;
      ip->addrs[NDIRECT] = addr;
    }
    // 读间接映射块
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    // a应当是一个uint[]
    // 如果为0的话 分配数据块
    // 同时因为修改了间接映射快 重写简介映射块
    if ((addr = a[bn]) == 0) {
      addr = balloc(ip->dev);
      if (addr) {
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }
  panic("bmap: out of range");
}

// 清除inode 和内容
// 调用者需要有inode的锁
void itrunc(struct inode *ip) {
  int i, j;
  struct buf *bp;
  uint *a;

  // 清除直接映射的内容
  for (i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      // 清除指向的数据块的内容
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // 间接映射
  if (ip->addrs[NDIRECT]) {
    // 读入间接映射块
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    for (j = 0; j < NINDIRECT; j++) {
      if (a[j]) {
        // 清理间接映射的数据块
        bfree(ip->dev, a[j]);
      }
    }
    brelse(bp);
    // 释放间接映射块
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  // 更新硬盘中的inode
  iupdate(ip);
}

// 设置inode的stat 调用者需要有inode的锁
void stati(struct inode *ip, struct stat *st) {
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// 从inode读文件
// 调用者必须由inode的锁 如果user_dst为1 说明dst是user vm
// 其他情况dst是kernel vm
// inode dst标志位 dst 偏移 大小
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  // 偏移量大于文件大小 大小小于0
  if (off > ip->size || off + n < off) {
    return 0;
  }
  // 偏移量加大小大于文件长度
  if (off + n > ip->size) {
    // 从偏移量开始读到结尾
    n = ip->size - off;
  }
  // tot 是读取的字节 n是读取的总大小 m是循环执行时读取的大小
  // 这里的m需要在循环里计算 因为有off和n的缘故 一般不会是页大小
  for (tot = 0; tot < n; tot += m, off += m, dst += m) {
    // 找到off开始地址的数据块
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0) {
      break;
    }
    // 读数据块到buf
    bp = bread(ip->dev, addr);
    // n - tot是还剩要读取的大小
    // 第二个参数是 从off开始到页结尾的大小
    // 选出最小的作为当前的读入大小
    m = min(n - tot, BSIZE - off % BSIZE);
    if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  // 返回读入的字节数
  return tot;
}

// 写数据到inode
// 调用者必须由inode的锁 如果user_dst为1 说明dst是user vm
// 返回写入数
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  // n 小于零 或者偏移大于文件大小
  if (off > ip->size || off + n < off) {
    return -1;
  }
  // 超过单文件最大
  if (off + n > MAXFILE * BSIZE) {
    return -1;
  }
  // 和readi差不多
  for (tot = 0; tot < n; tot += m, off += m, src += m) {
    // 找到off开始地址的数据块
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0) {
      break;
    }
    // 读入数据块到buf
    bp = bread(ip->dev, addr);

    // 计算写入大小 m是当次循环的写入大小
    m = min(n - tot, BSIZE - off % BSIZE);
    // 拷贝到buf
    if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  // 更新文件大小
  // off就是偏移地址+写入大小
  // 如果超过了源文件大小 那off就是新文件大小
  if (off > ip->size) {
    ip->size = off;
  }
  // bmap可能修改了inode 写回磁盘
  iupdate(ip);

  return tot;
}

// 比较文件名
int namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

// 给文件名 查找类型为文件夹的inode下的文件 返回文件描述符
struct inode *dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR) {
    panic("dirlookup not DIR");
  }

  // 遍历文件夹的文件项
  for (off = 0; off < dp->size; off += sizeof(de)) {
    // 读一个文件项到de
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
      panic("dirlookup read");
    }
    // 当前文件项无效
    if (de.inum == 0) {
      continue;
    }
    // 对比一下文件名
    if (namecmp(name, de.name) == 0) {
      if (poff) {
        // 修改poff为文件夹类型inode中文件夹项的偏移地址
        *poff = off;
      }
      // 读到了inode号
      inum = de.inum;
      // 分配一个inode内存节点
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// 给一个类型为文件夹的inode中写入一个新的文件夹项
int dirlink(struct inode *dp, char *name, uint inum) {
  int off;
  struct dirent de;
  struct inode *ip;

  // 检查文件夹是不是已经存在了 存在了返回-1
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // 寻找空的文件夹项
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
      panic("dirlink read");
    }
    if (de.inum == 0) {
      break;
    }
  }
  // 配置当前文件夹项
  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  // 修改文件夹inode的内容
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
    return -1;
  }
  return 0;
}
