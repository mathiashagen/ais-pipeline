import { useEffect, useState } from "react";

/**
 * Wall clock as state, re-rendering on an interval.
 *
 * Reading Date.now() during render is impure: the value changes between
 * renders for reasons React cannot see, so anything derived from it goes
 * stale silently. Holding it in state also means that if polling stops, ages
 * keep advancing and stale ships grey out, rather than freezing at whatever
 * age they had when the API went away.
 */
export function useNow(intervalMs = 10_000): number {
  const [now, setNow] = useState(() => Date.now());

  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), intervalMs);
    return () => window.clearInterval(timer);
  }, [intervalMs]);

  return now;
}
