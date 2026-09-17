import { useState } from "react";
import { BLIP_CATEGORIES } from "../radar";

/** Matches the breakpoint in App.css where the layout switches to phone. */
const WIDE_SCREEN = "(min-width: 640px)";

export function RadarLegend() {
  // Open where there is room for it. On a phone it covered a third of the
  // radar, so there it starts closed. Only the starting state: after that the
  // <details> element keeps whatever the user chose.
  const [initiallyOpen] = useState(() => window.matchMedia(WIDE_SCREEN).matches);

  return (
    <details className="radar-legend" open={initiallyOpen}>
      <summary>Legend</summary>
      <ul>
        {BLIP_CATEGORIES.map(({ category, label, colour }) => (
          <li key={category}>
            <svg viewBox="-6 -6 12 12" width="11" height="11" aria-hidden="true">
              <path d="M0,-5.5 L4,4.5 L0,2 L-4,4.5 Z" fill={colour} />
            </svg>
            {label}
          </li>
        ))}
        <li className="radar-legend-note">faded: no report for 5 min · dot: not making way</li>
      </ul>
    </details>
  );
}
