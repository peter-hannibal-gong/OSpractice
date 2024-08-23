#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_EXTRA_ARG_LEN 100
int my_gets(char* buf, int max) {  //读取字符串，读到文件末尾时返回-1
  int i, cc;
  char c;
  int res = 0;
  for (i = 0; i + 1 < max;) {
    cc = read(0, &c, 1);
    if (cc < 1) {
      res = -1;
      break;
    }
    if (c == '\n') break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return res;
}
int main(int argc, char* argv[]) {
  char extra_args[MAX_EXTRA_ARG_LEN];
  while (1) {
    int res = my_gets(extra_args, MAX_EXTRA_ARG_LEN);
    // 双重判断，避免\n\n的情况
    if (extra_args[0] == '\0' && res == -1) {
      break;
    }
    if (fork() == 0) {
      // 此时argv[0]为xargs，需将参数整体往前移动一位
      for (int i = 0; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
      }
      argv[argc - 1] = extra_args;  // 设置额外参数
      exec(argv[0], argv);
      exit(0);
    } else {
      wait((int*)0);
    }
  }
  exit(0);
}

