#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace ais {

template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(std::size_t capacity);

    void push(T item);
    std::optional<T> pop();
    void close();

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    const std::size_t capacity_;
    bool closed_ = false;
};

template <typename T>
ThreadSafeQueue<T>::ThreadSafeQueue(std::size_t capacity) : capacity_(capacity) {}

template <typename T>
void ThreadSafeQueue<T>::push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(lock, [this]() { return queue_.size() < capacity_ || closed_; });

    if (closed_) {
        throw std::runtime_error("ThreadSafeQueue is closed");
    }

    queue_.push(std::move(item));
    not_empty_.notify_one();
}

template <typename T>
std::optional<T> ThreadSafeQueue<T>::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this]() { return !queue_.empty() || closed_; });

    if (queue_.empty()) {
        return std::nullopt; // Queue is closed and empty
    }

    T item = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    return item;
}

template <typename T>
void ThreadSafeQueue<T>::close() {
    std::unique_lock<std::mutex> lock(mutex_);
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

} // namespace ais