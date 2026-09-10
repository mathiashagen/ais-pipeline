import { CircleMarker, Polyline } from "react-leaflet";
import { hasPosition, receivedAt, type PositionRecord } from "../api/types";

const TRACK_COLOR = "#f97316";

/**
 * The recorded track of one ship, drawn from its stored position reports
 * rather than extrapolated from speed -- every row the pipeline decoded is
 * kept, so this is where the ship actually went, turns included.
 */
export function ShipTrack({ records }: { records: PositionRecord[] }) {
  // history returns newest first; reversed so the line reads oldest to
  // newest and the start marker lands on the earliest fix.
  const points = [...records]
    .reverse()
    .filter(hasPosition)
    .map((record) => ({
      position: [record.latitude, record.longitude] as [number, number],
      at: receivedAt(record),
    }));

  // A single fix is a dot, not a track.
  if (points.length < 2) return null;

  const start = points[0];

  return (
    <>
      <Polyline
        positions={points.map((point) => point.position)}
        pathOptions={{ color: TRACK_COLOR, weight: 2, opacity: 0.85 }}
      />

      <CircleMarker
        center={start.position}
        radius={3}
        pathOptions={{
          color: TRACK_COLOR,
          fillColor: TRACK_COLOR,
          fillOpacity: 1,
          weight: 1,
        }}
      />
    </>
  );
}
