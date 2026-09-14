#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ais {

template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(std::size_t capacity);

    void push(T item);
    std::optional<T> pop();

    // Waits like pop() for at least one item, then takes up to `max` of what
    // is queued without waiting for more. Returns an empty vector only when
    // the queue is closed and drained. `max` must be at least 1.
    std::vector<T> pop_batch(std::size_t max);

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
std::vector<T> ThreadSafeQueue<T>::pop_batch(std::size_t max) {
    if (max == 0) {
        // An empty result is how a closed, drained queue is reported, so a
        // batch of nothing would be indistinguishable from shutdown.
        throw std::invalid_argument("pop_batch max must be at least 1");
    }

    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this]() { return !queue_.empty() || closed_; });

    // Take what is there now and return. Not waiting to fill the batch is
    // what keeps latency unchanged: at a trickle each batch is one item, and
    // only under a burst do batches grow -- without any timer.
    std::vector<T> batch;
    batch.reserve(std::min(max, queue_.size()));
    while (!queue_.empty() && batch.size() < max) {
        batch.push_back(std::move(queue_.front()));
        queue_.pop();
    }

    if (!batch.empty()) {
        // Several slots may just have freed up, and more than one producer
        // may be waiting for one; notify_one would leave the rest asleep.
        not_full_.notify_all();
    }
    // Returned by value: the vector is constructed in place in the caller or
    // moved, never copied element by element.
    return batch;
}

template <typename T>
void ThreadSafeQueue<T>::close() {
    std::unique_lock<std::mutex> lock(mutex_);
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

} // namespace ais