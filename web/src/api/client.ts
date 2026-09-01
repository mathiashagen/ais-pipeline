import type { PositionRecord } from "./types";

const BASE_URL = import.meta.env.VITE_API_URL ?? "http://localhost:8080";

export interface BoundingBox {
  minLat: number;
  maxLat: number;
  minLon: number;
  maxLon: number;
}

/**
 * The C++ server distinguishes 400 (bad request) from 500 (database failure),
 * so surface the status rather than collapsing both into "fetch failed".
 */
export class ApiError extends Error {
  // Declared as fields rather than constructor parameter properties, which
  // `erasableSyntaxOnly` bans.
  readonly status: number;
  readonly body: string;

  constructor(status: number, body: string) {
    super(`${status}: ${body || "(no body)"}`);
    this.name = "ApiError";
    this.status = status;
    this.body = body;
  }
}

async function getJson<T>(path: string, signal?: AbortSignal): Promise<T> {
  const response = await fetch(`${BASE_URL}${path}`, { signal });

  if (!response.ok) {
    // Error bodies are text/plain, not JSON.
    throw new ApiError(response.status, await response.text().catch(() => ""));
  }

  return (await response.json()) as T;
}

/** Latest position per ship. GET /positions */
export function fetchLatestPositions(signal?: AbortSignal): Promise<PositionRecord[]> {
  return getJson<PositionRecord[]>("/positions", signal);
}

/** Latest position per ship within a box. GET /positions/area */
export function fetchPositionsInArea(
  box: BoundingBox,
  signal?: AbortSignal,
): Promise<PositionRecord[]> {
  const params = new URLSearchParams({
    min_lat: String(box.minLat),
    max_lat: String(box.maxLat),
    min_lon: String(box.minLon),
    max_lon: String(box.maxLon),
  });

  return getJson<PositionRecord[]>(`/positions/area?${params}`, signal);
}

/**
 * Track for one ship, newest first. GET /positions/{mmsi}/history
 * The server rejects a limit outside 1..1000 with a 400.
 */
export function fetchHistory(
  mmsi: number,
  limit = 50,
  signal?: AbortSignal,
): Promise<PositionRecord[]> {
  return getJson<PositionRecord[]>(`/positions/${mmsi}/history?limit=${limit}`, signal);
}
