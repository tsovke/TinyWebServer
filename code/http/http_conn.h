#pragma once

#include "../lock/locker.hpp"
#include <arpa/inet.h>
#include <bits/types/struct_iovec.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  static const int FILENAME_LEN = 256;
  static const int READ_BUFFER_SIZE = 2048;
  static const int WRITE_BUFFER_SIZE = 2048;

  // HTTP请求方法，但我们只支持GET
  enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };

  /*
    解析客户端请求时，主状态机的状态
    CHECK_STATE_REQUESTLINE:当前正在分析请求行
    CHECK_STATE_HEADER:当前正在分析头部字段
    CHECK_STATE_CONNTENT:当前正在解析请求体
  */
  enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONNTENT
  };

  /*
    从状态机的三种可能状态，即行的读取状态，分别表示
    1. 读取到一个完整的行 2.行出错 3.行数据尚且不完整
  */
  enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

  /*
    服务器处理HTTP请求的可能结果，报文解析的结果
    NO_REQUEST         : 请求不完整，需要继续读取客户端数据
    GET_REQUEST        : 表示获得了一个完成的客户端请求
    BAD_REQUEST        : 表示客户端请求语法错误
    NO_RESOURCE        : 表示服务端没有资源
    FORBIDDEN_REQUEST  : 表示客户端对资源没有足够的访问权限
    FILE_REQUEST       : 文件请求，获取文件成功
    INTERNAL_ERROR     : 表示服务器内部错误
    CLOSED_CONNECTION  : 表示客户端已经关闭连接
  */
  enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
  };

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
  int m_read_idx; // 标识读缓冲区中以及读入的客户端数据的最后一个字节的下一位置

  char m_write_buf[WRITE_BUFFER_SIZE];
  int m_write_idx; // 标识写缓冲区中以及写入的客户端数据的最后一个字节的下一位置

  int m_checked_idx;    // 当前正在分析的字符在读缓冲区的位置
  int m_start_line;     // 当前正在解析的行的起始位置
  char *m_url;          // 请求目标文件的文件名
  char *m_version;      // 协议版本，只支持HTTP1.1
  METHOD m_method;      // 请求方法
  char *m_host;         // 主机名
  bool m_linger;        // HTTP请求是否保持连接
  int m_content_length; // 请求体的长度

  CHECK_STATE m_check_state; // 主状态机当前所处的状态

  char m_real_file[FILENAME_LEN];
  struct stat m_file_stat;
  char *m_file_address; // 客户请求的目标文件被mmap映射到内存的起始地址中
  struct iovec m_iv[2]; // 创建一个包含两个缓冲区的iovec数组
  int m_iv_count;       // m_iv数组中的元素数量

  int bytes_have_send; // 已经发送的字节
  int bytes_to_send;   // 将要发送的字节

  void init();                                // 初始化连接其余的信息
  HTTP_CODE process_read();                   // 解析HTTP请求
  bool process_write(HTTP_CODE ret);          // 填充HTTP应答
  HTTP_CODE parse_request_line(char *text);   // 解析请求首行
  HTTP_CODE parse_headers(char *text);        // 解析请求行
  HTTP_CODE parse_content(char *text);        // 解析请求体
  void unmap();                               // 取消内存映射
  bool add_response(const char *format, ...); // 添加响应状态
  bool add_status_line(int status, const char *title);
  bool add_headers(int content_len);
  bool add_content_length(int content_len);
  bool add_content_type();
  bool add_linger();
  bool add_blank_line();
  bool add_content(const char *content);

  LINE_STATUS parse_line();
  char *get_line() { return m_read_buf + m_start_line; }
  HTTP_CODE do_request();
};
