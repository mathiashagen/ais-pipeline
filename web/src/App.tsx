import { ShipMap } from "./components/ShipMap";
import { usePositions } from "./hooks/usePositions";
import { useNow } from "./hooks/useNow";
import { hasPosition } from "./api/types";
import "./App.css";

export default function App() {
  const { records, error, loading, lastUpdated } = usePositions(5000);
  const now = useNow(10_000);
  const positioned = records.filter(hasPosition);

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
      </header>

      <ShipMap records={records} now={now} />
    </div>
  );
}
