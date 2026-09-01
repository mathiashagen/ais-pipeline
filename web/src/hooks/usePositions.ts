import { useEffect, useState } from "react";
import { ApiError, fetchLatestPositions } from "../api/client";
import type { PositionRecord } from "../api/types";

export interface PositionsState {
  records: PositionRecord[];
  error: string | null;
  loading: boolean;
  lastUpdated: Date | null;
}

/**
 * Polls GET /positions.
 *
 * Schedules the next request only after the previous one settles, rather than
 * using setInterval: a slow or stalled API would otherwise pile up overlapping
 * requests. A failed poll still schedules the next one, so the map recovers on
 * its own once the API comes back.
 */
export function usePositions(intervalMs = 5000): PositionsState {
  const [records, setRecords] = useState<PositionRecord[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);

  useEffect(() => {
    const controller = new AbortController();
    let cancelled = false;
    let timer: number | undefined;

    async function poll() {
      try {
        const data = await fetchLatestPositions(controller.signal);
        if (cancelled) return;

        setRecords(data);
        setError(null);
        setLastUpdated(new Date());
      } catch (err: unknown) {
        // A superseded request must not touch state at all -- reporting its
        // outcome would overwrite whatever the live request just wrote.
        if (cancelled) return;

        setError(
          err instanceof ApiError
            ? `API returned ${err.status}: ${err.body}`
            : `Could not reach the API: ${String(err)}`,
        );
      } finally {
        if (!cancelled) {
          setLoading(false);
          timer = window.setTimeout(poll, intervalMs);
        }
      }
    }

    void poll();

    return () => {
      cancelled = true;
      controller.abort();
      window.clearTimeout(timer);
    };
  }, [intervalMs]);

  return { records, error, loading, lastUpdated };
}
