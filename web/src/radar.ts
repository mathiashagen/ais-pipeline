import type { BoundingBox } from "./api/client";
import { ageInSeconds, type PositionedRecord, type PositionRecord } from "./api/types";

/** Where the radar sits. Every range, ring and filter is measured from here. */
export interface RadarCentre {
  lat: number;
  lon: number;
  /** A place name, or null for a point picked on the map or from GPS. */
  name: string | null;
}

export const DEFAULT_CENTRE: RadarCentre = { lat: 62.4722, lon: 6.1495, name: "Ålesund" };

/**
 * The selectable ranges, like the range knob on a radar: the outer ring's
 * distance. Each divides evenly into RING_COUNT, so ring spacings are round
 * numbers too (0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 10, 12, 15 nm).
 */
export const RADAR_RANGES_NM = [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 40, 48, 60] as const;

export const DEFAULT_RANGE_NM = 60;

/** Rings drawn at every range, evenly spaced out to the range. */
export const RING_COUNT = 4;

/**
 * How much of the available radius -- half the shorter side of the view --
 * the outer ring fills. Not 1: the ring labels sit just outside the ring and
 * the "N" above it, and would be cut off at the edge.
 */
export const RING_FILL = 0.9;

/** Must match TILE_DEGREES in scripts/build-coastline.mjs. */
export const COASTLINE_TILE_DEGREES = 1;

export const METRES_PER_NM = 1852;

/**
 * GET /positions returns the newest row per MMSI with no upper bound on age,
 * so a ship that stopped transmitting hours ago is still in the response.
 */
export const STALE_AFTER_SECONDS = 300;

/** Below this speed over ground a ship is not making way, and its course is noise. */
export const MOVING_KNOTS = 0.5;

const EQUATOR_METRES = 40_075_016.686;

function cosDegrees(degrees: number): number {
  return Math.cos((degrees * Math.PI) / 180);
}

/**
 * The zoom level at which a range's outer ring fills RING_FILL of the
 * available radius, exactly.
 *
 * Web Mercator: the world is 256 * 2^zoom pixels wide at the equator, and one
 * pixel covers cos(latitude) of that ground distance further north. So a
 * pixel covers EQUATOR * cos(lat) / 2^(zoom + 8) metres; solve for zoom. The
 * answer is fractional, which is why the map runs with zoomSnap 0 -- snapping
 * to quarter levels would put the ring up to 19% off.
 */
export function zoomForRange(rangeNm: number, availableRadiusPx: number, lat: number): number {
  const metresPerPixel = (rangeNm * METRES_PER_NM) / (RING_FILL * availableRadiusPx);
  return Math.log2((EQUATOR_METRES * cosDegrees(lat)) / metresPerPixel) - 8;
}

/** The next range in or out from `rangeNm`, clamped at either end. */
export function stepRange(rangeNm: number, direction: "in" | "out"): number {
  const index = RADAR_RANGES_NM.indexOf(rangeNm as (typeof RADAR_RANGES_NM)[number]);
  const from = index === -1 ? RADAR_RANGES_NM.indexOf(DEFAULT_RANGE_NM) : index;
  const to = Math.min(RADAR_RANGES_NM.length - 1, Math.max(0, from + (direction === "in" ? -1 : 1)));
  return RADAR_RANGES_NM[to];
}

export function isRadarRange(value: number): boolean {
  return (RADAR_RANGES_NM as readonly number[]).includes(value);
}

/**
 * Distance from the centre in nautical miles.
 *
 * Flat-earth approximation: one minute of latitude is one nautical mile, and
 * a minute of longitude is cos(latitude) of that -- about 0.46 at Ålesund,
 * 0.34 at Hammerfest. Leaving the cosine out would put every ship two to
 * three times as far east or west as it really is. Taking it at the midpoint
 * latitude keeps the error small even that far north, where the cosine
 * changes quickly over the 2 degrees of latitude a 60 nm range spans.
 */
export function distanceNm(centre: RadarCentre, [lat, lon]: [number, number]): number {
  const dy = (lat - centre.lat) * 60;
  const dx = (lon - centre.lon) * 60 * cosDegrees((lat + centre.lat) / 2);
  return Math.hypot(dx, dy);
}

/** Whether a ship is inside the outer ring. */
export function inRange(centre: RadarCentre, rangeNm: number, record: PositionedRecord): boolean {
  return distanceNm(centre, [record.latitude, record.longitude]) <= rangeNm;
}

/**
 * The smallest lat/lon box that contains the outer ring, for asking the API
 * for only the ships that can be inside it. The API filters by box, not by
 * circle, so the box's corners -- about a fifth of its area -- still come
 * back and are dropped by inRange.
 *
 * Its east-west half-width uses the cosine at the ring's poleward edge rather
 * than at the centre. A degree of longitude shrinks towards the pole, so the
 * ring's widest point in degrees lies slightly poleward of the centre, and a
 * box sized with the centre's cosine clips a sliver off each side. Every
 * point of the ring is at most rangeNm east or west of the centre, at a
 * latitude no further poleward than the edge, so the edge's cosine is a safe
 * upper bound -- a little wide, never short.
 */
export function boundingBoxAround(centre: RadarCentre, rangeNm: number): BoundingBox {
  const dLat = rangeNm / 60;
  const poleward = Math.min(89, Math.abs(centre.lat) + dLat);
  const dLon = rangeNm / (60 * cosDegrees(poleward));

  return {
    minLat: centre.lat - dLat,
    maxLat: centre.lat + dLat,
    minLon: centre.lon - dLon,
    maxLon: centre.lon + dLon,
  };
}

/** A point `nm` due north/east of the centre, for placing rings and labels. */
export function offsetFrom(centre: RadarCentre, northNm: number, eastNm: number): [number, number] {
  return [centre.lat + northNm / 60, centre.lon + eastNm / (60 * cosDegrees(centre.lat))];
}

export interface ViewBox {
  south: number;
  west: number;
  north: number;
  east: number;
}

/** Ids ("lat_lon" of the south-west corner) of the coastline tiles a box overlaps. */
export function coastlineTilesIn(box: ViewBox): string[] {
  const step = COASTLINE_TILE_DEGREES;
  const ids: string[] = [];

  for (let lat = Math.floor(box.south / step) * step; lat < box.north; lat += step) {
    for (let lon = Math.floor(box.west / step) * step; lon < box.east; lon += step) {
      ids.push(`${lat}_${lon}`);
    }
  }
  return ids;
}

export function formatCentre(centre: RadarCentre): string {
  if (centre.name) return centre.name;
  const lat = `${Math.abs(centre.lat).toFixed(3)}° ${centre.lat >= 0 ? "N" : "S"}`;
  const lon = `${Math.abs(centre.lon).toFixed(3)}° ${centre.lon >= 0 ? "E" : "W"}`;
  return `${lat}, ${lon}`;
}

export type BlipCategory =
  | "passenger"
  | "cargo"
  | "tanker"
  | "fishing"
  | "service"
  | "leisure"
  | "other"
  | "unknown";

export const BLIP_CATEGORIES: { category: BlipCategory; label: string; colour: string }[] = [
  { category: "passenger", label: "passenger", colour: "#3dd6b0" },
  { category: "cargo", label: "cargo", colour: "#7f8fe0" },
  { category: "tanker", label: "tanker", colour: "#e06464" },
  { category: "fishing", label: "fishing", colour: "#e07fb0" },
  { category: "service", label: "tug / pilot / SAR", colour: "#e8b04b" },
  { category: "leisure", label: "sailing / pleasure", colour: "#b5d96a" },
  { category: "other", label: "other", colour: "#c6c1ad" },
  { category: "unknown", label: "type unknown", colour: "#8a8f87" },
];

const COLOUR_BY_CATEGORY = Object.fromEntries(
  BLIP_CATEGORIES.map(({ category, colour }) => [category, colour]),
) as Record<BlipCategory, string>;

/**
 * Colours by AIS ship type, from message type 5. Navigational status is in
 * the popup instead. Ships the pipeline has no type 5 for yet -- every Class
 * B transponder, and any ship until its first one arrives, up to 6 minutes --
 * are "unknown" rather than guessed at, and so are ships that send code 0.
 *
 * High-speed craft count as passenger: along this coast they are almost all
 * fast ferries.
 */
export function categoryOf(record: PositionRecord): BlipCategory {
  const code = record.ship_type;
  if (code === null || code === 0) return "unknown";

  if (code === 30) return "fishing";
  if (code === 36 || code === 37) return "leisure";
  if ((code >= 31 && code <= 34) || (code >= 50 && code <= 59)) return "service";
  if (code >= 40 && code <= 49) return "passenger";
  if (code >= 60 && code <= 69) return "passenger";
  if (code >= 70 && code <= 79) return "cargo";
  if (code >= 80 && code <= 89) return "tanker";
  return "other";
}

export function colourOf(category: BlipCategory): string {
  return COLOUR_BY_CATEGORY[category];
}

export function isStale(record: PositionRecord, now: number): boolean {
  return ageInSeconds(record, now) > STALE_AFTER_SECONDS;
}

/**
 * The direction to point the blip, or null to draw a dot.
 *
 * Heading is where the bow points, course is where the ship is going; they
 * differ in wind and current, and heading is what a radar operator expects.
 * Course is the fallback when no heading is sent. A ship not making way gets
 * no direction at all: its course over ground is GPS jitter, and an arrow
 * spinning at a quay is a lie.
 */
export function bearingOf(record: PositionRecord): number | null {
  if ((record.sog ?? 0) < MOVING_KNOTS) return null;
  return record.true_heading ?? record.cog;
}
