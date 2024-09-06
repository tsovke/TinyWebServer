#include <cerrno>
#include <csignal>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include "../timer/lst_timer.hpp"
#include <stdexcept>

const int FD_LIMIT {65535};
const int MAX_EVEVNT_NUMBER {1024};
const int TIMESLOT {5};

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd{0};

int setnonblocking(int fd){
  int old_option = fcntl(fd,F_GETFL );
  int new_option=old_option|O_NONBLOCK;
  fcntl(fd,F_SETFL,new_option );
  return old_option;
}

void addfd(int epollfd,int fd){
  epoll_event event;
  event.data.fd=fd;
  event.events=EPOLLIN|EPOLLET;
  epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
  setnonblocking(fd);
}

void sig_handler(int sig){
  int save_errno=errno;
  int msg=sig;
  send(pipefd[1],(char *)&msg,1,0);
  errno=save_errno;
}

void addsig(int sig){
  struct sigaction sa;
  memset(&sa,'\0' ,sizeof(sa) );
  sa.sa_handler=sig_handler;
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig,&sa ,nullptr )!=1); 
}

void timer_handler(){
  timer_lst.tick();
  alarm(TIMESLOT);
}


void cb_func(client_data *user_data){
  epoll_ctl(epollfd,EPOLL_CTL_ADD ,user_data->sockfd ,nullptr );
  assert(user_data);
  close(user_data->sockfd);
  printf("close fd %d\n", user_data->sockfd);
}

int main(int argc,char *argv[])
{
  if (argc<=1) {
    printf("usage: %s port_number\n",basename(argv[0]));
  }

  int port=atoi(argv[1]);
  int ret{0};
  struct sockaddr_in address;
  memset(&address,'\0' ,sizeof(address) );
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=INADDR_ANY;
  address.sin_port=htons(port);

  int listenfd= socket(AF_INET, SOCK_STREAM,0 );
  assert(listenfd>=0);

  ret= bind(listenfd,reinterpret_cast<struct sockaddr *>(&address),sizeof(address));
  assert(ret!=-1);

  ret=listen(listenfd,5 );
  assert(ret!=-1);

  epoll_event events[MAX_EVEVNT_NUMBER];
  int epollfd= epoll_create(5);
  assert(epollfd!=-1);
  addfd(epollfd,listenfd );

  ret=socketpair(PF_UNIX,SOCK_STREAM ,0 ,pipefd);
  assert(ret!=-1);
  setnonblocking(pipefd[1]);
  addfd(epollfd,pipefd[0] );

  addsig(SIGALRM);
  addsig(SIGTERM);
  bool stop_server{false};

  client_data *users=new client_data[FD_LIMIT];
  bool timeout{false};
  alarm(TIMESLOT);

  while (!stop_server) {
    int number=epoll_wait(epollfd,events ,MAX_EVEVNT_NUMBER ,-1 );
    if ((number<0)&&(errno!=EINTR)) {
      throw std::runtime_error("epoll failure");
      break;
    }
    for (int i=0;i<number;++i ) {
      int sockfd=events[i].data.fd;
      if (sockfd==listenfd) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_length=sizeof(client_addr);
        int connfd =
            accept(listenfd, reinterpret_cast<struct sockaddr *>(&client_addr),
                   &client_addr_length);
          addfd(epollfd,connfd );
          users[connfd].address=client_addr;
          users[connfd].sockfd=connfd;

          util_timer *timer=new util_timer;
          timer->user_data=&users[connfd];
          timer->cb_func=cb_func;
          time_t cur=time(nullptr);
          timer->expire = cur + 3*TIMESLOT;
          users[connfd].timer=timer;
          timer_lst.add_timer(timer);
      }
      else if (sockfd==pipefd[0]&&events[i].events&EPOLLIN) {
        int sig;
        char signals[1024];
        ret=recv(pipefd[0],signals ,sizeof(signals) ,0 );
        if (ret==-1||ret==0) {
          continue;
        }else {
          for (int i=0;i<ret ;++i ) {
            switch (signals[i]) {
              case SIGALRM:
                timeout=true;
                break;
                case SIGTERM:
                  stop_server=true;
            }
          }
        }
      }

      else if (events[i].events&EPOLLIN) {
        memset(users[sockfd].buf,'\0' ,BUFFER_SIZE );
        ret=recv(sockfd,users[sockfd].buf ,BUFFER_SIZE ,0 );
        printf("get %d bytes of client data %s from %d\n",ret,users[sockfd].buf,sockfd);
        util_timer * timer=users[sockfd].timer;
        if (ret<0) {
          if (errno!=EAGAIN) {
            cb_func(&users[sockfd]);
            if (timer) {
              timer_lst.del_timer(timer);
            }
          }
        }
        else if (ret==0) {
          cb_func(&users[sockfd]);
          if (timer) {
            timer_lst.del_timer(timer);
          }
        }
        else
        {
          if (timer) {
            time_t cur=time(nullptr);
            timer->expire=cur+3*TIMESLOT;
            printf("adjust timer once\n");
            timer_lst.add_timer(timer);
          }
        }
      }
      
    }
    if (timeout) {
      timer_handler();
      timeout=false;
    }

    
  }
  close(listenfd);
  
  close(pipefd[1]);
  close(pipefd[0]);
  delete [] users;
  return 0;
}
