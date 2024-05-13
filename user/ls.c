#include "includes/types.h"
#include "includes/stat.h"
#include "user/user.h"
#include "includes/fs.h"

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // 找到最后一个/的位置
  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  if (strlen(p) >= DIRSIZ) {
    return p;
  }

  memmove(buf, p, strlen(p));
  // 填充空格
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

// 传入路径
void ls(char *path) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 打开路径
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }
  // 查看文件状态
  if (fstat(fd, &st) < 0) {
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  // 查看当前文件状态
  switch (st.type) {
    case T_DEVICE:
    case T_FILE: {
      // 如果是文件或者设备
      // 打印文件名 文件类型 文件inode 文件大小
      printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
      break;
    }
    case T_DIR: {
      // 如果是ls目录
      // 路径长度检查
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("ls: path too long\n");
        break;
      }
      // 拷贝路径到buf
      strcpy(buf, path);
      p = buf + strlen(buf);
      // 让文件夹以/结尾
      *p++ = '/';
      // 每次从文件中读一个dirent
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) {
          continue;
        }
        // 把文件名拷贝到buf
        memmove(p, de.name, DIRSIZ);
        // 强制以\0结尾
        p[DIRSIZ] = 0;
        // 查看该文件的状态
        if (stat(buf, &st) < 0) {
          printf("ls: cannot stat %s\n", buf);
          continue;
        }
        // 打印文件名 文件类型 文件inode 文件大小
        printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
      }
      break;
    }
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  printf("filename       type inode size\n");
  int i;
  // 如果没有参数，则默认显示当前目录
  if (argc < 2) {
    ls(".");
    exit(0);
  }
  // 依次显示参数中的目录
  for (i = 1; i < argc; i++) {
    ls(argv[i]);
  }
  exit(0);
}
