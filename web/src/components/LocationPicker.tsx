import {
  useEffect,
  useEffectEvent,
  useId,
  useRef,
  useState,
  type ButtonHTMLAttributes,
  type ReactNode,
} from "react";
import { searchPlaces, type Place } from "../api/placeSearch";
import { formatCentre, type RadarCentre } from "../radar";

const SEARCH_DELAY_MS = 300;
const MIN_QUERY_LENGTH = 2;

const CROSSHAIR_ICON = (
  <svg viewBox="0 0 16 16" width="15" height="15" aria-hidden="true">
    <g fill="none" stroke="currentColor" strokeWidth="1.5">
      <circle cx="8" cy="8" r="5" />
      <path d="M8 0.5v4M8 11.5v4M0.5 8h4M11.5 8h4" />
    </g>
  </svg>
);

const PIN_ICON = (
  <svg className="location-pin" viewBox="0 0 16 16" width="14" height="14" aria-hidden="true">
    <path
      d="M8 15s5-4.6 5-8.5A5 5 0 0 0 3 6.5C3 10.4 8 15 8 15z"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
    />
    <circle cx="8" cy="6.5" r="1.75" fill="currentColor" />
  </svg>
);

const LOCATE_ICON = (
  <svg viewBox="0 0 16 16" width="15" height="15" aria-hidden="true">
    <path d="M14.5 1.5 1.5 7l5.5 2 2 5.5z" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinejoin="round" />
  </svg>
);

/**
 * A button with an icon and a text label. On a phone the CSS hides the label
 * to fit the header on one line; aria-label keeps the name for screen
 * readers, and title shows it on hover.
 */
function IconButton({
  label,
  icon,
  ...props
}: { label: string; icon: ReactNode } & Omit<ButtonHTMLAttributes<HTMLButtonElement>, "children">) {
  return (
    <button type="button" aria-label={label} title={label} {...props}>
      {icon}
      <span className="button-label">{label}</span>
    </button>
  );
}

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
      {/* The current centre and the search for a new one share one frame, so
          the place name reads as the field's value rather than loose text. */}
      <div className="location-field">
        {PIN_ICON}
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
      </div>

      <IconButton
        label={picking ? "Click the map…" : "Pick on map"}
        icon={CROSSHAIR_ICON}
        className={picking ? "active" : undefined}
        aria-pressed={picking}
        onClick={() => onPickingChange(!picking)}
      />

      <IconButton
        label={locating ? "Locating…" : "My position"}
        icon={LOCATE_ICON}
        onClick={centreOnDevicePosition}
        disabled={locating}
      />

      {/* Dropped below the header rather than inline, where it would wrap it. */}
      {locateError && (
        <p className="location-error error" role="alert">
          {locateError}
        </p>
      )}
    </div>
  );
}
