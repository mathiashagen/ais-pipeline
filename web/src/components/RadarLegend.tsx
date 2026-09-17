import { useState } from "react";
import { BLIP_CATEGORIES } from "../radar";

/** Matches the breakpoint in App.css where the layout switches to phone. */
const WIDE_SCREEN = "(min-width: 640px)";

/** Same shape as a blip in RadarBlips, pointing north. */
const ARROW_PATH = "M0,-5.5 L4,4.5 L0,2 L-4,4.5 Z";

const NEUTRAL = "#c6c1ad";
const SELECTED = "#f97316";

function Arrow({ colour, opacity = 1 }: { colour: string; opacity?: number }) {
  return (
    <svg viewBox="-6 -6 12 12" width="12" height="12" aria-hidden="true">
      <path d={ARROW_PATH} fill={colour} fillOpacity={opacity} />
    </svg>
  );
}

/** The symbols a blip can take, drawn as the radar draws them. */
const SYMBOLS = [
  { label: "no report for 5 min", icon: <Arrow colour={NEUTRAL} opacity={0.35} /> },
  {
    label: "not making way",
    icon: (
      <svg viewBox="-6 -6 12 12" width="12" height="12" aria-hidden="true">
        <circle r="2.75" fill={NEUTRAL} />
      </svg>
    ),
  },
  {
    label: "selected ship",
    icon: (
      <svg viewBox="-11 -11 22 22" width="14" height="14" aria-hidden="true">
        <circle r="9" fill="none" stroke={SELECTED} strokeWidth="2" />
        <path d="M0,-6.5 L4.5,5 L0,2.5 L-4.5,5 Z" fill={NEUTRAL} />
      </svg>
    ),
  },
  {
    label: "recorded track",
    icon: (
      <svg viewBox="0 0 14 12" width="14" height="12" aria-hidden="true">
        <path d="M1 10 L5 6 L9 7 L13 2" fill="none" stroke={SELECTED} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
      </svg>
    ),
  },
];

export function RadarLegend() {
  // Open where there is room for it. On a phone it covered a third of the
  // radar, so there it starts closed. Only the starting state: after that the
  // <details> element keeps whatever the user chose.
  const [initiallyOpen] = useState(() => window.matchMedia(WIDE_SCREEN).matches);

  return (
    <details className="radar-legend" open={initiallyOpen}>
      <summary>Legend</summary>

      <div className="radar-legend-body">
        <h3>Ship type</h3>
        <ul className="radar-legend-types">
          {BLIP_CATEGORIES.map(({ category, label, colour }) => (
            <li key={category}>
              <Arrow colour={colour} />
              {label}
            </li>
          ))}
        </ul>

        <h3>Symbols</h3>
        <ul className="radar-legend-symbols">
          {SYMBOLS.map(({ label, icon }) => (
            <li key={label}>
              <span className="radar-legend-icon">{icon}</span>
              {label}
            </li>
          ))}
        </ul>
      </div>
    </details>
  );
}
