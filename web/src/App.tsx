import { useState } from "react";
import { LocationPicker } from "./components/LocationPicker";
import { RadarMap } from "./components/RadarMap";
import { usePositions } from "./hooks/usePositions";
import { useHistory } from "./hooks/useHistory";
import { useNow } from "./hooks/useNow";
import { useRadarView } from "./hooks/useRadarView";
import { hasPosition } from "./api/types";
import { inRange, RADAR_RANGE_NM, type RadarCentre } from "./radar";
import "./App.css";

export default function App() {
  const { records, error, loading, lastUpdated } = usePositions(5000);
  const { centre, setCentre, rangeNm, setRangeNm } = useRadarView();
  const [picking, setPicking] = useState(false);
  const [selectedMmsi, setSelectedMmsi] = useState<number | null>(null);
  const track = useHistory(selectedMmsi);
  const now = useNow(10_000);

  const positioned = records.filter(hasPosition);
  const visible = positioned.filter((record) => inRange(centre, record));
  const trackPoints = track.records.filter(hasPosition).length;

  function moveTo(next: RadarCentre) {
    // The range stays as it was, like a radar's range knob.
    setCentre(next);
    setPicking(false);
    // A ship selected at the old location is almost never in range of the
    // new one, and its track would be drawn off in empty space.
    setSelectedMmsi(null);
  }

  return (
    <div className="app">
      <header>
        <h1>AIS pipeline</h1>

        <div className="status">
          {loading && <span>Loading…</span>}

          {!loading && !error && (
            <span>
              <strong>{visible.length}</strong> ships within {RADAR_RANGE_NM} nm
              {` (${positioned.length} tracked)`}
              {lastUpdated && ` · updated ${lastUpdated.toLocaleTimeString()}`}
            </span>
          )}

          {/* Kept visible alongside the radar: the last good positions stay on
              screen, so without this a stalled API looks like calm seas. */}
          {error && <span className="error">{error}</span>}
        </div>

        <LocationPicker
          centre={centre}
          onChange={moveTo}
          picking={picking}
          onPickingChange={setPicking}
        />

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

      <RadarMap
        centre={centre}
        rangeNm={rangeNm}
        onRangeChange={setRangeNm}
        records={visible}
        now={now}
        selectedMmsi={selectedMmsi}
        onSelect={setSelectedMmsi}
        track={track.records}
        picking={picking}
        onPick={(lat, lon) => moveTo({ lat, lon, name: null })}
      />
    </div>
  );
}
