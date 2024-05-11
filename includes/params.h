// CPU的最大数量
#define NCPU 8

// 设备数量最大值
#define NDEV 10

// 进程最大数
#define NPROC 64

#define MAXOPBLOCKS 10

#define LOGSIZE (MAXOPBLOCKS * 3)

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