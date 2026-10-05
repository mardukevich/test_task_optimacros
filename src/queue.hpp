#pragma once

#include <condition_variable>
#include <queue>

namespace mtfind {

// Bounded thread-safe blocking queue (producer-consumer).
template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t c) : capacity(c) {}

    // Blocks while the queue is full.
    void push(T item) {
        std::unique_lock lock(mutex);
        notFull.wait(lock, [this] { return queue.size() < capacity; });
        queue.push(std::move(item));
        notEmpty.notify_one();
    }

    // Blocks while the queue is empty.
    T pop() {
        std::unique_lock lock(mutex);
        notEmpty.wait(lock, [this] { return !queue.empty(); });
        T item = std::move(queue.front());
        queue.pop();
        notFull.notify_one();
        return item;
    }

private:
    std::size_t capacity = 0;
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable notFull;
    std::condition_variable notEmpty;
};

}  // namespace mtfind