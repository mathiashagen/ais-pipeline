// Builds public/coastline/ -- the Norwegian coastline as 1x1 degree GeoJSON
// tiles, plus index.json listing which tiles exist.
//
// Source data (CC BY 4.0, "© Kartverket"): N250 Kartdata, format GML,
// projection EUREF89 UTM zone 33, ordered per county from
// https://kartkatalog.geonorge.no (dataset 442cae64-b447-478d-b384-545bc1d9ab48).
// Download every county with a coastline (all but Innlandet), unzip, and pass
// the *_N250Arealdekke_GML.gml files:
//
//   node scripts/build-coastline.mjs path/to/*_N250Arealdekke_GML.gml
//
// Only the coastline (Kystkontur) is kept. The data has no land polygons: sea
// surface polygons stop at the territorial border, so any filled land derived
// from them would draw a false shore out at sea.
//
// Tiled because the whole coast is too much for the browser to load at once.
// The radar can be centred anywhere, and loads only the tiles around it; the
// tile size must match COASTLINE_TILE_DEGREES in src/radar.ts.

import { createReadStream, mkdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import proj4 from "proj4";

const TILE_DEGREES = 1;

/**
 * Douglas–Peucker tolerance in metres. N250 is generalised for 1:100k–1:300k,
 * so vertices closer than this add file size without visible detail.
 */
const SIMPLIFY_METRES = 25;

/** ~1 m at Norwegian latitudes; more digits are noise in the output. */
const DECIMALS = 5;

const UTM33 = "+proj=utm +zone=33 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs";
const toWgs84 = proj4(UTM33, "WGS84");

const OPEN_TAG = "<app:Kystkontur ";
const CLOSE_TAG = "</app:Kystkontur>";

/**
 * Yields the posList of every <app:Kystkontur> feature in a GML file, as flat
 * numbers. Streams the file: a large county's GML runs to hundreds of MB,
 * close to V8's maximum string length, so it is never held in memory whole.
 */
async function* coastlines(path) {
  let pending = "";

  for await (const chunk of createReadStream(path, { encoding: "utf8", highWaterMark: 4 << 20 })) {
    pending += chunk;
    let from = 0;

    for (;;) {
      const start = pending.indexOf(OPEN_TAG, from);
      if (start === -1) break;
      const end = pending.indexOf(CLOSE_TAG, start);
      if (end === -1) {
        // Feature continues in the next chunk; keep it from its opening tag.
        from = start;
        break;
      }
      from = end + CLOSE_TAG.length;

      const list = /<gml:posList[^>]*>([^<]*)<\/gml:posList>/.exec(pending.slice(start, end));
      if (!list) continue;

      const numbers = list[1].trim().split(/\s+/).map(Number);
      if (numbers.length < 4 || numbers.length % 2 !== 0 || numbers.some(Number.isNaN)) {
        throw new Error(`malformed posList in a Kystkontur feature in ${path}`);
      }
      yield numbers;
    }

    // Keep an unfinished feature, or enough of the tail that an opening tag
    // split across the chunk boundary is still found next time round.
    const unfinished = pending.indexOf(OPEN_TAG, from);
    pending = unfinished !== -1 ? pending.slice(unfinished) : pending.slice(-OPEN_TAG.length);
  }
}

/** Pairs a flat [x0, y0, x1, y1, ...] list into points. */
function toPoints(numbers) {
  const points = [];
  for (let i = 0; i < numbers.length; i += 2) points.push([numbers[i], numbers[i + 1]]);
  return points;
}

function perpendicularDistance([px, py], [ax, ay], [bx, by]) {
  const dx = bx - ax;
  const dy = by - ay;
  const lengthSquared = dx * dx + dy * dy;
  if (lengthSquared === 0) return Math.hypot(px - ax, py - ay);
  const t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / lengthSquared));
  return Math.hypot(px - (ax + t * dx), py - (ay + t * dy));
}

/**
 * Douglas–Peucker, iterative so long coastlines cannot overflow the stack.
 * Runs in UTM metres, where the tolerance means the same thing everywhere.
 * Endpoints are always kept, so adjoining Kystkontur segments still meet.
 */
function simplify(points, tolerance) {
  if (points.length < 3) return points;

  const keep = new Uint8Array(points.length);
  keep[0] = keep[points.length - 1] = 1;
  const stack = [[0, points.length - 1]];

  while (stack.length > 0) {
    const [first, last] = stack.pop();
    let furthest = -1;
    let furthestDistance = tolerance;

    for (let i = first + 1; i < last; i++) {
      const distance = perpendicularDistance(points[i], points[first], points[last]);
      if (distance > furthestDistance) {
        furthest = i;
        furthestDistance = distance;
      }
    }

    if (furthest !== -1) {
      keep[furthest] = 1;
      stack.push([first, furthest], [furthest, last]);
    }
  }

  return points.filter((_, i) => keep[i] === 1);
}

/**
 * Splits a line into the runs that lie inside the box. Each run keeps the
 * neighbouring outside point on either end, so the line still reaches the
 * box edge; neighbouring tiles overlap by one segment and the coast has no
 * gaps at tile boundaries.
 */
function clip(points, box) {
  const inside = ([lon, lat]) =>
    lon >= box.minLon && lon <= box.maxLon && lat >= box.minLat && lat <= box.maxLat;

  const runs = [];
  let run = null;

  for (let i = 0; i < points.length; i++) {
    if (inside(points[i])) {
      if (run === null) run = i > 0 ? [points[i - 1]] : [];
      run.push(points[i]);
    } else if (run !== null) {
      run.push(points[i]);
      runs.push(run);
      run = null;
    }
  }
  if (run !== null) runs.push(run);

  return runs.filter((r) => r.length >= 2);
}

function round(value) {
  const factor = 10 ** DECIMALS;
  return Math.round(value * factor) / factor;
}

/** Tile id for the south-west corner "lat_lon", e.g. "62_6". */
function tileId(lat, lon) {
  return `${lat}_${lon}`;
}

async function main() {
  const inputs = process.argv.slice(2);
  if (inputs.length === 0) {
    console.error("usage: node scripts/build-coastline.mjs <N250Arealdekke.gml>...");
    process.exit(1);
  }

  /** tile id -> lines in that tile */
  const tiles = new Map();
  let sourceVertices = 0;

  for (const path of inputs) {
    let features = 0;

    for await (const numbers of coastlines(path)) {
      features++;
      const utm = toPoints(numbers);
      sourceVertices += utm.length;

      const line = simplify(utm, SIMPLIFY_METRES).map((point) => toWgs84.forward(point));

      let minLat = Infinity, maxLat = -Infinity, minLon = Infinity, maxLon = -Infinity;
      for (const [lon, lat] of line) {
        minLat = Math.min(minLat, lat);
        maxLat = Math.max(maxLat, lat);
        minLon = Math.min(minLon, lon);
        maxLon = Math.max(maxLon, lon);
      }

      // Only the tiles the line's bounding box touches can contain it.
      for (let lat = Math.floor(minLat / TILE_DEGREES) * TILE_DEGREES; lat <= maxLat; lat += TILE_DEGREES) {
        for (let lon = Math.floor(minLon / TILE_DEGREES) * TILE_DEGREES; lon <= maxLon; lon += TILE_DEGREES) {
          const box = { minLat: lat, maxLat: lat + TILE_DEGREES, minLon: lon, maxLon: lon + TILE_DEGREES };
          for (const run of clip(line, box)) {
            const id = tileId(lat, lon);
            if (!tiles.has(id)) tiles.set(id, []);
            tiles.get(id).push(run.map(([x, y]) => [round(x), round(y)]));
          }
        }
      }
    }

    console.log(`${path.split(/[\\/]/).pop()}: ${features} coastline features`);
  }

  const outputDir = new URL("../public/coastline/", import.meta.url);
  rmSync(outputDir, { recursive: true, force: true });
  mkdirSync(outputDir, { recursive: true });

  let totalBytes = 0;
  let largest = { id: "", bytes: 0 };
  let keptVertices = 0;

  for (const [id, lines] of tiles) {
    keptVertices += lines.reduce((sum, line) => sum + line.length, 0);
    const file = new URL(`${id}.geojson`, outputDir);
    writeFileSync(
      file,
      JSON.stringify({
        type: "FeatureCollection",
        features: [{ type: "Feature", properties: {}, geometry: { type: "MultiLineString", coordinates: lines } }],
      }),
    );
    const bytes = statSync(file).size;
    totalBytes += bytes;
    if (bytes > largest.bytes) largest = { id, bytes };
  }

  writeFileSync(
    new URL("index.json", outputDir),
    JSON.stringify({
      source: "N250 Kartdata, Kystkontur",
      attribution: "© Kartverket",
      licence: "CC BY 4.0",
      tileDegrees: TILE_DEGREES,
      tiles: [...tiles.keys()].sort(),
    }),
  );

  console.log(
    `${tiles.size} tiles, ${keptVertices} of ${sourceVertices} vertices kept, ` +
      `${(totalBytes / 1048576).toFixed(1)} MB total, largest ${largest.id} ` +
      `${(largest.bytes / 1024).toFixed(0)} KB`,
  );
}

await main();
