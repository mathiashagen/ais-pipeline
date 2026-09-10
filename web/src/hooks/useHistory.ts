import { useEffect, useState } from "react";
import { ApiError, fetchHistory } from "../api/client";
import type { PositionRecord } from "../api/types";

export interface HistoryState {
  records: PositionRecord[];
  error: string | null;
  loading: boolean;
}

/** What the last completed fetch produced, and which ship it belongs to. */
interface HistoryData {
  mmsi: number;
  records: PositionRecord[];
  error: string | null;
}

/**
 * Polls GET /positions/{mmsi}/history for one ship, or sits idle when nothing
 * is selected. Same setTimeout chain as usePositions, so a slow API cannot
 * stack overlapping requests, and re-polling means the track grows while the
 * ship is being watched.
 */
export function useHistory(
  mmsi: number | null,
  limit = 200,
  intervalMs = 5000,
): HistoryState {
  const [data, setData] = useState<HistoryData | null>(null);

  useEffect(() => {
    if (mmsi === null) return;

    // Bound outside the closure so the narrowing above survives into it.
    const selected = mmsi;

    const controller = new AbortController();
    let cancelled = false;
    let timer: number | undefined;

    async function poll() {
      try {
        const records = await fetchHistory(selected, limit, controller.signal);
        if (cancelled) return;

        setData({ mmsi: selected, records, error: null });
      } catch (err: unknown) {
        if (cancelled) return;

        // Keep whatever track is already drawn: a failed poll should report
        // itself without erasing the positions it previously fetched.
        setData((previous) => ({
          mmsi: selected,
          records: previous?.mmsi === selected ? previous.records : [],
          error:
            err instanceof ApiError
              ? `API returned ${err.status}: ${err.body}`
              : `Could not load track: ${String(err)}`,
        }));
      } finally {
        if (!cancelled) {
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
  }, [mmsi, limit, intervalMs]);

  // Derived during render rather than reset from the effect. State left over
  // from a previous selection does not belong to this one, so switching ships
  // reads as "loading" instead of briefly drawing the old ship's track.
  const current = data !== null && data.mmsi === mmsi ? data : null;

  return {
    records: current?.records ?? [],
    error: current?.error ?? null,
    loading: mmsi !== null && current === null,
  };
}
