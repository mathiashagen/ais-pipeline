import { useEffect, useState } from "react";
import { ApiError, fetchPositionsInArea, type BoundingBox } from "../api/client";
import type { PositionRecord } from "../api/types";

export interface PositionsState {
  records: PositionRecord[];
  error: string | null;
  loading: boolean;
  lastUpdated: Date | null;
}

/**
 * Polls GET /positions/area for the ships inside a box.
 *
 * Schedules the next request only after the previous one settles, rather than
 * using setInterval: a slow or stalled API would otherwise pile up overlapping
 * requests. A failed poll still schedules the next one, so the radar recovers
 * on its own once the API comes back.
 *
 * A new box -- the centre moved or the range changed -- aborts the request in
 * flight and polls straight away, instead of waiting out the interval with
 * ships from the old area on screen. Those stay displayed until the new
 * answer lands; the caller filters to the ring anyway, so they are never
 * drawn in the wrong place.
 */
export function usePositions(box: BoundingBox, intervalMs = 5000): PositionsState {
  const [records, setRecords] = useState<PositionRecord[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);

  // A new object with the same corners must not restart polling, so the
  // effect is keyed on the values rather than the object.
  const { minLat, maxLat, minLon, maxLon } = box;

  useEffect(() => {
    const area = { minLat, maxLat, minLon, maxLon };
    const controller = new AbortController();
    let cancelled = false;
    let timer: number | undefined;

    async function poll() {
      try {
        const data = await fetchPositionsInArea(area, controller.signal);
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
  }, [minLat, maxLat, minLon, maxLon, intervalMs]);

  return { records, error, loading, lastUpdated };
}
