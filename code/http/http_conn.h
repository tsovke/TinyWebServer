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
  static int m_epollfd; // 所有的socket的事件都注册到一个epollfd上
  static int m_user_count; // 统计用户的数量
  static const int READ_BUFFER_SIZE = 2048;
  static const int WRITE_BUFFER_SIZE = 2048;
  
  http_conn() {}
  ~http_conn() {}

  void process(); // 处理客户端请求
  void init(int sockfd, const sockaddr_in &addr);
  void close_conn(); // 关闭连接
  bool read();       // 非阻塞读
  bool write();      // 非阻塞读
private:
  int m_sockfd;          // 该http连接的socket
  sockaddr_in m_address; // 通信的socket地址
  char m_read_buf[READ_BUFFER_SIZE];
  int m_read_idx;//标识读缓冲区中以及读入的客户端数据的最后一个字节的下一位置
};
