import { useMemo, useState } from "react";
import { divIcon, type DivIcon } from "leaflet";
import { Circle, CircleMarker, Marker, Polyline, useMap, useMapEvents } from "react-leaflet";
import { METRES_PER_NM, offsetFrom, RING_COUNT, RING_FILL, type RadarCentre } from "../radar";

const RING_COLOUR = "#3b4a40";
const CENTRE_COLOUR = "#e8b04b";

const RING_STYLE = { color: RING_COLOUR, weight: 1, fill: false };
const CROSS_STYLE = { color: RING_COLOUR, weight: 1 };
const CENTRE_STYLE = {
  color: CENTRE_COLOUR,
  fillColor: CENTRE_COLOUR,
  fillOpacity: 1,
  weight: 0,
};

/** Roughly the width of "0.75 nm" in the label font, plus a gap. */
const MIN_LABEL_SPACING_PX = 56;

const labelIcons = new Map<string, DivIcon>();

/**
 * Cached per text. react-leaflet replaces a marker's DOM whenever its icon
 * changes identity, and the app re-renders on every poll.
 */
function labelIcon(text: string, className = "radar-label"): DivIcon {
  const key = `${className}|${text}`;
  let icon = labelIcons.get(key);
  if (!icon) {
    // Zero-size icon anchored at the point; the CSS positions the text off it.
    icon = divIcon({ className, html: text, iconSize: [0, 0] });
    labelIcons.set(key, icon);
  }
  return icon;
}

function formatNm(nm: number): string {
  return `${Number.isInteger(nm) ? nm : String(nm)} nm`;
}

/**
 * Range rings around the centre, RING_COUNT of them evenly spaced out to the
 * selected range. The map is zoomed so the outer ring fills the view the
 * same way at every range (see zoomForRange), so the rings need no zoom
 * logic of their own.
 */
export function RadarScope({ centre, rangeNm }: { centre: RadarCentre; rangeNm: number }) {
  const map = useMap();
  const [size, setSize] = useState(() => map.getSize());

  // Memoised so the listener stays registered across renders: useMapEvents
  // re-subscribes whenever the handler object changes, and a resize landing
  // between the old unsubscribe and the new subscribe would be lost.
  const handlers = useMemo(() => ({ resize: () => setSize(map.getSize()) }), [map]);
  useMapEvents(handlers);

  // Memoised because react-leaflet redraws a layer whenever its positions
  // change identity, and the app re-renders on every poll.
  const { position, rings, northSouth, westEast, northPosition } = useMemo(() => {
    const spacing = rangeNm / RING_COUNT;
    return {
      position: [centre.lat, centre.lon] as [number, number],
      rings: Array.from({ length: RING_COUNT }, (_, i) => {
        const nm = spacing * (i + 1);
        return { nm, labelPosition: offsetFrom(centre, 0, nm) };
      }),
      northSouth: [offsetFrom(centre, -rangeNm, 0), offsetFrom(centre, rangeNm, 0)],
      westEast: [offsetFrom(centre, 0, -rangeNm), offsetFrom(centre, 0, rangeNm)],
      northPosition: offsetFrom(centre, rangeNm, 0),
    };
  }, [centre, rangeNm]);

  // Labels sit along the east axis, one ring-spacing apart. On a narrow
  // screen that can be less than a label is wide, so walking outwards in, a
  // label is kept only if it clears the last one kept. The outer ring always
  // has one.
  const spacingPx = (RING_FILL * Math.min(size.x, size.y)) / 2 / RING_COUNT;
  const labelled = new Set<number>();
  let lastLabelledIndex = Infinity;
  for (let i = rings.length - 1; i >= 0; i--) {
    if ((lastLabelledIndex - i) * spacingPx >= MIN_LABEL_SPACING_PX) {
      labelled.add(rings[i].nm);
      lastLabelledIndex = i;
    }
  }

  return (
    <>
      {rings.map(({ nm }) => (
        <Circle
          key={nm}
          center={position}
          // Metres, not pixels: Leaflet keeps the ring the right geographic
          // size at every zoom, so a 10 nm ring really spans 10 nm of sea.
          radius={nm * METRES_PER_NM}
          interactive={false}
          pathOptions={RING_STYLE}
        />
      ))}

      <Polyline positions={northSouth} interactive={false} pathOptions={CROSS_STYLE} />
      <Polyline positions={westEast} interactive={false} pathOptions={CROSS_STYLE} />

      {rings
        .filter(({ nm }) => labelled.has(nm))
        .map(({ nm, labelPosition }) => (
          <Marker
            key={`label-${nm}`}
            position={labelPosition}
            icon={labelIcon(formatNm(nm))}
            interactive={false}
            keyboard={false}
          />
        ))}

      <Marker
        position={northPosition}
        icon={labelIcon("N", "radar-label radar-north")}
        interactive={false}
        keyboard={false}
      />

      <CircleMarker center={position} radius={4} interactive={false} pathOptions={CENTRE_STYLE} />
    </>
  );
}
