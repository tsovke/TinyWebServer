#include "http_conn.h"
#include <sys/epoll.h>


//添加文件描述符到epoll中
void addfd(int epollfd,int fd, bool one_shot){
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLRDHUP;
  if (one_shot) {
    event.events | EPOLLONESHOT;
    
  }
  epoll_ctl(epollfd,EPOLL_CTL_ADD ,fd ,&event );
}

//从epoll中删除文件描述符
void removefd(int epollfd,int fd){
  
}
