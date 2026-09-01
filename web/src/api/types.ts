/**
 * Mirrors ais::PositionRecord's to_json in
 * src/api/include/ais/position_record.hpp. Keep the two in step.
 */

/**
 * Mirrors ais::NavStatus. Serialized as its underlying integer.
 *
 * A const object rather than a TS `enum`: this template enables
 * `erasableSyntaxOnly`, which bans syntax that emits runtime code.
 */
export const NavStatus = {
  UnderWayEngine: 0,
  AtAnchor: 1,
  NotUnderCommand: 2,
  RestrictedManeuver: 3,
  ConstrainedByDraught: 4,
  Moored: 5,
  Aground: 6,
  Fishing: 7,
  UnderWaySailing: 8,
  ReservedHSC: 9,
  ReservedWIG: 10,
  Reserved11: 11,
  Reserved12: 12,
  Reserved13: 13,
  AisSartMobEpirb: 14,
  NotDefined: 15,
} as const;

export type NavStatus = (typeof NavStatus)[keyof typeof NavStatus];

export const NAV_STATUS_LABELS: Record<NavStatus, string> = {
  [NavStatus.UnderWayEngine]: "Under way using engine",
  [NavStatus.AtAnchor]: "At anchor",
  [NavStatus.NotUnderCommand]: "Not under command",
  [NavStatus.RestrictedManeuver]: "Restricted manoeuvrability",
  [NavStatus.ConstrainedByDraught]: "Constrained by draught",
  [NavStatus.Moored]: "Moored",
  [NavStatus.Aground]: "Aground",
  [NavStatus.Fishing]: "Engaged in fishing",
  [NavStatus.UnderWaySailing]: "Under way sailing",
  [NavStatus.ReservedHSC]: "Reserved (HSC)",
  [NavStatus.ReservedWIG]: "Reserved (WIG)",
  [NavStatus.Reserved11]: "Reserved",
  [NavStatus.Reserved12]: "Reserved",
  [NavStatus.Reserved13]: "Reserved",
  [NavStatus.AisSartMobEpirb]: "AIS-SART / MOB / EPIRB",
  [NavStatus.NotDefined]: "Not defined",
};

export function navStatusLabel(status: number): string {
  return NAV_STATUS_LABELS[status as NavStatus] ?? `Unknown (${status})`;
}

export interface PositionRecord {
  mmsi: number;

  /**
   * std::optional<double> on the C++ side, so null whenever the AIS message
   * carried no fix. Leaflet throws on a null LatLng -- filter with hasPosition
   * before rendering.
   */
  latitude: number | null;
  longitude: number | null;

  sog: number | null;
  cog: number | null;
  true_heading: number | null;

  /**
   * NOT a timestamp. This is the AIS second-of-minute field: 0-59, with 60
   * meaning unavailable. For wall-clock time use received_at.
   */
  timestamp: number;

  nav_status: NavStatus;

  /** Wall clock the pipeline stored the row, in epoch SECONDS, not millis. */
  received_at: number;
}

/** A record known to carry a position, so latitude/longitude are non-null. */
export type PositionedRecord = PositionRecord & {
  latitude: number;
  longitude: number;
};

/** Type guard that narrows away the nulls Leaflet cannot handle. */
export function hasPosition(record: PositionRecord): record is PositionedRecord {
  return record.latitude !== null && record.longitude !== null;
}

/** received_at is in seconds; JS Date wants milliseconds. */
export function receivedAt(record: PositionRecord): Date {
  return new Date(record.received_at * 1000);
}

/** Seconds since this record was stored. */
export function ageInSeconds(record: PositionRecord, now: number = Date.now()): number {
  return (now - record.received_at * 1000) / 1000;
}
