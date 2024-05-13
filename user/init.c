// 初始化控制台设备 启动shell
#include "includes/types.h"
#include "includes/stat.h"
#include "includes/spinlock.h"
#include "includes/sleeplock.h"
#include "includes/fs.h"
#include "includes/file.h"
#include "user/user.h"
#include "includes/fcntl.h"

char *argv[] = {"sh", 0};

int main(void) {
  int pid, wpid;

  // fid 0 标准输入 打开控制台文件 如果没有则创建
  if (open("console", O_RDWR) < 0) {
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }

  dup(0);  // fid 1 标准输出
  dup(0);  // fid 2 标准错误输出
  for (;;) {
    printf("\n正在启动shell\n\n");
    printf("欢迎进入 PADDLE OS\n");
    pid = fork();
    if (pid < 0) {
      printf("进程fork失败\n");
      exit(1);
    }
    // fork成功
    if (pid == 0) {
      exec("sh", argv);
      printf("shell启动失败\n");
      exit(1);
    }

    // 父进程要等待shell退出
    // 或者等待其他孤儿进程退出
    // 因为kernel会将孤儿进程的父进程设置为init
    for (;;) {
      // wpid是退出的进程的pid
      wpid = wait((int *)0);
      // 这里的pid是shell的pid 因为fork会在父进程返回子进程的pid
      if (wpid == pid) {
        // shell退出了 重新启动shell
        break;
      } else if (wpid < 0) {
        printf("等待子进程退出失败\n");
        exit(1);
      } else {
        // 孤儿进程 不做处理
      }
    }
  }
}