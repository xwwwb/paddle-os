// 内存中inode类型
#define T_DIR 1     // 文件夹
#define T_FILE 2    // 文件
#define T_DEVICE 3  // 当前文件是设备

// 当前inode状态
struct stat {
  int dev;      // 设备号
  uint ino;     // inode号
  short type;   // 类型
  short nlink;  // 硬链接数
  uint64 size;  // 文件大小
};
