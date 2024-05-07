#define ROOTINO 1   // 根目录的inode number是1
#define BSIZE 1024  // 块大小是1024Byte

// 启动区 | superblock | 日志区 | inode区 | 空闲位 | 数据区
// 超级快
struct superblock {
  uint magic;       // 文件系统魔法标识
  uint size;        // 块总数
  uint nblocks;     // 数据块总数
  uint ninodes;     // inodes区占用块总数
  uint nlog;        // 日志区占用块总数
  uint logstart;    // 第一个日志块的块号
  uint inodestart;  // 第一个inode区的块号
  uint bmapstart;   // 第一个位图区的块号
};

// paddle-fs的魔术标识
#define FSMAGIC 0x00627778

// 直接映射数量是12
#define NDIRECT 12
// 间接文件索引块 每个索引值是一个uint 每个块可以管理256个索引号
#define NINDIRECT (BSIZE / sizeof(uint))
// 单文件最大是 268个block 也就是268KB
#define MAXFILE (NDIRECT + NINDIRECT)

// 文件系统中的inode结构
struct dinode {
  short type;   // 文件类型
  short major;  // 主设备号
  short minor;  // 辅设备号
  short nlink;  // 当前inode被多少个文件夹引用 多少个硬链接
  uint size;    // 文件大小 单位字节
  // 数据区块块号 按顺序排列 最后一位是间接文件索引块的块号
  uint addrs[NDIRECT + 1];
};

// 每一个block可以放多少个dinode
// 1024 / 64 = 16个
#define IPB (BSIZE / sizeof(struct dinode))

// 计算出哪个block存放着当前的inode
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// 每个block的位图可以表示几位
// 8192 位 当前INODE数量小于这个数 所以一个block的位图就能管理全盘了
#define BPB (BSIZE * 8)

// 当前的块号是位图区哪个块管理的
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// 文件夹是一个文件 顺序排列的dirent
// DIRSIZE是文件夹名 也就是路径名
// 也可以是单文件名
#define DIRSIZ 14

struct dirent {
  ushort inum;        // 该文件夹索引指向哪个inode
  char name[DIRSIZ];  // 当前的目录名
};
