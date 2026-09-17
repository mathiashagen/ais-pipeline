import { useEffect, useEffectEvent, useRef, useState, type ReactNode } from "react";
import { GeoJSON, MapContainer, useMap, useMapEvents } from "react-leaflet";
import type { PositionedRecord, PositionRecord } from "../api/types";
import { useCoastline } from "../hooks/useCoastline";
import {
  RADAR_RANGES_NM,
  stepRange,
  zoomForRange,
  type RadarCentre,
  type ViewBox,
} from "../radar";
import { RadarBlips } from "./RadarBlips";
import { RadarLegend } from "./RadarLegend";
import { RadarScope } from "./RadarScope";
import { ShipTrack } from "./ShipTrack";

// Without this Leaflet's panes lay out as an unstyled pile of elements.
// Leaflet ships its own CSS and the bundler will not find it on its own.
import "leaflet/dist/leaflet.css";

const COASTLINE_STYLE = { color: "#5f7d68", weight: 1, opacity: 0.9 };

const KARTVERKET_ATTRIBUTION =
  '&copy; <a href="https://www.kartverket.no/">Kartverket</a> (N250, CC BY 4.0)';

/** Wheel travel, in pixels, that counts as one step of the range knob. */
const WHEEL_STEP_PX = 80;

/** Minimum time between wheel steps, so one fast spin does not run the whole scale. */
const WHEEL_COOLDOWN_MS = 150;

/** A wheel pause longer than this starts a new gesture; leftover travel is dropped. */
const WHEEL_GESTURE_GAP_MS = 300;

/** How far fingers must spread (or close) during a pinch to step the range once. */
const PINCH_STEP_RATIO = 1.35;

type Direction = "in" | "out";

/**
 * Holds the view on the centre, at the zoom where the selected range fills
 * the view (see zoomForRange). The zoom is recomputed rather than kept, since
 * it depends on the view's size and the centre's latitude as well as the
 * range. MapContainer's own center/zoom props only apply at creation, and
 * Leaflet does not notice its container changing size, only the window --
 * hence the ResizeObserver.
 */
function LockedView({ centre, rangeNm }: { centre: RadarCentre; rangeNm: number }) {
  const map = useMap();
  const shown = useRef<{ centre: RadarCentre; rangeNm: number } | null>(null);

  useEffect(() => {
    const position: [number, number] = [centre.lat, centre.lon];
    const zoomNow = () => {
      const size = map.getSize();
      return zoomForRange(rangeNm, Math.min(size.x, size.y) / 2, centre.lat);
    };

    // Animate a range change at the same place, like turning a range knob.
    // A move to somewhere else is a jump: animating between two distant
    // places would sweep the coastline across the screen.
    const previous = shown.current;
    const animate = previous !== null && previous.centre === centre && previous.rangeNm !== rangeNm;
    map.setView(position, zoomNow(), { animate });
    shown.current = { centre, rangeNm };

    const container = map.getContainer();
    let lastWidth = container.clientWidth;
    let lastHeight = container.clientHeight;

    const observer = new ResizeObserver(() => {
      // ResizeObserver reports once as soon as it starts observing. Acting on
      // that would cut the range animation above short, so only real size
      // changes count.
      if (container.clientWidth === lastWidth && container.clientHeight === lastHeight) return;
      lastWidth = container.clientWidth;
      lastHeight = container.clientHeight;

      map.invalidateSize({ pan: false });
      map.setView(position, zoomNow(), { animate: false });
    });
    observer.observe(container);

    return () => observer.disconnect();
  }, [map, centre, rangeNm]);

  return null;
}

/**
 * Turns scroll, pinch, double-click and the +/- keys into range steps.
 * Leaflet's own zoom handlers are off: they zoom continuously, and any zoom
 * between two ranges would leave the rings the wrong size for the view.
 */
function RangeInput({ onStep }: { onStep: (direction: Direction) => void }) {
  const map = useMap();
  // Always calls the latest onStep without re-attaching every listener.
  const step = useEffectEvent(onStep);

  useEffect(() => {
    const container = map.getContainer();

    let wheelTravel = 0;
    let lastWheelEvent = 0;
    let lastWheelStep = 0;

    const onWheel = (event: WheelEvent) => {
      event.preventDefault(); // otherwise the page scrolls as well
      const now = performance.now();
      if (now - lastWheelEvent > WHEEL_GESTURE_GAP_MS) wheelTravel = 0;
      lastWheelEvent = now;

      // Firefox can report in lines or pages rather than pixels.
      const scale = event.deltaMode === 1 ? 40 : event.deltaMode === 2 ? 800 : 1;
      wheelTravel += event.deltaY * scale;

      if (Math.abs(wheelTravel) >= WHEEL_STEP_PX && now - lastWheelStep >= WHEEL_COOLDOWN_MS) {
        // Scrolling up (negative delta) zooms in, as on every map.
        step(wheelTravel < 0 ? "in" : "out");
        wheelTravel = 0;
        lastWheelStep = now;
      }
    };

    let pinchBaseline: number | null = null;
    const spread = (touches: TouchList) =>
      Math.hypot(touches[0].clientX - touches[1].clientX, touches[0].clientY - touches[1].clientY);

    const onTouchStart = (event: TouchEvent) => {
      if (event.touches.length === 2) pinchBaseline = spread(event.touches);
    };
    const onTouchMove = (event: TouchEvent) => {
      if (pinchBaseline === null || event.touches.length !== 2) return;
      event.preventDefault(); // otherwise the browser zooms the whole page
      const ratio = spread(event.touches) / pinchBaseline;
      if (ratio >= PINCH_STEP_RATIO || ratio <= 1 / PINCH_STEP_RATIO) {
        step(ratio > 1 ? "in" : "out");
        pinchBaseline = spread(event.touches); // measure the next step from here
      }
    };
    const onTouchEnd = (event: TouchEvent) => {
      if (event.touches.length < 2) pinchBaseline = null;
    };

    const onDoubleClick = () => step("in");

    const onKeyDown = (event: KeyboardEvent) => {
      const target = event.target;
      if (target instanceof HTMLElement && (target.isContentEditable || target.closest("input, textarea, select"))) {
        return;
      }
      if (event.key === "+" || event.key === "=") step("in");
      if (event.key === "-" || event.key === "_") step("out");
    };

    // passive: false is required for preventDefault to work on these.
    container.addEventListener("wheel", onWheel, { passive: false });
    container.addEventListener("touchstart", onTouchStart, { passive: true });
    container.addEventListener("touchmove", onTouchMove, { passive: false });
    container.addEventListener("touchend", onTouchEnd);
    container.addEventListener("touchcancel", onTouchEnd);
    container.addEventListener("dblclick", onDoubleClick);
    window.addEventListener("keydown", onKeyDown);

    return () => {
      container.removeEventListener("wheel", onWheel);
      container.removeEventListener("touchstart", onTouchStart);
      container.removeEventListener("touchmove", onTouchMove);
      container.removeEventListener("touchend", onTouchEnd);
      container.removeEventListener("touchcancel", onTouchEnd);
      container.removeEventListener("dblclick", onDoubleClick);
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [map]);

  return null;
}

/** Reports the visible area whenever it changes, so the coastline can follow it. */
function ViewBoxReporter({ onChange }: { onChange: (box: ViewBox) => void }) {
  const map = useMap();
  const report = useEffectEvent(() => {
    const bounds = map.getBounds();
    onChange({
      south: bounds.getSouth(),
      west: bounds.getWest(),
      north: bounds.getNorth(),
      east: bounds.getEast(),
    });
  });

  // Subscribed once, in an effect, rather than through useMapEvents: the
  // listener must never be briefly absent between renders, and an effect
  // event may only be called from code that an effect sets up.
  useEffect(() => {
    const onViewChange = () => report();
    report();
    map.on("moveend resize", onViewChange);
    return () => {
      map.off("moveend resize", onViewChange);
    };
  }, [map]);

  return null;
}

/**
 * Background clicks: in pick mode they move the radar, otherwise they clear
 * the selected ship. Leaflet stops click propagation from markers, so
 * clicking a ship never reaches here.
 */
function MapClicks({
  picking,
  onPick,
  onClear,
}: {
  picking: boolean;
  onPick: (lat: number, lon: number) => void;
  onClear: () => void;
}) {
  const map = useMap();

  useEffect(() => {
    map.getContainer().classList.toggle("picking", picking);
  }, [map, picking]);

  useMapEvents({
    click: (event) => {
      if (picking) onPick(event.latlng.lat, event.latlng.lng);
      else onClear();
    },
  });

  return null;
}

/** The range knob: step in, the current range, step out. */
function RangeControl({
  rangeNm,
  onStep,
}: {
  rangeNm: number;
  onStep: (direction: Direction) => void;
}) {
  return (
    <div className="radar-range" role="group" aria-label="Radar range">
      <button
        type="button"
        aria-label="Shorter range"
        disabled={rangeNm === RADAR_RANGES_NM[0]}
        onClick={() => onStep("in")}
      >
        +
      </button>
      <output aria-live="polite">{rangeNm} nm</output>
      <button
        type="button"
        aria-label="Longer range"
        disabled={rangeNm === RADAR_RANGES_NM[RADAR_RANGES_NM.length - 1]}
        onClick={() => onStep("out")}
      >
        −
      </button>
    </div>
  );
}

export interface RadarMapProps {
  centre: RadarCentre;
  rangeNm: number;
  onRangeChange: (rangeNm: number) => void;
  /** Ships with a position inside the radar range. */
  records: PositionedRecord[];
  now: number;
  selectedMmsi: number | null;
  onSelect: (mmsi: number | null) => void;
  track: PositionRecord[];
  picking: boolean;
  onPick: (lat: number, lon: number) => void;
  /** True until the first poll has answered. */
  loading: boolean;
  /** Overlaid on the map, such as the selected ship's details. */
  children?: ReactNode;
}

export function RadarMap({
  centre,
  rangeNm,
  onRangeChange,
  records,
  now,
  selectedMmsi,
  onSelect,
  track,
  picking,
  onPick,
  loading,
  children,
}: RadarMapProps) {
  const [viewBox, setViewBox] = useState<ViewBox | null>(null);
  const coastline = useCoastline(viewBox);

  const step = (direction: Direction) => onRangeChange(stepRange(rangeNm, direction));

  return (
    <div className="radar">
      <MapContainer
        center={[centre.lat, centre.lon]}
        zoom={7}
        // Any zoom is allowed, not just quarter levels: the zoom for a range
        // is computed exactly, and snapping would put the rings off by up
        // to 19%.
        zoomSnap={0}
        // The radar does not move and has its own range control, so every
        // built-in way of panning or zooming is off.
        dragging={false}
        keyboard={false}
        boxZoom={false}
        scrollWheelZoom={false}
        doubleClickZoom={false}
        touchZoom={false}
        zoomControl={false}
        className="map"
      >
        <LockedView centre={centre} rangeNm={rangeNm} />
        <RangeInput onStep={step} />
        <ViewBoxReporter onChange={setViewBox} />

        {coastline.tiles.map((tile) => (
          <GeoJSON
            key={tile.id}
            data={tile.data}
            style={COASTLINE_STYLE}
            interactive={false}
            attribution={KARTVERKET_ATTRIBUTION}
          />
        ))}

        <RadarScope centre={centre} rangeNm={rangeNm} />

        <MapClicks picking={picking} onPick={onPick} onClear={() => onSelect(null)} />

        {/* Under the blips, so a ship is never hidden by its own track. */}
        <ShipTrack records={track} />

        <RadarBlips records={records} now={now} selectedMmsi={selectedMmsi} onSelect={onSelect} />
      </MapContainer>

      <RangeControl rangeNm={rangeNm} onStep={step} />
      <RadarLegend />
      {children}

      {/* The rings draw at once, so without this an empty scope looks like
          calm seas until the first answer lands. */}
      {loading && <p className="radar-notice">Loading ships…</p>}
      {!loading && coastline.error && <p className="radar-notice radar-error">{coastline.error}</p>}
      {!loading && coastline.outsideCoverage && (
        <p className="radar-notice">No coastline data here — Kartverket's covers mainland Norway</p>
      )}
    </div>
  );
}
