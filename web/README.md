# web — live ship map

A small React + Leaflet frontend for the AIS pipeline. It polls `api_server`
every five seconds, draws every ship with a known position around Ålesund,
fades markers whose last report is stale, and shows the recorded track of a
ship when you click it.

## Run

```bash
npm install
npm run dev
```

Open <http://localhost:5173>. The app expects `api_server` on
`http://localhost:8080`; set `VITE_API_URL` (see `.env.example`) to point it
elsewhere.

## Layout

| Path              | What it does                                                      |
| ----------------- | ----------------------------------------------------------------- |
| `src/api/`        | Typed client for the three REST endpoints, and the record shape.  |
| `src/hooks/`      | Polling hooks: all positions, one ship's history, a shared clock. |
| `src/components/` | The map, the ship markers, and the selected ship's track.         |

`npm run build` type-checks and bundles to `dist/`; `npm run lint` runs Oxlint.
