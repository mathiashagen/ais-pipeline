#include "ais/reconnect_backoff.hpp"

#include <algorithm>

namespace ais {
std::chrono::milliseconds ReconnectBackoff::after_failure(std::chrono::steady_clock::duration attempt_duration) {
    if (attempt_duration >= healthy_connection) {
        consecutive_failures_ = 0;
    }
    // Capped, so the counter cannot overflow however long the feed is down.
    consecutive_failures_ = std::min(consecutive_failures_ + 1, 1000);

    // Doubling a duration stops once it reaches the maximum, so it never
    // grows past it; no shift or multiplication by a large power of two.
    std::chrono::milliseconds delay = initial_delay;
    for (int i = 0; i < consecutive_failures_ && delay < max_delay; ++i) {
        delay *= 2;
    }
    return std::min(delay, max_delay);
}
}
