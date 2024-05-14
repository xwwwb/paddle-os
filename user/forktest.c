#include "includes/types.h"
#include "includes/stat.h"
#include "user/user.h"

int main() {
  int pid;
  pid = fork();
  if (pid == 0) {
    printf("这里是子进程\n");
  } else {
    sleep(1);
    printf("这里是父进程\n");
  }
  exit(0);
}