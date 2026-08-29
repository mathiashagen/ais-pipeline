#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <chrono>

#include "ais/concurrent_queue.hpp"

TEST(ThreadSafeQueue, PushAndPop) {
    ais::ThreadSafeQueue<int> queue(2);

    queue.push(1);
    queue.push(2);

    auto item1 = queue.pop();
    ASSERT_TRUE(item1.has_value());
    EXPECT_EQ(*item1, 1);

    auto item2 = queue.pop();
    ASSERT_TRUE(item2.has_value());
    EXPECT_EQ(*item2, 2);
}

TEST(ThreadSafeQueue, PopFromClosedEmptyQueue) {
    ais::ThreadSafeQueue<int> queue(2);
    queue.close();
    auto item = queue.pop();
    EXPECT_FALSE(item.has_value());
}

TEST(ThreadSafeQueue, PopBlocksUntilPushed) {
    ais::ThreadSafeQueue<int> queue(2);

    std::promise<std::optional<int>> promise;
    std::future<std::optional<int>> future = promise.get_future();

    std::jthread popper([&queue, &promise] {
        promise.set_value(queue.pop());
    });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

    queue.push(42);

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(ThreadSafeQueue, CloseUnblocksWaitingPop) {
    ais::ThreadSafeQueue<int> queue(2);

    std::promise<std::optional<int>> promise;
    std::future<std::optional<int>> future = promise.get_future();

    std::jthread popper([&queue, &promise] {
        promise.set_value(queue.pop());
    });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    queue.close();

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
    auto result = future.get();
    EXPECT_FALSE(result.has_value());
}

TEST(ThreadSafeQueue, PushBlocksUntilPopped) {
    ais::ThreadSafeQueue<int> queue(2);

    queue.push(1);
    queue.push(2);

    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    std::jthread pusher([&queue, &promise] {
        queue.push(3);
        promise.set_value();
    });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

    auto item = queue.pop();

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 1);
}

TEST(ThreadSafeQueue, PushAndPopMoveOnlyType) {
    ais::ThreadSafeQueue<std::unique_ptr<int>> queue(2);

    queue.push(std::make_unique<int>(42));

    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(**item, 42);
}