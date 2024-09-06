#pragma once

#include <arpa/inet.h>
#include <ctime>
#include <netinet/in.h>

const int BUFFER_SIZE{64};
class util_timer; // 前向声明

// 用户数据结构
struct client_data {
  sockaddr_in address; // 客户端地址
  int sockfd;
  char buf[BUFFER_SIZE]; // 读缓存
  util_timer *timer;     // 定时器
};

// 定时器类
class util_timer {
public:
  util_timer() : prev(nullptr), next(nullptr) {}
  time_t expire;                  // 任务超时时间，使用绝对时间
  void (*cb_func)(client_data *); // 任务回调函数，处理客户数据
  client_data *user_data;
  util_timer *prev;
  util_timer *next;
};

class sort_timer_lst {
public:
  sort_timer_lst() : head(nullptr), tail(nullptr) {}
  ~sort_timer_lst() {
    util_timer *temp = head;
    while (temp) {
      head = temp->next;
      delete temp;
      temp = head;
    }
  }

  void add_timer(util_timer *timer) {
    if (!timer) {
      return;
    }
    if (!head) {
      head = tail = timer;
      return;
    }
    if (timer->expire < head->expire) {
      timer->next = head;
      head->prev = timer;
      head = timer;
      return;
      return;
    }
    add_timer(timer, head);

  }
  void add_timer(util_timer *timer, util_timer *lst_head) {
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while (tmp) {
      if (timer->expire < tmp->expire) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = tmp;
        tmp->prev = timer;
        break;
      }
      prev = tmp;
      tmp = tmp->next;
    }
    if (!tmp) {
      prev->next = timer;
      timer->prev = prev;
      timer->next = nullptr;
      tail = timer;
    }
  }

  void adjust_timer(util_timer *timer) {
    if (!timer) {
      return;
    }
    util_timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire)) {
      return;
    }
    if (timer == head) {
      head = head->next;
      head->prev = nullptr;
      timer->next = nullptr;
      add_timer(timer, head);

    } else {
      timer->prev->next = timer->next;
      timer->next->prev = timer->prev;
      add_timer(timer, tmp);
    }
  }

  void del_timer(util_timer *timer) {
    if (!timer) {
      return;
    }
    if (timer == head && timer == tail) {
      delete timer;
      head = tail = nullptr;
      return;
    }
    if (timer == head) {
      head = head->next;
      head->prev = nullptr;
      delete timer;
      return;
    }
    if (timer == tail) {
      tail = tail->prev;
      tail->prev = nullptr;
      delete timer;
      return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
  }

  void tick() {
    if (!head) {
      return;
    }

    time_t cur = time(nullptr);
    util_timer *tmp = head;
    while (tmp) {
      if (cur < tmp->expire) {
        break;
      }

      tmp->cb_func(tmp->user_data);
      head = tmp->next;
      if (head) {
        head->prev = nullptr;
      }
      delete tmp;
      tmp = head;
    }
  }

public:
  util_timer *head;

  util_timer *tail;
};

