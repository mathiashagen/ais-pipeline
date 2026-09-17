import type { ReactNode } from "react";
import {
  ageInSeconds,
  displayName,
  hasPosition,
  NavStatus,
  navStatusLabel,
  shipTypeLabel,
  type PositionRecord,
} from "../api/types";
import { formatAge, formatLatLon } from "../format";
import type { HistoryState } from "../hooks/useHistory";
import { bearingOf, categoryOf, colourOf, isStale } from "../radar";

export interface ShipPanelProps {
  mmsi: number;
  /** The ship's latest record, or undefined once it has left the radar's range. */
  record: PositionRecord | undefined;
  track: HistoryState;
  now: number;
  onClose: () => void;
}

function Row({ label, children }: { label: string; children: ReactNode }) {
  return (
    <div>
      <dt>{label}</dt>
      <dd>{children}</dd>
    </div>
  );
}

/**
 * Details of the selected ship, docked over the map. It replaces a Leaflet
 * popup, which covered the neighbouring blips and the ship's own track, and
 * the selection line in the header, whose appearance made the header wrap
 * and the whole radar jump down. Fields the ship has not reported are left
 * out rather than shown as dashes.
 */
export function ShipPanel({ mmsi, record, track, now, onClose }: ShipPanelProps) {
  // The heading already shows the MMSI when there is no name.
  const ids = [
    record?.name != null ? `MMSI ${mmsi}` : null,
    record?.call_sign ?? null,
    record?.ship_type ? shipTypeLabel(record.ship_type) : null,
  ].filter((part) => part !== null);

  const trackPoints = track.records.filter(hasPosition).length;

  return (
    <aside className="ship-panel" aria-label="Selected ship">
      <div className="ship-panel-heading">
        {record && (
          <svg viewBox="-6 -6 12 12" width="12" height="12" aria-hidden="true">
            <path d="M0,-5.5 L4,4.5 L0,2 L-4,4.5 Z" fill={colourOf(categoryOf(record))} />
          </svg>
        )}
        <h2>{displayName({ mmsi, name: record?.name ?? null })}</h2>
        <button type="button" className="ship-panel-close" aria-label="Close" onClick={onClose}>
          ×
        </button>
      </div>

      {ids.length > 0 && <p className="ship-panel-ids">{ids.join(" · ")}</p>}

      {!record && <p className="ship-panel-note">No longer within the radar's range.</p>}

      <dl>
        {record?.sog != null && <Row label="Speed">{record.sog.toFixed(1)} kn</Row>}
        {/* Same rule as the blip's arrow: at a standstill the course is GPS noise. */}
        {record && bearingOf(record) !== null && record.cog !== null && (
          <Row label="Course">{record.cog.toFixed(0)}°</Row>
        )}
        {record?.true_heading != null && <Row label="Heading">{record.true_heading}°</Row>}
        {/* Class B reports carry no status and decode as Not defined. */}
        {record && record.nav_status !== NavStatus.NotDefined && (
          <Row label="Status">{navStatusLabel(record.nav_status)}</Row>
        )}
        {record?.destination != null && <Row label="Destination">{record.destination}</Row>}
        {record && hasPosition(record) && (
          <Row label="Position">{formatLatLon(record.latitude, record.longitude, 4)}</Row>
        )}
        {record && (
          <Row label="Seen">
            <span className={isStale(record, now) ? "ship-panel-stale" : undefined}>
              {formatAge(ageInSeconds(record, now))}
            </span>
          </Row>
        )}
        <Row label="Track">
          {track.error ? (
            <span className="ship-panel-error">{track.error}</span>
          ) : track.loading ? (
            "loading…"
          ) : (
            `${trackPoints} point${trackPoints === 1 ? "" : "s"}`
          )}
        </Row>
      </dl>
    </aside>
  );
}
