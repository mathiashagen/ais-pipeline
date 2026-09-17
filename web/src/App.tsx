import { useEffect, useState } from "react";
import { LocationPicker } from "./components/LocationPicker";
import { RadarMap } from "./components/RadarMap";
import { ShipPanel } from "./components/ShipPanel";
import { usePositions } from "./hooks/usePositions";
import { useHistory } from "./hooks/useHistory";
import { useNow } from "./hooks/useNow";
import { useRadarView } from "./hooks/useRadarView";
import { hasPosition } from "./api/types";
import { boundingBoxAround, inRange, type RadarCentre } from "./radar";
import "./App.css";

export default function App() {
  const { centre, setCentre, rangeNm, setRangeNm } = useRadarView();
  // Only the ships that can be inside the outer ring are fetched. The API
  // takes a box, so the box's corners come back too and are filtered below.
  const { records, error, loading, lastUpdated } = usePositions(
    boundingBoxAround(centre, rangeNm),
    5000,
  );
  const [picking, setPicking] = useState(false);
  const [selectedMmsi, setSelectedMmsi] = useState<number | null>(null);
  const track = useHistory(selectedMmsi);
  const now = useNow(10_000);

  const visible = records
    .filter(hasPosition)
    .filter((record) => inRange(centre, rangeNm, record));
  // Looked up in the latest poll rather than kept from the click, so a name
  // that arrives while the ship is selected shows up. Among the visible ships,
  // so a ship that sails out of the ring is reported as gone rather than
  // described while not drawn.
  const selected = visible.find((record) => record.mmsi === selectedMmsi);

  // Escape backs out of whatever is open: pick mode first, then the selected
  // ship. The search box handles its own Escape, clearing the query.
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key !== "Escape") return;
      if (event.target instanceof HTMLElement && event.target.closest("input, textarea, select")) return;
      if (picking) setPicking(false);
      else setSelectedMmsi(null);
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [picking]);

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
        <div className="brand">
          <h1>AIS pipeline</h1>

          <p className="status" aria-live="polite">
            {loading && "Loading…"}

            {!loading && !error && (
              <>
                <strong>{visible.length}</strong> ships within {rangeNm} nm
                {lastUpdated && (
                  <span className="status-updated"> · updated {lastUpdated.toLocaleTimeString()}</span>
                )}
              </>
            )}

            {/* Kept visible alongside the radar: the last good positions stay on
                screen, so without this a stalled API looks like calm seas. */}
            {error && <span className="error">{error}</span>}
          </p>
        </div>

        <LocationPicker
          centre={centre}
          onChange={moveTo}
          picking={picking}
          onPickingChange={setPicking}
        />
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
        loading={loading}
      >
        {selectedMmsi !== null && (
          <ShipPanel
            mmsi={selectedMmsi}
            record={selected}
            track={track}
            now={now}
            onClose={() => setSelectedMmsi(null)}
          />
        )}
      </RadarMap>
    </div>
  );
}
