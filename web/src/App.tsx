import { useState } from "react";
import { ShipMap } from "./components/ShipMap";
import { usePositions } from "./hooks/usePositions";
import { useHistory } from "./hooks/useHistory";
import { useNow } from "./hooks/useNow";
import { hasPosition } from "./api/types";
import "./App.css";

export default function App() {
  const { records, error, loading, lastUpdated } = usePositions(5000);
  const [selectedMmsi, setSelectedMmsi] = useState<number | null>(null);
  const track = useHistory(selectedMmsi);
  const now = useNow(10_000);

  const positioned = records.filter(hasPosition);
  const trackPoints = track.records.filter(hasPosition).length;

  return (
    <div className="app">
      <header>
        <h1>AIS pipeline</h1>

        <div className="status">
          {loading && <span>Loading…</span>}

          {!loading && !error && (
            <span>
              <strong>{positioned.length}</strong> ships
              {positioned.length !== records.length &&
                ` (${records.length - positioned.length} without a position)`}
              {lastUpdated && ` · updated ${lastUpdated.toLocaleTimeString()}`}
            </span>
          )}

          {/* Kept visible alongside the map: the last good positions stay on
              screen, so without this a stalled API looks like calm seas. */}
          {error && <span className="error">{error}</span>}
        </div>

        {selectedMmsi !== null && (
          <div className="selection">
            <strong>MMSI {selectedMmsi}</strong>
            {track.loading && " · loading track…"}
            {track.error && <span className="error"> {track.error}</span>}
            {!track.loading &&
              !track.error &&
              ` · ${trackPoints} track point${trackPoints === 1 ? "" : "s"}`}
            <button type="button" onClick={() => setSelectedMmsi(null)}>
              Clear
            </button>
          </div>
        )}
      </header>

      <ShipMap
        records={records}
        now={now}
        selectedMmsi={selectedMmsi}
        onSelect={setSelectedMmsi}
        track={track.records}
      />
    </div>
  );
}
