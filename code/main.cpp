#include "http/http_conn.h" #include "lock/locker.hpp" #include "threadpool/threadpool.hpp" #include < arpa / inet.h> #include < csignal> #include < cstdlib> #include < errno.h> #include < fcntl.h> #include < netinet / in.h> #include < signal.h> #include < stdio.h>
#include "lock/locker.hpp"
#include "threadpool/threadpool.hpp"
#include <arpa/inet.h>
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

#define MAX_FD 65535 // 最大文件描述符数量
#define MAX_EVENT_NUMBER 10000 // 监听的最大支持事件数

// 添加信号捕捉
void addsig(int sig, void (*handler)(int)) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigfillset(&sa.sa_mask);
  sigaction(sig, &sa, nullptr);
}

//添加文件描述符到epoll中
extern int addfd(int epollfd,int fd, bool one_shot);

//从epoll中删除文件描述符
extern int removefd(int epollfd,int fd);

int main(int argc, char *argv[]) {

  if (argc <= 1) {
    printf("按照如下格式运行： %s port_number\n", basename(argv[1]));
    exit(-1);
  }

  // 获取端口号
  int port = atoi(argv[1]);

  // 对SIG_PIPE信号进行处理
  addsig(SIGPIPE, SIG_IGN);

  // 创建线程池，初始化线程池
  threadpool<http_conn> *pool = nullptr;
  try {
    pool = new threadpool<http_conn>;
  } catch (...) {
    exit(-1);
  }

  // 创建一个数组用于保存所有的客户端信息
  http_conn *user = http_conn[MAX_FD];

  // 创建监听的套接字
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    perror("socket");
    exit(-1);
  }

  // 设置端口复用
  int yes = 1;
  setsockopt(listenfd, SOCK_STREAM, SO_REUSEADDR, &yes, sizeof(yes));

  // 绑定
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
  if (ret == -1) {
    perror("bind");
    exit(-1);
  }

  // 监听
  listen(listenfd, 5);

  //创建epoll对象，事件数组，添加
  epoll_event events[MAX_EVENT_NUMBER];
  int epollfd = epoll_create(5);

  return 0;
}
