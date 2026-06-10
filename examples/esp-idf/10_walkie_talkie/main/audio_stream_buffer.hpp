/*
 * Audio stream buffer management class
 * Provide a type-safe and user-friendly circular buffer implementation
 */
 
#pragma once
 
#include <cstring>
#include <algorithm>
#include <cassert>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
 
/**
 * @brief Circular buffer template class
 * @tparam T Buffer data type
 * @tparam MAX_SIZE Maximum buffer size
 */
template<typename T, size_t MAX_SIZE>
class RingBuffer {
public:
    RingBuffer() : head_(0), tail_(0), size_(0) {}
    
    /**
     * @brief Add data to the buffer if space is available
     * @param data Data pointer to add
     * @param len Data length to add
     * @return Actual data length added
     */
    size_t push(const T *data, size_t len) {
        if (!data || len == 0) return 0;
        
        size_t available = MAX_SIZE - size_;
        size_t to_write = std::min(len, available);
        
        if (to_write == 0) return 0;  // Buffer is full
        
        // Write data to the buffer
        size_t write_end = (tail_ + to_write) % MAX_SIZE;
        
        if (write_end > tail_) {
            // Single write
            std::memcpy(&buffer_[tail_], data, to_write * sizeof(T));
        } else {
            // Two-part write
            size_t first_part = MAX_SIZE - tail_;
            std::memcpy(&buffer_[tail_], data, first_part * sizeof(T));
            std::memcpy(&buffer_[0], data + first_part, (to_write - first_part) * sizeof(T));
        }
        
        tail_ = write_end;
        size_ += to_write;
        
        return to_write;
    }
    
    /**
     * @brief Read data from the buffer
     * @param data Data pointer to read
     * @param len Data length to read
     * @return Actual data length read
     */
    size_t pop(T *data, size_t len) {
        if (!data || len == 0 || size_ == 0) return 0;
        
        size_t to_read = std::min(len, size_);
        
        if (to_read == 0) return 0;
        
        // Read data from the buffer
        size_t read_end = (head_ + to_read) % MAX_SIZE;
        
        if (read_end > head_) {
            // Single read
            std::memcpy(data, &buffer_[head_], to_read * sizeof(T));
        } else {
            // Two-part read
            size_t first_part = MAX_SIZE - head_;
            std::memcpy(data, &buffer_[head_], first_part * sizeof(T));
            std::memcpy(data + first_part, &buffer_[0], (to_read - first_part) * sizeof(T));
        }
        
        head_ = read_end;
        size_ -= to_read;
        
        return to_read;
    }
    
    /**
     * @brief Peek at data in the buffer without removing it
     * @param data Data pointer to peek
     * @param len Data length to peek
     * @return Actual data length peeked
     */
    size_t peek(T *data, size_t len) const {
        if (!data || len == 0 || size_ == 0) return 0;
        
        size_t to_read = std::min(len, size_);
        size_t read_end = (head_ + to_read) % MAX_SIZE;
        
        if (read_end > head_) {
            std::memcpy(data, &buffer_[head_], to_read * sizeof(T));
        } else {
            size_t first_part = MAX_SIZE - head_;
            std::memcpy(data, &buffer_[head_], first_part * sizeof(T));
            std::memcpy(data + first_part, &buffer_[0], (to_read - first_part) * sizeof(T));
        }
        
        return to_read;
    }
    
    /**
     * @brief Clear the buffer
     */
    inline void clear() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }
    
    /**
     * @brief Get current data size in the buffer
     */
    inline size_t size() const {
        return size_;
    }
    
    /**
     * @brief Get available space in the buffer
     */
    inline size_t available() const {
        return MAX_SIZE - size_;
    }
    
    /**
     * @brief Check if the buffer is empty
     */
    inline bool empty() const {
        return size_ == 0;
    }
    
    /**
     * @brief Check if the buffer is full
     */
    inline bool full() const {
        return size_ == MAX_SIZE;
    }
 
private:
    T buffer_[MAX_SIZE];
    size_t head_;
    size_t tail_;
    size_t size_;
};
 
/**
 * @brief Linear buffer template class
 * Simple and predictable for small data volumes
 */
template<typename T>
class DynamicBuffer {
public:
    explicit DynamicBuffer(size_t initial_capacity = 1024)
        : capacity_(initial_capacity), size_(0) {
        buffer_ = new T[capacity_];
        mutex_ = xSemaphoreCreateMutex();
    }
    
    ~DynamicBuffer() {
        if (mutex_) {
            vSemaphoreDelete(mutex_);
        }
        delete[] buffer_;
    }
    
    /**
     * @brief Add data to the buffer (auto-expand)  
     */
    void push(const T *data, size_t len) {
        if (!data || len == 0) return;

        lock();
        
        // Expand buffer if necessary
        if (size_ + len > capacity_) {
            while (size_ + len > capacity_) {
                capacity_ *= 2;
            }

            T *new_buffer = new T[capacity_];
            std::memcpy(new_buffer, buffer_, size_ * sizeof(T));
            delete[] buffer_;
            buffer_ = new_buffer;
        }
        
        std::memcpy(buffer_ + size_, data, len * sizeof(T));
        size_ += len;

        unlock();
    }

    bool pop(T *data, size_t len) {
        if (!data || len == 0) return false;

        lock();

        if (size_ < len) {
            unlock();
            return false;
        }

        std::memcpy(data, buffer_, len * sizeof(T));
        if (len >= size_) {
            size_ = 0;
        } else {
            std::memmove(buffer_, buffer_ + len, (size_ - len) * sizeof(T));
            size_ -= len;
        }

        unlock();
        return true;
    }
    
    /**
     * @brief Pop data from the buffer (remove from front)
     * @param len Data length to pop
         */
    void pop_front(size_t len) {
        lock();

        if (len >= size_) {
            size_ = 0;
            unlock();
            return;
        }
        
        std::memmove(buffer_, buffer_ + len, (size_ - len) * sizeof(T));
        size_ -= len;

        unlock();
    }
    
    /**
     * @brief Get data pointer (read-only)
     */
    const T *data() const {
        return buffer_;
    }
    
    /**
     * @brief Get data pointer (writable)
     */
    T *data() {
        return buffer_;
    }
    
    /**
     * @brief Clear the buffer
     */
    void clear() {
        lock();
        size_ = 0;
        unlock();
    }
    
    /**
     * @brief Get current size
     */
    size_t size() const {
        lock();
        size_t current_size = size_;
        unlock();
        return current_size;
    }
    
    /**
     * @brief Get buffer capacity
     */
    size_t capacity() const {
        lock();
        size_t current_capacity = capacity_;
        unlock();
        return current_capacity;
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

    T *buffer_;
    size_t capacity_;
    size_t size_;
    SemaphoreHandle_t mutex_;
};
 
/**
 * @brief Pre-allocated fixed-size buffer
 * Simple and best performance for deep buffering scenarios
 */
template<typename T, size_t MAX_SIZE>
class StaticBuffer {
public:
    StaticBuffer() : size_(0) {}
    
    /**
     * @brief Add data to the buffer
     * @param data Data pointer to add
     * @param len Data length to add
     * @return Actual data length added
     */
    size_t push(const T *data, size_t len) {
        if (!data || len == 0 || size_ == MAX_SIZE) return 0;
        
        size_t to_add = std::min(len, MAX_SIZE - size_);
        std::memcpy(buffer_ + size_, data, to_add * sizeof(T));
        size_ += to_add;
        return to_add;
    }
    
    /**
     * @brief Pop data from the buffer (remove from front)
     * @param len Data length to pop
     */
    void pop_front(size_t len) {
        if (len >= size_) {
            clear();
            return;
        }
        
        std::memmove(buffer_, buffer_ + len, (size_ - len) * sizeof(T));
        size_ -= len;
    }
    
    /**
     * @brief Get data pointer (read-only)
     */
    const T *data() const {
        return buffer_;
    }
    
    /**
     * @brief Get data pointer (writable)
     */
    T *data() {
        return buffer_;
    }
    
    /**
     * @brief Clear the buffer
     */
    void clear() {
        size_ = 0;
    }
    
    /**
     * @brief Get current data size in the buffer
     */
    size_t size() const {
        return size_;
    }
    
    /**
     * @brief Check if the buffer is empty
     */
    bool empty() const {
        return size_ == 0;
    }
 
private:
    T buffer_[MAX_SIZE];
    size_t size_;
};
