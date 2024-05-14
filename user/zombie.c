#include "includes/types.h"
#include "includes/stat.h"
#include "user/user.h"

int main(void) {
  if (fork() > 0) {
    sleep(50);
  }
  exit(0);
}
