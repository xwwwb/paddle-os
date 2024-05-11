// 文件系统相关系统调用
// 主要是一些参数检查
#include "types.h"
#include "params.h"
#include "syscall.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "file.h"
#include "stat.h"
#include "fcntl.h"

// 传入文件描述符的参数的下标n
// 取参数的值作为文件描述符 检查其正确性
// 写入文件描述符到pfd和 文件结构指针 到 文件指针的指针 pf
static int argfd(int n, int *pfd, struct file **pf) {
  int fd;
  struct file *f;
  // 拿到当前参数下标的值 存入fd
  argint(n, &fd);
  // fd应当是文件描述符
  // 如果小于0 或者大于进程打开文件数 或者当前文件描述符未被分配 报错
  if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0) {
    return -1;
  }
  if (pfd) {
    *pfd = fd;
  }
  if (pf) {
    *pf = f;
  }
  return 0;
}

// 给一个文件分配一个文件描述符 返回的是当前文件在进程的文件结构数组中的下标
static int fdalloc(struct file *f) {
  int fd;
  struct proc *p = myproc();

  for (fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd] == 0) {
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

// 增加文件的引用次数 返回一个新的文件描述符
// 只有一个参数是文件描述符
uint64 sys_dup(void) {
  struct file *f;
  int fd;
  // 检查第0个参数是否正确
  // 正确的话 写入文件结构指针到f中 写入文件描述符到0中 (无意义)
  if (argfd(0, 0, &f) < 0) {
    return -1;
  }
  // 新分配一个描述符
  if ((fd = fdalloc(f)) < 0) {
    return -1;
  }
  // 增加文件的引用
  filedup(f);
  return fd;
}

// 读文件
// 第一个参数是文件描述符
// 第二个参数是写入地址
// 第三个参数是大小
uint64 sys_read(void) {
  struct file *f;
  int n;
  uint64 p;
  // 拿到写入地址
  argaddr(1, &p);
  // 拿到写入大小
  argint(2, &n);
  // 检查文件描述符的可用性 并写入文件结构到f
  if (argfd(0, 0, &f) < 0) {
    return -1;
  }
  // 从f中读n个大小到p中
  return fileread(f, p, n);
}

// 写文件
// 第一个参数是文件描述符
// 第二个参数是用户态的地址 原数据
// 第三个参数是大小
uint64 sys_write(void) {
  struct file *f;
  int n;
  uint64 p;

  argaddr(1, &p);
  argint(2, &n);
  if (argfd(0, 0, &f) < 0) {
    return -1;
  }

  return filewrite(f, p, n);
}

// 关闭文件
// 第一个参数是文件描述符
uint64 sys_close(void) {
  int fd;
  struct file *f;
  // 检查文件描述符的正确性
  if (argfd(0, &fd, &f) < 0) {
    return -1;
  }
  myproc()->ofile[fd] = 0;
  // 关闭文件 减少引用 实际上操作的是文件结构
  fileclose(f);
  return 0;
}

// 获得文件状态
// 第一个参数是文件描述符
// 第二个参数是用户态的地址 用于保存stat结构到这个地址
uint64 sys_fstat(void) {
  struct file *f;
  uint64 st;

  argaddr(1, &st);
  if (argfd(0, 0, &f) < 0) {
    return -1;
  }
  return filestat(f, st);
}

// 创建一个文件的硬链接
// 创建一个新的路径连接到相同的Inode上
// 第一个参数是 老路径
// 第二个参数是 新路径
uint64 sys_link(void) {
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;
  // 将新老路径从参数中取出
  if (argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0) {
    return -1;
  }
  // 开启事务
  begin_op();
  // 找到老文件的文件描述符
  if ((ip = namei(old)) == 0) {
    end_op();
    return -1;
  }
  // 锁定inode
  ilock(ip);
  // 如果老的文件是目录 不允许 只允许给文件做硬链接
  if (ip->type == T_DIR) {
    iunlockput(ip);
    end_op();
    return -1;
  }
  // inode的硬链接数增加
  ip->nlink++;
  // 更新磁盘inode结构
  iupdate(ip);
  // 解锁inode
  iunlock(ip);

  // dp是文件夹inode name为new这个路径的最后一部分
  if ((dp = nameiparent(new, name)) == 0) {
    goto bad;
  }
  // 锁定Inode
  ilock(dp);
  // 给文件夹inode中加入一个文件夹项 名字叫name 指向的文件Inode是ip->inum
  if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
    iunlockput(dp);
    goto bad;
  }
  // 解锁文件夹inode
  iunlockput(dp);
  // namei -> namex -> iget -> ref++
  // 这里要减回去
  iput(ip);

  end_op();

  return 0;

bad:
  // 减少inode的硬链接数
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// 检查一个文件夹是不是空 除了.和..
static int isdirempty(struct inode *dp) {
  int off;
  struct dirent de;
  // 前俩文件一个是. 一个是.. 跳过这两个
  for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
      panic("isdirempty: readi");
    }
    if (de.inum != 0) {
      return 0;
    }
  }
  return 1;
}

// 取消文件和目录的硬链接
// 第一个参数是path
uint64 sys_unlink(void) {
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;
  // 取第一个参数
  if (argstr(0, path, MAXPATH) < 0) {
    return -1;
  }

  begin_op();
  // 找到文件的父目录的inode 并且把路径的最后部分拷贝到name
  if ((dp = nameiparent(path, name)) == 0) {
    end_op();
    return -1;
  }
  // 锁定父目录inode
  ilock(dp);

  // 如果当前目录是.或者.. 不能unlink
  if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
    goto bad;
  }
  // 找到路径的文件描述符 并且写入off为文件夹项在文件夹目录中的偏移
  // 在dp 文件夹inode中找到名为name的inode
  if ((ip = dirlookup(dp, name, &off)) == 0) {
    goto bad;
  }
  // 锁定文件描述符
  ilock(ip);
  // 不能unlink 0
  if (ip->nlink < 1) {
    panic("unlink: nlink < 1");
  }
  // 如果被unlink的文件是目录 且目录非空 不能解除link
  if (ip->type == T_DIR && !isdirempty(ip)) {
    iunlockput(ip);
    goto bad;
  }
  // 给预先准备的de写0
  memset(&de, 0, sizeof(de));
  // 父目录的inode的第off项写0
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
    panic("unlink: writei");
  }
  // 当一个目录被删除时 需要将父目录的连接次数-1
  if (ip->type == T_DIR) {
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);
  // 文件的硬链接数减少
  ip->nlink--;
  iupdate(ip);
  // 如果硬链接数为0 iput会删除文件回收inode
  iunlockput(ip);
  // 结束事务
  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

// 创建一个inode
// path是路径 type是inode type major和minor都是设备号
static struct inode *create(char *path, short type, short major, short minor) {
  struct inode *ip, *dp;
  char name[DIRSIZ];
  // 找到path的父目录
  // name是path最后一部分
  // 如果path的父目录inode为0 失败
  if ((dp = nameiparent(path, name)) == 0) {
    return 0;
  }
  // 锁定目录
  ilock(dp);
  // 查找父目录下的名字有没有名字重复的
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    // 解锁父目录
    iunlockput(dp);
    ilock(ip);
    // 返回这个重名的文件
    if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  // 分配一个inode
  if ((ip = ialloc(dp->dev, type)) == 0) {
    iunlockput(dp);
    return 0;
  }
  // 写入inode的信息
  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);
  // 如果当前创建的是文件夹
  if (type == T_DIR) {
    // 创建. 和..这两个文件夹项
    if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }
  // 在父文件夹里创建name和inode的连接
  if (dirlink(dp, name, ip->inum) < 0) {
    goto fail;
  }
  // 如果新建立的文件是文件夹 那么父文件夹新增一个硬链接
  // 是因为会有一个..指向他
  if (type == T_DIR) {
    dp->nlink++;
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

fail:
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

// 打开文件 分配文件描述符
// 第一个参数是path
// 第二个参数是模式
uint64 sys_open(void) {
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;
  // 取模式
  argint(1, &omode);
  // 取path
  if ((n = argstr(0, path, MAXPATH)) < 0) {
    return -1;
  }
  // 开启事务
  begin_op();
  // 如果是创建文件
  if (omode & O_CREATE) {
    //  调用create函数
    // 设备号都是0
    ip = create(path, T_FILE, 0, 0);
    if (ip == 0) {
      end_op();
      return -1;
    }
  } else {
    // 不是创建文件就是打开文件
    // 拿到文件描述符
    if ((ip = namei(path)) == 0) {
      end_op();
      return -1;
    }
    // 锁定文件描述符
    ilock(ip);
    // 不允许打开文件夹和非只读共存
    if (ip->type == T_DIR && omode != O_RDONLY) {
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  // 如果当前的inode类型是设备 (模式是打开文件 因为创建文件只能创建FILE类型)
  // 但是设备号不合法 报错
  if (ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
    iunlockput(ip);
    end_op();
    return -1;
  }
  // 分配一个文件结构体 分配一个文件描述符
  if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
    if (f) {
      fileclose(f);
    }
    iunlockput(ip);
    end_op();
    return -1;
  }

  if (ip->type == T_DEVICE) {
    // 如果打开的是设备
    // 文件结构中type写入类型是设备
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    // 如果是新创建文件或者打开的是文件
    // 文件结构中的inode类型也是inode
    f->type = FD_INODE;
    f->off = 0;
  }
  // 文件结构体中的inode
  f->ip = ip;
  // 如果模式是只允许写入 那么该文件不可读
  f->readable = !(omode & O_WRONLY);
  // 如果是只允许写入或者可读可写 该文件可写
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  // 如果模式是删除 且是文件类型 删除该inode和内容
  if ((omode & O_TRUNC) && ip->type == T_FILE) {
    // 这里不用调用unlink吗
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

// 创建文件夹
// 第一个参数是路径
uint64 sys_mkdir(void) {
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  // 将路径写入path 然后再path的地方创建一个目录类型的inode
  if (argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0) {
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

// 创建字符设备文件
// 第一个参数是路径 第2 3 个路径是主设备号和辅设备号
uint64 sys_mknod(void) {
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  argint(1, &major);
  argint(2, &minor);
  // 路径拷贝到path
  // 创建设备inode到路径
  if ((argstr(0, path, MAXPATH)) < 0 ||
      (ip = create(path, T_DEVICE, major, minor)) == 0) {
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

// 更改进程的目录
// 第一个参数是路径
uint64 sys_chdir(void) {
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();

  begin_op();
  // 参数复制到path
  // 找到path指向的inode
  if (argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0) {
    end_op();
    return -1;
  }
  ilock(ip);
  // 如果inode不是目录 失败 因为改变目录就是改变的文件夹
  if (ip->type != T_DIR) {
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  // 减少原来的cwd的inode的引用
  iput(p->cwd);
  end_op();
  // 进程中cwd指向新目录
  p->cwd = ip;
  return 0;
}

// 替换进程
// 第一个参数是新进程的路径
// 第二个参数是新进程的args
uint64 sys_exec(void) {
  char path[MAXPATH], *argv[MAXARG];

  int i;
  uint64 uargv, uarg;

  // 取参数中的指针
  argaddr(1, &uargv);
  // 取参数中的路径
  if (argstr(0, path, MAXPATH) < 0) {
    return -1;
  }
  // 先清空argv
  memset(argv, 0, sizeof(argv));
  // 塞入argv
  for (i = 0;; i++) {
    if (i >= NELEM(argv)) {
      goto bad;
    }
    // 从uargv中取当前字符串地址取到uarg里面
    if (fetchaddr(uargv + sizeof(uint64) * i, (uint64 *)&uarg) < 0) {
      goto bad;
    }
    // 地址为0 说明取完了
    if (uarg == 0) {
      argv[i] = 0;
      break;
    }
    // 开辟一个物理空间存字符串
    argv[i] = kalloc();
    if (argv[i] == 0) {
      goto bad;
    }
    // 把uarg指向的字符串复制到物理页中
    if (fetchstr(uarg, argv[i], PGSIZE) < 0) {
      goto bad;
    }
  }
  // argv是一个char**
  int ret = exec(path, argv);
  // exec已经拷贝参数到用户栈了
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++) {
    kfree(argv[i]);
  }
  // 这里的ret写入trampframe的a0了 ret是argc 这样exec实际上执行的
  // main(argc, argv) 参数都齐了
  return ret;
bad:
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++) {
    kfree(argv[i]);
  }
  return -1;
}