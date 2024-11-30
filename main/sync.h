#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory>

template <typename T>
class Mutex {
public:
  class Guard {
  public:
    Guard(T* data, SemaphoreHandle_t mutex)
        : m_data(data),
          m_mutex(mutex) {}
    ~Guard() { xSemaphoreGive(m_mutex); }

    T* operator*() { return m_data; }
    T* operator->() { return m_data; }

    T* get() { return m_data; }

  private:
    T* m_data;
    SemaphoreHandle_t const m_mutex;
  };

  Mutex()
      : m_data(nullptr),
        m_mutex(xSemaphoreCreateMutex()) {}

  Mutex(T data)
      : m_data(std::make_unique<T>(data)),
        m_mutex(xSemaphoreCreateMutex()) {}

  Guard lock(uint32_t ticks_to_wait = portMAX_DELAY) {
    xSemaphoreTake(m_mutex, ticks_to_wait);
    return Guard(m_data.get(), m_mutex);
  }

private:
  std::unique_ptr<T> m_data;
  SemaphoreHandle_t const m_mutex;
};

class Lock {
public:
  class Guard {
  public:
    Guard(SemaphoreHandle_t mutex)
        : m_mutex(mutex) {}
    ~Guard() { xSemaphoreGive(m_mutex); }

  private:
    SemaphoreHandle_t const m_mutex;
  };

  Lock()
      : m_mutex(xSemaphoreCreateMutex()) {}

  Guard lock(uint32_t ticks_to_wait = portMAX_DELAY) {
    xSemaphoreTake(m_mutex, ticks_to_wait);
    return Guard(m_mutex);
  }

private:
  SemaphoreHandle_t m_mutex;
};
