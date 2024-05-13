#include "includes/types.h"
#include "includes/stat.h"
#include "user/user.h"

char buf[512];

void cat(int fd) {
  int n;
  // 每次读取512字节
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    // 向标准输出写入n字节
    if (write(1, buf, n) != n) {
      fprintf(2, "cat: write error\n");
      exit(1);
    }
  }
  if (n < 0) {
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  int fd, i;
  // 检查参数
  if (argc <= 1) {
    cat(0);
    exit(0);
  }
  // 依次打开文件并输出
  for (i = 1; i < argc; i++) {
    if ((fd = open(argv[i], 0)) < 0) {
      fprintf(2, "cat: cannot open %s\n", argv[i]);
      exit(1);
    }
    cat(fd);
    close(fd);
  }
  exit(0);
}
