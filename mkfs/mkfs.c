// 创建磁盘镜像 并且格式化为paddle-fs 文件系统
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat  // 避免使用的stat是host Linux的stat
#include "includes/types.h"
#include "includes/fs.h"
#include "includes/stat.h"
#include "includes/params.h"

// 定义一下静态断言
#ifndef static_assert
#define static_assert(a, b) \
  do {                      \
    switch (0)              \
    case 0:                 \
    case (a):;              \
  } while (0)
#endif

// INODE的数量 可以保存200个文件/文件夹
#define NINODES 200

// 磁盘层级
// 启动区 | superblock | 日志区 | inode区 | 空闲位 | 数据区

// 用总块数除以一个块能表示多少的块的位图
// 得到块数是位图的数量 + 1 是向上取整
int nbitmap = FSSIZE / (BSIZE * 8) + 1;
// IPB 是块大小除以inode结构大小 也就是一个块能装多个inode
// 这里算完ninodeblocks是inode区域所用的块数
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // 元数据所占区块个数
int nblocks;  // 数据块个数

// 文件描述符
int fsfd;
// 超级快
struct superblock sb;
// 一个为0的空闲区块 全局变量初始化后是0
char zeroes[BSIZE];
// inode编号从1开始
uint freeinode = 1;
// 空余块数
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
void die(const char *);

// 转换字节序为小端序 即在一个数据类型中 字节序和常规的字节序是反的
// 存储到硬盘后 应该是小端序
ushort xshort(ushort x) {
  ushort y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint xint(uint x) {
  uint y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

// 每一个inode表示一个文件 inode中有addr按顺序记录着文件存储在data区的block号
// addr最后一位记录了一个块号 该块号存的数据结构是 int[256]
// 每个int指向一个新的块号 相当于二级块表 一个inode可以表示256+12个数据块
// 一个块1k 所以最大存储文件是268kb
// dinode是文件系统的文件索引节点结构
// 当一个inode是文件夹类型的时候 该inode指向的文件（块）中
// 顺序排布着dirent[] 结构
int main(int argc, char *argv[]) {
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;

  // 断言int是4 否则不允许在该操作系统使用mkfs
  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  // 参数不够
  if (argc < 2) {
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  // 断言文件索引节点和文件夹记录项大小是1024的约数
  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  // 打开参数中传递的文件 新建磁盘镜像
  fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fsfd < 0) die(argv[1]);

  // 元信息包括一个boot块 一个superblock n个log
  // n个inode块（算出来的200个inode占多少）
  // n个位图块
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  // 数据块就是总块数-nmeta
  nblocks = FSSIZE - nmeta;

  // 文件系统的魔法标记
  sb.magic = FSMAGIC;
  // 总共有2000个块
  sb.size = xint(FSSIZE);
  // 数据块数量
  sb.nblocks = xint(nblocks);
  // 200个inodes
  sb.ninodes = xint(NINODES);
  // 30个块用作日志
  sb.nlog = xint(nlog);
  // log块起始是2号块
  sb.logstart = xint(2);
  // inode块紧跟着log块
  sb.inodestart = xint(2 + nlog);
  // 位图块跟着inode块
  sb.bmapstart = xint(2 + nlog + ninodeblocks);
  // 打印当前磁盘信息
  printf(
      "nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) "
      "blocks %d total %d\n",
      nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

  // nmeta的值就是第一个空闲的数据块
  freeblock = nmeta;  // 可以开始分配数据的第一个block

  // 所有块都写0
  for (i = 0; i < FSSIZE; i++) {
    wsect(i, zeroes);
  }

  // 清除buf buf是函数内一个操作block的临时块
  memset(buf, 0, sizeof(buf));
  // 存入superblock
  memmove(buf, &sb, sizeof(sb));
  // buf 写到磁盘的第一个块
  wsect(1, buf);

  // 创建根目录的inode 根目录文件的类型是dir
  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);

  // 创建名字为.的目录项并且放入根目录的inode中
  bzero(&de, sizeof(de));
  // 该目录项指向根目录本身
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  // 追加到inode管理的文件中
  iappend(rootino, &de, sizeof(de));

  // ..目录也放进去
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  // 拷贝其余参数文件
  for (i = 2; i < argc; i++) {
    // 拷贝文件的时候 去掉user/前缀
    char *shortname;
    if (strncmp(argv[i], "user/", 5) == 0)
      shortname = argv[i] + 5;
    else
      shortname = argv[i];

    // 确保文件名中没有/
    // 断言当条件为假会报错
    // 如果文件名中带/ 则index(shortname,'/')返回一个指针
    assert(index(shortname, '/') == 0);

    // 如果读取失败 也报错
    if ((fd = open(argv[i], 0)) < 0) {
      die(argv[i]);
    }
    // 我们拷贝的文件中可能带前缀下划线
    // 为了避免真实操作系统采用了本地PATH引发错误
    // 拷贝的时候去掉先
    if (shortname[0] == '_') shortname += 1;
    // 分配一个inode
    inum = ialloc(T_FILE);
    // 分配一个文件夹目录项指向该文件并且把文件夹目录项放入根目录
    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, shortname, DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    // 每次读1024字节然后写入到inode表示的文件中
    while ((cc = read(fd, buf, sizeof(buf))) > 0) {
      iappend(inum, buf, cc);
    }
    // 关闭当前文件 拷贝完成
    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off / BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  balloc(freeblock);

  exit(0);
}

// 向文件中写入 sec是目标块号 buf是数据
void wsect(uint sec, void *buf) {
  // 根据块号计算出偏移量 修改文件指针
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    die("lseek");
  }
  // 写入
  if (write(fsfd, buf, BSIZE) != BSIZE) {
    die("write");
  }
}

// 给inode号 和inode结构 写入到磁盘
void winode(uint inum, struct dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  // 一个dinode 64byte 一个block 1024 一个块可以存16个inode
  // 也就是当前inode号除以16 在加上inode区开始的块号
  // 拿到要写入的inode在哪个块的块号
  bn = IBLOCK(inum, sb);
  // 读这个块进buf
  rsect(bn, buf);
  // 把当前buf的地址看作dinode指针 然后做指针加减法就可以走到
  // inum的64byte区 inum % IPB 就是取余数 偏移
  dip = ((struct dinode *)buf) + (inum % IPB);
  // 此时dip就是被定位的dinode指针 等于传入的ip
  *dip = *ip;
  // 将被修改的inode buf写入磁盘
  wsect(bn, buf);
}

// 读inode到buf 和winode反着来
void rinode(uint inum, struct dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode *)buf) + (inum % IPB);
  *ip = *dip;
}

// 从文件中读入一个block
void rsect(uint sec, void *buf) {
  // 先修改文件指针 再读入
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    die("lseek");
  }
  if (read(fsfd, buf, BSIZE) != BSIZE) {
    die("read");
  }
}

// 分配一个inode
uint ialloc(ushort type) {
  // inum自增 第一次调用的时候返回1
  uint inum = freeinode++;
  struct dinode din;

  // bzero是C标准库 向地址写0
  bzero(&din, sizeof(din));
  // 设置有一个硬链接 后续会让一个文件夹inode item包含这里
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

// 简单的位图映射 传入使用的块数
// 假设块是按顺序覆盖的 给前used块设置为1
// 目前只写入位图的第一个块
void balloc(int used) {
  uchar buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  // 断言用的区块数量小于一个块能表示的位图数量
  assert(used < BSIZE * 8);
  // buf区写0
  bzero(buf, BSIZE);
  // 逐bit修改
  for (i = 0; i < used; i++) {
    // i / 8拿到的是字节号 也就是buf的下标
    // 0x1 << (i % 8) 得到的就是一个第i位为1的二进制
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
  // 直接写入第一个block了 这里不考虑位图使用第二个block的情况
  wsect(sb.bmapstart, buf);
}

// 简易数值比较
#define min(a, b) ((a) < (b) ? (a) : (b))

// 向inode中追加数据
// 就是inode中的addr找到空闲的位置然后记录数据所在的块号
// inum是inode号 xp是
void iappend(uint inum, void *xp, int n) {
  char *p = (char *)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  // 把inode读进来
  rinode(inum, &din);
  // 计算当前的inode装的文件大小
  // 因为新放入的数据要跟在后面
  off = xint(din.size);
  // 这里n是字节 逐字节拷贝
  while (n > 0) {
    // 取到当前追加的内容要追加到inode管理的第fbn个block中
    fbn = off / BSIZE;
    // MAXFILE是256+12 = 268 单文件只能放268个块 再多就不行了
    assert(fbn < MAXFILE);
    if (fbn < NDIRECT) {
      // 没有用到第13个地址 也就是没用到二级文件索引
      if (xint(din.addrs[fbn]) == 0) {
        // 如果当前的文件大小是1024的倍数 则addrs[fbn]应当是0
        // 新占用一个data区的块 并且指过去
        din.addrs[fbn] = xint(freeblock++);
      }
      // x就是需要写到inode里面的块号
      x = xint(din.addrs[fbn]);
    } else {
      // 如果addrs的前12个块都用了 即当前文件已经超过了 12kb
      // 就用第13个字节指向的二级文件索引
      if (xint(din.addrs[NDIRECT]) == 0) {
        // 如果第13个块没有被用 先分配一个数据区的块让第13个地址指过去
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      // 把这个简介文件索引块读入
      rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      // 如果fbn是1024的倍数 这里需要新开一个空闲的数据块
      if (indirect[fbn - NDIRECT] == 0) {
        // 新分配数据块
        indirect[fbn - NDIRECT] = xint(freeblock++);
        // 让间接文件索引块先保存一下 因为有新的指向被分配了
        wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      }
      // x是数据实际被写入的块号
      x = xint(indirect[fbn - NDIRECT]);
    }
    // n是需要写入的字节数
    // 后面的参数是 当前块的剩余空间 写入的时候不能超过当前块
    // 剩余空间
    n1 = min(n, (fbn + 1) * BSIZE - off);
    // 先读出需要写的块的内容到buf
    rsect(x, buf);
    // off - fbn * BSIZE 是在本次被写入块中的偏移地址
    // 在加buf（首地址）就是实际写入的地址
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    // 写会磁盘
    wsect(x, buf);
    n -= n1;    // 剩余未拷贝字节
    off += n1;  // off是文件文件总大小
    p += n1;    // p是拷贝的源
  }
  // 存入新的大小
  din.size = xint(off);
  // 修改inode
  winode(inum, &din);
}

// 报错
void die(const char *s) {
  perror(s);
  exit(1);
}
