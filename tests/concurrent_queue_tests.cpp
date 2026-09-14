#include <gtest/gtest.h>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>

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

TEST(ThreadSafeQueue, PopBatchReturnsAtMostMaxInOrder) {
    ais::ThreadSafeQueue<int> queue(10);
    for (int i = 1; i <= 5; ++i) queue.push(i);

    EXPECT_EQ(queue.pop_batch(3), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(queue.pop_batch(10), (std::vector<int>{4, 5}));
}

// The batch adapts to load: it takes what is already queued and returns,
// rather than holding items back until it has `max` of them.
TEST(ThreadSafeQueue, PopBatchDoesNotWaitToFillTheBatch) {
    ais::ThreadSafeQueue<int> queue(10);
    queue.push(7);

    std::promise<std::vector<int>> promise;
    std::future<std::vector<int>> future = promise.get_future();
    std::jthread popper([&queue, &promise] { promise.set_value(queue.pop_batch(100)); });

    ASSERT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
    EXPECT_EQ(future.get(), (std::vector<int>{7}));
}

TEST(ThreadSafeQueue, PopBatchBlocksUntilPushed) {
    ais::ThreadSafeQueue<int> queue(10);

    std::promise<std::vector<int>> promise;
    std::future<std::vector<int>> future = promise.get_future();
    std::jthread popper([&queue, &promise] { promise.set_value(queue.pop_batch(100)); });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    queue.push(42);

    ASSERT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
    EXPECT_EQ(future.get(), (std::vector<int>{42}));
}

// Closing must not drop what is still queued: the writer relies on this to
// commit the last reports on shutdown. Empty means closed *and* drained.
TEST(ThreadSafeQueue, PopBatchDrainsRemainingItemsAfterClose) {
    ais::ThreadSafeQueue<int> queue(10);
    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.close();

    EXPECT_EQ(queue.pop_batch(2), (std::vector<int>{1, 2}));
    EXPECT_EQ(queue.pop_batch(2), (std::vector<int>{3}));
    EXPECT_TRUE(queue.pop_batch(2).empty());
}

// Taking several items frees several slots, so every blocked producer must be
// woken. With notify_one, the second pusher would sleep on despite the room.
TEST(ThreadSafeQueue, PopBatchWakesAllBlockedPushers) {
    ais::ThreadSafeQueue<int> queue(2);
    queue.push(1);
    queue.push(2);

    std::promise<void> first_done;
    std::promise<void> second_done;
    auto first = first_done.get_future();
    auto second = second_done.get_future();
    std::jthread first_pusher([&queue, &first_done] { queue.push(3); first_done.set_value(); });
    std::jthread second_pusher([&queue, &second_done] { queue.push(4); second_done.set_value(); });

    EXPECT_EQ(first.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    EXPECT_EQ(queue.pop_batch(2).size(), 2u);

    EXPECT_EQ(first.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    EXPECT_EQ(second.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
}

TEST(ThreadSafeQueue, PopBatchWithMoveOnlyType) {
    ais::ThreadSafeQueue<std::unique_ptr<int>> queue(2);
    queue.push(std::make_unique<int>(1));
    queue.push(std::make_unique<int>(2));

    auto batch = queue.pop_batch(2);
    ASSERT_EQ(batch.size(), 2u);
    EXPECT_EQ(*batch[0], 1);
    EXPECT_EQ(*batch[1], 2);
}

// A batch of zero could not be told apart from "closed and drained".
TEST(ThreadSafeQueue, PopBatchRejectsZeroMax) {
    ais::ThreadSafeQueue<int> queue(2);
    EXPECT_THROW(queue.pop_batch(0), std::invalid_argument);
}