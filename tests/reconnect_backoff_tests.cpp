#include "ais/reconnect_backoff.hpp"

#include <gtest/gtest.h>
#include <chrono>

using namespace std::chrono_literals;

TEST(ReconnectBackoff, FirstFailureWaitsTwoSeconds) {
    ais::ReconnectBackoff backoff;
    EXPECT_EQ(backoff.after_failure(1s), 2s);
}

TEST(ReconnectBackoff, DoublesUpToThirtySeconds) {
    ais::ReconnectBackoff backoff;
    EXPECT_EQ(backoff.after_failure(1s), 2s);
    EXPECT_EQ(backoff.after_failure(1s), 4s);
    EXPECT_EQ(backoff.after_failure(1s), 8s);
    EXPECT_EQ(backoff.after_failure(1s), 16s);
    EXPECT_EQ(backoff.after_failure(1s), 30s);
    EXPECT_EQ(backoff.after_failure(1s), 30s);
}

// The old formula, 1000 * (1 << failures), overflowed int at 22 failures.
TEST(ReconnectBackoff, StaysAtMaximumAfterManyFailures) {
    ais::ReconnectBackoff backoff;
    std::chrono::milliseconds delay{};
    for (int i = 0; i < 1000; ++i) {
        delay = backoff.after_failure(0s);
    }
    EXPECT_EQ(delay, 30s);
}

TEST(ReconnectBackoff, ConnectionThatStayedUpResetsTheDelay) {
    ais::ReconnectBackoff backoff;
    for (int i = 0; i < 10; ++i) {
        backoff.after_failure(1s);
    }
    EXPECT_EQ(backoff.after_failure(ais::ReconnectBackoff::healthy_connection), 2s);
    EXPECT_EQ(backoff.after_failure(1s), 4s);
}

TEST(ReconnectBackoff, ShortConnectionDoesNotResetTheDelay) {
    ais::ReconnectBackoff backoff;
    backoff.after_failure(1s);
    backoff.after_failure(1s);
    EXPECT_EQ(backoff.after_failure(ais::ReconnectBackoff::healthy_connection - 1s), 8s);
}
