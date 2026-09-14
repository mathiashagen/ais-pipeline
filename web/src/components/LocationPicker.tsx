import { useEffect, useEffectEvent, useId, useRef, useState } from "react";
import { searchPlaces, type Place } from "../api/placeSearch";
import { formatCentre, type RadarCentre } from "../radar";

const SEARCH_DELAY_MS = 300;
const MIN_QUERY_LENGTH = 2;

export interface LocationPickerProps {
  centre: RadarCentre;
  onChange: (centre: RadarCentre) => void;
  picking: boolean;
  onPickingChange: (picking: boolean) => void;
}

/** Results are tagged with the query that produced them, so stale ones are recognisable. */
type SearchState =
  | { status: "idle" }
  | { status: "searching"; query: string }
  | { status: "done"; query: string; places: Place[] }
  | { status: "failed"; query: string; message: string };

/**
 * Three ways to move the radar: search for a place, pick a point on the map,
 * or use the device's position. The map itself is locked to the centre, so
 * this is the only way to look somewhere else.
 */
export function LocationPicker({ centre, onChange, picking, onPickingChange }: LocationPickerProps) {
  const [query, setQuery] = useState("");
  const [search, setSearch] = useState<SearchState>({ status: "idle" });
  const [locating, setLocating] = useState(false);
  const [locateError, setLocateError] = useState<string | null>(null);
  const resultsId = useId();

  // Enter pressed before results arrived: take the top result once they do,
  // rather than silently ignoring a quick type-and-Enter. A ref, not state:
  // the search callback below is created before the key press, and reads the
  // ref's current value when results land, where state would be stale.
  const enterPending = useRef(false);

  const trimmed = query.trim();
  const active = trimmed.length >= MIN_QUERY_LENGTH;

  function choose(place: Place) {
    enterPending.current = false;
    onChange({ lat: place.lat, lon: place.lon, name: place.name });
    setQuery("");
    setSearch({ status: "idle" });
  }

  // An effect event: always sees the latest `choose` and props, without being
  // a dependency -- otherwise the search would rerun on every render, and the
  // app re-renders on every position poll.
  const onResults = useEffectEvent((forQuery: string, places: Place[]) => {
    if (enterPending.current && places[0]) choose(places[0]);
    else setSearch({ status: "done", query: forQuery, places });
  });

  useEffect(() => {
    if (!active) return;

    const controller = new AbortController();
    // Wait for a pause in typing, so every keystroke is not a request.
    const timer = window.setTimeout(() => {
      setSearch({ status: "searching", query: trimmed });
      searchPlaces(trimmed, controller.signal)
        .then((places) => onResults(trimmed, places))
        .catch((err: unknown) => {
          if (controller.signal.aborted) return;
          enterPending.current = false;
          setSearch({ status: "failed", query: trimmed, message: String(err) });
        });
    }, SEARCH_DELAY_MS);

    return () => {
      window.clearTimeout(timer);
      controller.abort();
    };
  }, [trimmed, active]);

  function centreOnDevicePosition() {
    if (!("geolocation" in navigator)) {
      setLocateError("This browser cannot report its position.");
      return;
    }
    setLocating(true);
    setLocateError(null);
    navigator.geolocation.getCurrentPosition(
      (position) => {
        setLocating(false);
        onChange({ lat: position.coords.latitude, lon: position.coords.longitude, name: null });
      },
      (error) => {
        setLocating(false);
        setLocateError(
          error.code === error.PERMISSION_DENIED
            ? "Location permission was denied."
            : "Could not get your position.",
        );
      },
      { timeout: 10_000, maximumAge: 60_000 },
    );
  }

  // Only what belongs to the text currently in the box is shown or chosen.
  const current = search.status !== "idle" && search.query === trimmed ? search : null;
  const showResults = active && current !== null;

  return (
    <div className="location">
      <span className="location-current" title="Radar centre">
        {formatCentre(centre)}
      </span>

      <div className="location-search">
        <input
          type="search"
          placeholder="Search place…"
          aria-label="Search for a place"
          aria-controls={resultsId}
          aria-expanded={showResults}
          value={query}
          onChange={(event) => {
            enterPending.current = false;
            setQuery(event.target.value);
          }}
          onKeyDown={(event) => {
            if (event.key === "Escape") setQuery("");
            if (event.key !== "Enter" || !active) return;

            if (current?.status === "done") {
              if (current.places[0]) choose(current.places[0]);
            } else if (current?.status !== "failed") {
              enterPending.current = true;
            }
          }}
        />

        {showResults && (
          <ul id={resultsId} className="location-results">
            {current?.status === "searching" && <li className="location-note">Searching…</li>}
            {current?.status === "failed" && <li className="location-note">{current.message}</li>}
            {current?.status === "done" && current.places.length === 0 && (
              <li className="location-note">No places found</li>
            )}
            {current?.status === "done" &&
              current.places.map((place) => (
                <li key={`${place.name}|${place.lat}|${place.lon}`}>
                  <button type="button" onClick={() => choose(place)}>
                    <strong>{place.name}</strong>
                    <span>
                      {place.kind}
                      {place.municipality && `, ${place.municipality}`}
                    </span>
                  </button>
                </li>
              ))}
          </ul>
        )}
      </div>

      <button
        type="button"
        className={picking ? "active" : undefined}
        aria-pressed={picking}
        onClick={() => onPickingChange(!picking)}
      >
        {picking ? "Click the map…" : "Pick on map"}
      </button>

      <button type="button" onClick={centreOnDevicePosition} disabled={locating}>
        {locating ? "Locating…" : "My position"}
      </button>

      {locateError && <span className="error">{locateError}</span>}
    </div>
  );
}
