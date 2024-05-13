#include "includes/types.h"
#include "includes/stat.h"
#include "includes/fcntl.h"
#include "user/user.h"

// 防止main函数不调用exit
void _main() {
  extern int main();
  main();
  exit(0);
}

// 字符串拷贝
char *strcpy(char *s, const char *t) {
  char *os;

  os = s;
  while ((*s++ = *t++) != 0);
  return os;
}

// 字符串比较
int strcmp(const char *p, const char *q) {
  while (*p && *p == *q) p++, q++;
  return (uchar)*p - (uchar)*q;
}

// 字符串长度
uint strlen(const char *s) {
  int n;

  for (n = 0; s[n]; n++);
  return n;
}

// 内存赋值
void *memset(void *dst, int c, uint n) {
  char *cdst = (char *)dst;
  int i;
  for (i = 0; i < n; i++) {
    cdst[i] = c;
  }
  return dst;
}

// 字符串查找
char *strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c) {
      return (char *)s;
    }
  return 0;
}

// 从标准输入读取一行
char *gets(char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(0, &c, 1);
    if (cc < 1) {
      break;
    }
    buf[i++] = c;
    if (c == '\n' || c == '\r') {
      break;
    }
  }
  buf[i] = '\0';
  return buf;
}

// 查一个文件描述符的状态
int stat(const char *n, struct stat *st) {
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  r = fstat(fd, st);
  close(fd);
  return r;
}

// 字符串转整数
int atoi(const char *s) {
  int n;

  n = 0;
  while ('0' <= *s && *s <= '9') {
    n = n * 10 + *s++ - '0';
  }
  return n;
}

// 内存拷贝
void *memmove(void *vdst, const void *vsrc, int n) {
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  // 考虑到了内存重叠的情况
  if (src > dst) {
    while (n-- > 0) {
      *dst++ = *src++;
    }
  } else {
    dst += n;
    src += n;
    while (n-- > 0) {
      *--dst = *--src;
    }
  }
  return vdst;
}

// 内存比较
int memcmp(const void *s1, const void *s2, uint n) {
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

// 内存拷贝
void *memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}
