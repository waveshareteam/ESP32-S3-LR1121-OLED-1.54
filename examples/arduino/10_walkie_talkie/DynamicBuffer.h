/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <new>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

template <typename T>
class DynamicBuffer {
public:
  explicit DynamicBuffer(size_t initialCapacity)
      : capacity_(initialCapacity), size_(0), buffer_(nullptr), mutex_(nullptr) {
    buffer_ = new (std::nothrow) T[capacity_];
    mutex_ = xSemaphoreCreateMutex();
  }

  ~DynamicBuffer() {
    if (mutex_) {
      vSemaphoreDelete(mutex_);
    }
    delete[] buffer_;
  }

  void push(const T *data, size_t len) {
    if (!data || len == 0 || !buffer_) {
      return;
    }
    lock();
    if (size_ + len > capacity_) {
      size_t newCapacity = capacity_;
      while (size_ + len > newCapacity) {
        newCapacity *= 2;
      }
      T *newBuffer = new (std::nothrow) T[newCapacity];
      if (!newBuffer) {
        size_ = 0;
        unlock();
        return;
      }
      memcpy(newBuffer, buffer_, size_ * sizeof(T));
      delete[] buffer_;
      buffer_ = newBuffer;
      capacity_ = newCapacity;
    }
    memcpy(buffer_ + size_, data, len * sizeof(T));
    size_ += len;
    unlock();
  }

  bool pop(T *data, size_t len) {
    if (!data || len == 0 || !buffer_) {
      return false;
    }
    lock();
    if (size_ < len) {
      unlock();
      return false;
    }
    memcpy(data, buffer_, len * sizeof(T));
    if (len >= size_) {
      size_ = 0;
    } else {
      memmove(buffer_, buffer_ + len, (size_ - len) * sizeof(T));
      size_ -= len;
    }
    unlock();
    return true;
  }

  void clear() {
    lock();
    size_ = 0;
    unlock();
  }

  size_t available() const {
    lock();
    size_t value = size_;
    unlock();
    return value;
  }

private:
  void lock() const {
    if (mutex_) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
    }
  }

  void unlock() const {
    if (mutex_) {
      xSemaphoreGive(mutex_);
    }
  }

  size_t capacity_;
  size_t size_;
  T *buffer_;
  mutable SemaphoreHandle_t mutex_;
};
