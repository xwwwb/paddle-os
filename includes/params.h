// CPU的最大数量
#define NCPU 8

// 设备数量最大值
#define NDEV 10

// 进程最大数
#define NPROC 64

// 文件系统可操作的最大文件块数
#define MAXOPBLOCKS 10

// 日志块大小
#define LOGSIZE (MAXOPBLOCKS * 3)

// 磁盘buffer的大小
#define NBUF (MAXOPBLOCKS * 3)

// 最大的活跃的inode数量 加载到内存中的Inode数
#define NINODE 50

// 系统最多可以打开文件数
#define NFILE 100

// 文件系统可以管理的块数 也就是2000kb的大小的磁盘
#define FSSIZE 2000

// 磁盘设备号
#define ROOTDEV 1

// exec可以传入的最大的参数
#define MAXARG 32

// 最大路径长度
#define MAXPATH 128

// 每个进程可以打开的文件数
#define NOFILE 16

// 系统可以打开的文件数
#define NFILE 100