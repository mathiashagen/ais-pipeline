import { BLIP_CATEGORIES } from "../radar";

export function RadarLegend() {
  return (
    <ul className="radar-legend" aria-label="Legend">
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
  );
}
