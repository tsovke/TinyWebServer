#include "http_conn.h"
#include <cstdio>
#include <fcntl.h>
#include <sys/epoll.h>

// 设置文件描述符非阻塞
void setnonblocking(int fd) {
  int flag = fcntl(fd, F_GETFL);
  flag |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flag);
}

// 添加文件描述符到epoll中
void addfd(int epollfd, int fd, bool one_shot) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLRDHUP;
  if (one_shot) {
    event.events = event.events | EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

  // 设置文件描述符非阻塞
  setnonblocking(fd);
}

// 从epoll中删除文件描述符
void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

// 修改文件描述符
void modfd(int epollfd, int fd, int ev) {
  epoll_event event;
  event.data.fd = fd;
  event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in &addr) {
  m_sockfd = sockfd;
  m_address = addr;

  // 设置端口复用
  int reuse = 1;
  setsockopt(m_sockfd, SOCK_STREAM, SO_REUSEADDR, &reuse, sizeof(reuse));

  // 添加到epoll对象中
  addfd(m_epollfd, m_sockfd, true);
  ++m_user_count; // 总用户数+1
}

// 关闭连接
void http_conn::close_conn() {
  if (m_sockfd != -1) {
    removefd(m_epollfd, m_sockfd);
    m_sockfd = -1;
    --m_user_count; // 总客户数-1
  }
}

bool http_conn::read() {
  printf("一次性读完数据\n");
  return true;
}
bool http_conn::write() {
  printf("一次性写完数据\n");
  return true;
}

//由线程池中的工作线程调用，这是处理HTTP请求那入口函数
void http_conn::process(){
  //解析HTTP请求
  printf("parse request, create response\n");
  //生成响应
}

