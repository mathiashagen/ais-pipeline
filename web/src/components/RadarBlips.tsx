import { divIcon, type DivIcon } from "leaflet";
import { Marker, Popup } from "react-leaflet";
import { navStatusLabel, receivedAt, type PositionedRecord } from "../api/types";
import { bearingOf, categoryOf, colourOf, isStale, type BlipCategory } from "../radar";

const SELECTED_COLOUR = "#f97316";

/** Rotation is snapped to this, which bounds how many distinct icons exist. */
const BEARING_STEP_DEGREES = 5;

const iconCache = new Map<string, DivIcon>();

/**
 * One cached icon per look. react-leaflet replaces a marker's DOM whenever
 * its icon changes identity, so building a fresh icon per render would redo
 * every blip's DOM on every poll. The cache is bounded: 5 categories x 2
 * staleness x 2 selection x (72 bearings + a dot).
 */
function blipIcon(
  category: BlipCategory,
  bearing: number | null,
  stale: boolean,
  selected: boolean,
): DivIcon {
  const snapped =
    bearing === null
      ? null
      : (Math.round(bearing / BEARING_STEP_DEGREES) * BEARING_STEP_DEGREES) % 360;

  const key = `${category}|${snapped ?? "dot"}|${stale}|${selected}`;
  const cached = iconCache.get(key);
  if (cached) return cached;

  const colour = colourOf(category);
  const opacity = stale ? 0.35 : 1;
  const ring = selected
    ? `<circle r="9" fill="none" stroke="${SELECTED_COLOUR}" stroke-width="2" />`
    : "";

  // An arrowhead pointing up (north) at 0 degrees, rotated clockwise -- the
  // same convention as compass bearings, so no conversion is needed.
  const shape =
    snapped === null
      ? `<circle r="2.75" fill="${colour}" fill-opacity="${opacity}" />`
      : `<path d="M0,-6.5 L4.5,5 L0,2.5 L-4.5,5 Z" fill="${colour}" fill-opacity="${opacity}"
               transform="rotate(${snapped})" />`;

  const icon = divIcon({
    className: "radar-blip",
    html: `<svg viewBox="-11 -11 22 22" width="22" height="22">${ring}${shape}</svg>`,
    iconSize: [22, 22],
    iconAnchor: [11, 11],
  });

  iconCache.set(key, icon);
  return icon;
}

function formatValue(value: number | null, digits: number, unit: string): string {
  return value === null ? "—" : `${value.toFixed(digits)}${unit}`;
}

export interface RadarBlipsProps {
  /** Already filtered to ships with a position inside the radar range. */
  records: PositionedRecord[];
  now: number;
  selectedMmsi: number | null;
  onSelect: (mmsi: number) => void;
}

export function RadarBlips({ records, now, selectedMmsi, onSelect }: RadarBlipsProps) {
  return (
    <>
      {records.map((record) => {
        const stale = isStale(record, now);
        const selected = record.mmsi === selectedMmsi;

        return (
          <Marker
            // Keyed by MMSI, not array index: an index key makes React reuse
            // the wrong marker when the ordering shifts between polls, which
            // closes open popups and makes ships appear to teleport.
            key={record.mmsi}
            position={[record.latitude, record.longitude]}
            icon={blipIcon(categoryOf(record), bearingOf(record), stale, selected)}
            // Keep the selected ship above the others where blips overlap.
            zIndexOffset={selected ? 1000 : 0}
            eventHandlers={{ click: () => onSelect(record.mmsi) }}
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
          </Marker>
        );
      })}
    </>
  );
}
