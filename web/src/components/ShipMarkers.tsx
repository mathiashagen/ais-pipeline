import { CircleMarker, Popup } from "react-leaflet";
import {
  ageInSeconds,
  hasPosition,
  navStatusLabel,
  receivedAt,
  type PositionRecord,
} from "../api/types";

/**
 * GET /positions returns the newest row per MMSI with no upper bound on age,
 * so a ship that stopped transmitting hours ago is still in the response.
 * Past this age it is drawn greyed out rather than presented as live.
 */
const STALE_AFTER_SECONDS = 300;

const COLOR_STALE = "#9ca3af";
const COLOR_STATIONARY = "#3b82f6";
const COLOR_MOVING = "#22c55e";

/** Below this speed over ground a ship is treated as not making way. */
const MOVING_KNOTS = 0.5;

function colorFor(record: PositionRecord, now: number): string {
  if (ageInSeconds(record, now) > STALE_AFTER_SECONDS) return COLOR_STALE;
  return (record.sog ?? 0) >= MOVING_KNOTS ? COLOR_MOVING : COLOR_STATIONARY;
}

function formatValue(value: number | null, digits: number, unit: string): string {
  return value === null ? "—" : `${value.toFixed(digits)}${unit}`;
}

export function ShipMarkers({ records, now }: { records: PositionRecord[]; now: number }) {
  // Leaflet throws on a null LatLng, and the C++ side models position as
  // std::optional, so these have to go before anything reaches the map.
  const positioned = records.filter(hasPosition);

  return (
    <>
      {positioned.map((record) => {
        const stale = ageInSeconds(record, now) > STALE_AFTER_SECONDS;

        return (
          <CircleMarker
            // Keyed by MMSI, not array index: an index key makes React reuse
            // the wrong marker when the ordering shifts between polls, which
            // closes open popups and makes ships appear to teleport.
            key={record.mmsi}
            center={[record.latitude, record.longitude]}
            radius={5}
            pathOptions={{
              color: colorFor(record, now),
              fillColor: colorFor(record, now),
              fillOpacity: stale ? 0.25 : 0.8,
              weight: 1,
            }}
          >
            <Popup>
              <strong>MMSI {record.mmsi}</strong>
              <br />
              {record.latitude.toFixed(4)}, {record.longitude.toFixed(4)}
              <br />
              Speed: {formatValue(record.sog, 1, " kn")}
              <br />
              Course: {formatValue(record.cog, 1, "°")}
              <br />
              Heading:{" "}
              {record.true_heading === null ? "—" : `${record.true_heading}°`}
              <br />
              Status: {navStatusLabel(record.nav_status)}
              <br />
              Seen: {receivedAt(record).toLocaleTimeString()}
              {stale && " (stale)"}
            </Popup>
          </CircleMarker>
        );
      })}
    </>
  );
}
