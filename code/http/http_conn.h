#pragma once

#include "../lock/locker.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

class http_conn {
public:
  static int m_epollfd;//所有的socket的事件都注册到一个epollfd上
  static int m_user_count;// 统计用户的数量
  http_conn() {}
  ~http_conn() {}

  
  void process(); // 处理客户端请求

private:
  int m_sockfd;//该http连接的socket
  sockaddr_in m_address;//通信的socket地址
};
