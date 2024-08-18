// 线程同步机制封装类

#pragma once

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 互斥锁
class locker {
public:
  locker(){
    if (pthread_mutex_init(&m_mutex,nullptr)!=0) {
 throw std::exception();     
    }
  }
  ~locker(){
    pthread_mutex_destroy(&m_mutex);
  }
  bool lock(){
    return pthread_mutex_lock(&m_mutex);
  }
  bool unlock(){
    return pthread_mutex_unlock(&m_mutex);
  }
private:
  pthread_mutex_t m_mutex;
};

// 条件变量
class cond{
  public:
  cond(){

    if (pthread_cond_init(&m_cond, nullptr) != 0) {
      throw std::exception();
    }
  }
  ~cond(){
    
    pthread_cond_destroy(&m_cond);
  }
  bool wait(pthread_mutex_t *mutex){
    return pthread_cond_wait(&m_cond,mutex)==0;
  }
  timewait(){}
  signal(){}
  broadcast(){}
  private:
    pthread_cond_t m_cond;
};
