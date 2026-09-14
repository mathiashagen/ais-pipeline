import { useCallback, useEffect, useState } from "react";
import { DEFAULT_CENTRE, DEFAULT_RANGE_NM, isRadarRange, type RadarCentre } from "../radar";

export interface RadarView {
  centre: RadarCentre;
  rangeNm: number;
}

function readFromUrl(): RadarView {
  const params = new URLSearchParams(window.location.search);
  const lat = Number(params.get("lat"));
  const lon = Number(params.get("lon"));
  const range = Number(params.get("range"));

  // Number("") is 0, a valid coordinate, so check presence before range.
  const validCentre =
    params.has("lat") &&
    params.has("lon") &&
    Number.isFinite(lat) &&
    Number.isFinite(lon) &&
    Math.abs(lat) <= 85 &&
    Math.abs(lon) <= 180;

  return {
    centre: validCentre ? { lat, lon, name: params.get("name") || null } : DEFAULT_CENTRE,
    // Only the ranges the radar can actually be set to; anything else in a
    // hand-edited URL falls back rather than producing rings at odd spacings.
    rangeNm: params.has("range") && isRadarRange(range) ? range : DEFAULT_RANGE_NM,
  };
}

function writeToUrl({ centre, rangeNm }: RadarView) {
  const params = new URLSearchParams(window.location.search);
  params.set("lat", centre.lat.toFixed(5));
  params.set("lon", centre.lon.toFixed(5));
  if (centre.name) params.set("name", centre.name);
  else params.delete("name");
  params.set("range", String(rangeNm));

  // replaceState, not pushState: stepping back through every range change
  // and every spot picked on the map would be more annoying than useful.
  window.history.replaceState(null, "", `${window.location.pathname}?${params}`);
}

/**
 * Where the radar is and what range it is set to, kept in the URL so a view
 * can be linked to and survives a reload. Read once on load, written after
 * every change (and once on load, which normalises a partial URL).
 */
export function useRadarView() {
  const [view, setView] = useState(readFromUrl);

  // The URL follows the state rather than being written inside the updater:
  // updaters must be pure, and React calls them twice in development to
  // catch the ones that are not.
  useEffect(() => writeToUrl(view), [view]);

  const update = useCallback((change: Partial<RadarView>) => {
    setView((previous) => ({ ...previous, ...change }));
  }, []);

  const setCentre = useCallback((centre: RadarCentre) => update({ centre }), [update]);
  const setRangeNm = useCallback((rangeNm: number) => update({ rangeNm }), [update]);

  return { centre: view.centre, rangeNm: view.rangeNm, setCentre, setRangeNm };
}
