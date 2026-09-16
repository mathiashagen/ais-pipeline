#pragma once
#include <chrono>

namespace ais {

// How long to wait before reconnecting to the feed. The delay doubles with
// each failure in a row, from 2 up to 30 seconds. A connection that stayed up
// for healthy_connection counts as a success, so the next failure starts over
// at 2 seconds; a connection that keeps dropping right away still backs off,
// instead of reconnecting every couple of seconds.
class ReconnectBackoff {
public:
    static constexpr std::chrono::milliseconds initial_delay{1000};
    static constexpr std::chrono::milliseconds max_delay{30000};
    static constexpr std::chrono::seconds healthy_connection{60};

    // Call when a connection attempt ended in an error. `attempt_duration` is
    // how long that attempt ran, connecting included. Returns the delay.
    std::chrono::milliseconds after_failure(std::chrono::steady_clock::duration attempt_duration);

private:
    int consecutive_failures_ = 0;
};
}
