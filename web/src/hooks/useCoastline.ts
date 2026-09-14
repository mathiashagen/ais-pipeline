import { useEffect, useMemo, useState } from "react";
import type { FeatureCollection } from "geojson";
import { coastlineTilesIn, type ViewBox } from "../radar";

export interface CoastlineTile {
  id: string;
  data: FeatureCollection;
}

export interface CoastlineState {
  tiles: CoastlineTile[];
  /** True once it is known there is no coastline data near the centre at all. */
  outsideCoverage: boolean;
  error: string | null;
}

interface CoastlineIndex {
  tiles: string[];
}

const base = `${import.meta.env.BASE_URL}coastline/`;

// Module-level caches: moving the centre back and forth, or a remount, does
// not refetch what the browser already has parsed. Promises rather than
// results, so two requests for the same tile at once share one fetch.
let indexPromise: Promise<Set<string>> | null = null;
const tilePromises = new Map<string, Promise<FeatureCollection>>();

async function getJson<T>(url: string): Promise<T> {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`HTTP ${response.status} for ${url}`);
  return (await response.json()) as T;
}

function loadIndex(): Promise<Set<string>> {
  indexPromise ??= getJson<CoastlineIndex>(`${base}index.json`)
    .then((index) => new Set(index.tiles))
    .catch((err: unknown) => {
      indexPromise = null; // let a later render retry
      throw err;
    });
  return indexPromise;
}

function loadTile(id: string): Promise<FeatureCollection> {
  let promise = tilePromises.get(id);
  if (!promise) {
    promise = getJson<FeatureCollection>(`${base}${id}.geojson`).catch((err: unknown) => {
      tilePromises.delete(id);
      throw err;
    });
    tilePromises.set(id, promise);
  }
  return promise;
}

/**
 * Loads the coastline tiles covering the visible area. Tiles are fetched at
 * runtime rather than bundled -- the whole coast is several MB -- and only
 * the ones listed in index.json are requested, so open sea and inland areas
 * cost nothing instead of a round of 404s.
 *
 * Driven by what is on screen rather than a fixed distance from the centre:
 * a wide window at a long range reaches well past the outer ring at the
 * sides, and the coast should not stop short there.
 */
export function useCoastline(view: ViewBox | null): CoastlineState {
  // The tile set as a string: the effect below reruns only when it changes,
  // not on every change of view that stays within the same tiles.
  const key = useMemo(() => (view ? coastlineTilesIn(view).join(",") : ""), [view]);

  const [state, setState] = useState<{ key: string } & CoastlineState>({
    key: "",
    tiles: [],
    outsideCoverage: false,
    error: null,
  });

  useEffect(() => {
    if (key === "") return;
    let cancelled = false;

    loadIndex()
      .then((available) => {
        const ids = key.split(",").filter((id) => available.has(id));
        return Promise.all(ids.map(async (id) => ({ id, data: await loadTile(id) })));
      })
      .then((tiles) => {
        if (cancelled) return;
        setState({ key, tiles, outsideCoverage: tiles.length === 0, error: null });
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        setState({ key, tiles: [], outsideCoverage: false, error: `Could not load coastline: ${String(err)}` });
      });

    return () => {
      cancelled = true;
    };
  }, [key]);

  // Tiles fetched for a previous centre are still drawn while the new set
  // loads -- shared tiles look continuous that way instead of blinking out.
  return { tiles: state.tiles, outsideCoverage: state.key === key && state.outsideCoverage, error: state.error };
}
