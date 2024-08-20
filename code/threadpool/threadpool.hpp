#pragma once

// 线程池
#include "../lock/locker.hpp"
#include <cstdio>
#include <exception>
#include <list>
#include <pthread.h>

template <typename T> class threadpool {
public:
  threadpool(int thread_number = 16, int max_requests = 10000);

  ~threadpool();

  bool append(T *request);

private:
  static void *worker(void *arg);
  void run();

private:
  int m_thread_number;        // 线程池中的线程数
  int m_max_requests;         // 请求队列允许的最大请求数
  pthread_t *m_threads;       // 线程池数组，大小为m_thread_number
  std::list<T *> m_wordqueue; // 请求队列
  locker m_quequelocker;      // 互斥锁
  sem m_queuestat;            // 信号量
  bool m_stop;                // 是否结束线程
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests)
    : m_thread_number(thread_number), m_max_requests(max_requests),
      m_stop(false), m_threads(nullptr) {
  if (thread_number <= 0 || max_requests <= 0) {
    throw std::exception();
  }

  m_threads = new pthread_t[m_thread_number];
  if (!m_threads) {
    throw std::exception();
  }

  // 创建m_thread_number个线程，并设置分离
  for (int i = 0; i < m_thread_number; ++i) {
    printf("create the %dth thread\n", i);
    if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
      delete[] m_threads;
      throw std::exception();
    }
    if (pthread_detach(m_threads[i])) {
      delete[] m_threads;
      throw std::exception();
    }
  }
}

template <typename T> threadpool<T>::~threadpool() {
  delete[] m_threads;
  m_stop = true;
}

template <typename T> bool threadpool<T>::append(T *request) {
  m_quequelocker.lock();
  if (m_wordqueue.size() > m_max_requests) {
    m_quequelocker.unlock();
    return false;
  }

  m_wordqueue.emplace_back(request);
  m_quequelocker.unlock();
  m_queuestat.post();
  return true;
}

template <typename T> void *threadpool<T>::worker(void *arg) {
  threadpool *pool = (threadpool *)arg;
  pool->run();
  return pool;
}

template <typename T> void threadpool<T>::run() {
  while (!m_stop) {
    m_queuestat.wait();
    m_quequelocker.lock();
    if (m_wordqueue.empty()) {
      m_quequelocker.unlock();
      continue;
    }

    T *request = m_wordqueue.front();
    m_wordqueue.pop_front();
    m_quequelocker.unlock();
    if (!request) {
      continue;
    }

    request->process();
  }
}
