#include "http_conn.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

int http_conn::m_epollfd{-1};
int http_conn::m_user_count{0};
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
  // event.events = EPOLLIN | EPOLLRDHUP;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

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

  init();
}

void http_conn::init() {
  m_check_state = CHECK_STATE_REQUESTLINE; // 初始化状态为解析请求行
  m_checked_idx = 0;
  m_start_line = 0;
  m_read_idx = 0;
  m_method = GET;
  m_url = 0;
  m_version = 0;
  m_linger= false;

  std::memset(m_read_buf, 0, READ_BUFFER_SIZE);
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

  int bytes_read{0};
  while (true) {
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                      READ_BUFFER_SIZE - m_read_idx, 0);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 没有数据
        break;
      }
    } else if (bytes_read == 0) {
      // 对方关闭连接
      return false;
    }
    m_read_idx += bytes_read;
  }
  printf("读取到了数据：%s\n", m_read_buf);
  return true;
}
bool http_conn::write() {
  printf("一次性写完数据\n");
  return true;
}

http_conn::HTTP_CODE http_conn::process_read() {
  LINE_STATUS line_status = LINE_OK;
  HTTP_CODE ret = NO_REQUEST;
  char *text = 0;
  while (
      ((m_check_state == CHECK_STATE_CONNTENT) && (line_status == LINE_OK)) ||
      ((line_status = parse_line()) == LINE_OK)) {
    // 解析到了一行完整的数据，或者解析到了请求但，也是完成的数据

    // 获取一行数据
    text = get_line();
    m_start_line = m_checked_idx;
    printf("got 1 http line : %s\n", text);
    switch (m_check_state) {
    case CHECK_STATE_REQUESTLINE: {
      ret = parse_request_line(text);
      if (ret == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }

    case CHECK_STATE_HEADER: {
      ret = parse_headers(text);
      if (ret == BAD_REQUEST) {
        return BAD_REQUEST;
      } else if (ret == GET_REQUEST) {
        return do_request();
      }
      break;
    }

    case CHECK_STATE_CONNTENT: {
      ret = parse_content(text);
      if (ret == BAD_REQUEST) {
        return BAD_REQUEST;
      } else if (ret == GET_REQUEST) {
        return do_request();
      }
      line_status = LINE_OPEN;
      break;
    }
    default: {
      return INTERNAL_ERROR;
    }
    }
  }
  return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
  // GET /index.html HTTP/1.1
  m_url = strpbrk(text, " \t");

  // GET\0/index.html HTTP/1.1
  *m_url++ = '\0';

  char *method = text;
  if (strcasecmp(method, "GET") == 0) {
    m_method = GET;
  } else {
    return BAD_REQUEST;
  }

  // /index.html HTTP/1.1
  m_version = strpbrk(m_url, " \t");
  if (!m_version) {
    return BAD_REQUEST;
  }

  // /index.html\0HTTP/1.1
  if (strcasecmp(m_version, "HTTP/1.1") != 0) {
    return BAD_REQUEST;
  }

  // http://192.168.1.1:10000/index.html
  if (strncasecmp(m_url, "http://", 7) == 0) {
    m_url += 7;                 // 192.168.1.1:10000/index.html
    m_url = strchr(m_url, '/'); // /index.html
  }

  if (!m_url || m_url[0] != '/') {
    return BAD_REQUEST;
  }

  m_check_state = CHECK_STATE_HEADER; // 主状态机检查状态变成检查请求头
  return NO_REQUEST;
}
// 解析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
  // 遇到空行，表示头部字段解析完毕
  if (text[0]=='\0') {
    //如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体
    //状态转移到CHECK_STATE_CONTENT状态
    if (m_content_length!=0) {
      m_check_state = CHECK_STATE_CONNTENT;
      return NO_REQUEST;
    }
    // 否则说明我们已经得到了一个完整的HTTP请求
    return GET_REQUEST;
  }else if (strncasecmp(text,"Connection:",11 )==0) {
    //处理Connection头部字段 Connection: keep-alive
    text+=11;
    text +=strspn(text," \t" );
    if (strcasecmp(text,"keep-alive" )==0) {
      m_linger=true;
    }
  }else if (strncasecmp(text,"Content-length",15 )==0) {
    // 处理Content-length头部字段
    text+=15;
    text+=strspn(text," \t" );
    m_content_length = atol(text);
    
  }else if (strncasecmp(text,"Host:" ,5 )==0) {
    // 处理Host头部字段
    text+=5;
    m_host= text;
    
  }else {
    printf("oop! unknow header: %s",text);
  }
   return NO_REQUEST; }

// 解析请求体，并没有真正解析HTTP请求体，只是判断它是否被完整的读取了
http_conn::HTTP_CODE http_conn::parse_content(char *text) { return NO_REQUEST; }

// 解析一行，判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line() {
  char temp;
  for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
    temp = m_read_buf[m_checked_idx];
    if (temp == '\r') {
      if (m_checked_idx + 1 == m_read_idx) {
        return LINE_OPEN;
      } else if (m_read_buf[m_checked_idx + 1] == '\n') {
        m_read_buf[m_checked_idx++] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;

    } else if (temp == '\n') {
      if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
        m_read_buf[m_checked_idx - 1] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
    return LINE_OPEN;
  }

  return LINE_OK;
}

http_conn::HTTP_CODE http_conn::do_request() { return NO_REQUEST; }

// 由线程池中的工作线程调用，这是处理HTTP请求那入口函数
void http_conn::process() {
  // 解析HTTP请求
  HTTP_CODE read_ret = process_read();
  if (read_ret == NO_REQUEST) {
    modfd(m_epollfd, m_sockfd, EPOLLIN);
    return;
  }

  // 生成响应
  bool write_ret = process_write(read_ret);
  if (!write_ret) {
    close_conn();
  }
  modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

  bool http_conn::process_write(HTTP_CODE read_code){
    return true;
  }
