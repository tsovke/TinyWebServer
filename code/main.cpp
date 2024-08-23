#include "lock/locker.hpp"
#include "threadpool/threadpool.hpp"
#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

// 添加信号捕捉
void addsig(int sig, void (*handler)(int)) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigfillset(&sa.sa_mask);
  sigaction(sig, &sa, nullptr);
}

int main(int argc, char *argv[]) {

  if (argc <= 1) {
    printf("按照如下格式运行： %s port_number\n", basename(argv[1]));
    exit(-1);
  }

  // 获取端口号
  int port = atoi(argv[1]);

  //对SIG_PIPE信号进行处理
  addsig(SIGPIPE, SIG_IGN);

  //创建线程池，初始化线程池
  threadpool<http_conn> pool = nullptr;
  try {
    pool=new threadpool<http_conn>;
  } catch (...) {
    exit(-1);
  }

  return 0;
}
