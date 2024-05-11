struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref;  // 引用次数
  char readable;
  char writable;
  struct pipe *pipe;  // FD_PIPE
  struct inode *ip;   // FD_INODE and FD_DEVICE
  uint off;           // FD_INODE 使用的属性 当前文件的读指针偏移
  short major;        // FD_DEVICE
};

struct inode {
  uint dev;               // 设备号
  uint inum;              // inode号
  int ref;                // 引用数量
  struct sleeplock lock;  // 保护读写inode
  int valid;              // inode是否被读入
  short type;             // 文件类型
  short major;            // 设备号主
  short minor;            // 设备号次
  short nlink;            // 硬链接数
  uint size;              // 文件大小
  uint addrs[NDIRECT + 1];  // 映射地址 前12个直接映射 第13个间接映射
};

// 映射设备号到设备函数
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

// 向外声明有名为devsw的数组 实际定义在file.c中
extern struct devsw devsw[];

// 控制台设备号是1
#define CONSOLE 1