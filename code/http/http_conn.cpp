#include "http_conn.h"
#include <sys/epoll.h>


//添加文件描述符到epoll中
void addfd(int epollfd,int fd, bool one_shot){
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLRDHUP;
  if (one_shot) {
    event.events= event.events | EPOLLONESHOT;
    
  }
  epoll_ctl(epollfd,EPOLL_CTL_ADD ,fd ,&event );
}

//从epoll中删除文件描述符
void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd ,0);
    close(fd);
}

//修改文件描述符
void modfd(int epollfd,int fd,int ev){
  epoll_event event;
  event.data.fd = fd;
  event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  epollfd_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}
