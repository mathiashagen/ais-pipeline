# web — live ship map

A small React + Leaflet frontend for the AIS pipeline. It polls `api_server`
every five seconds, draws every ship with a known position within range of a
chosen centre, fades markers whose last report is stale, and shows a ship's
details and recorded track when you click it. Escape closes the details.

## Run

```bash
npm install
npm run dev
```

Open <http://localhost:5173>. The app expects `api_server` on
`http://localhost:8080`; set `VITE_API_URL` (see `.env.example`) to point it
elsewhere.

To work on the frontend without running the pipeline locally, point it at a
deployed stack in `.env.local`, for example
`VITE_API_URL=http://192.168.1.140:8080/api`. Kystverket's feed accepts one
connection per IP address, so a second pipeline on the same network would
keep disconnecting the deployed one.

## Layout

| Path              | What it does                                                      |
| ----------------- | ----------------------------------------------------------------- |
| `src/api/`        | Typed client for the three REST endpoints, and the record shape.  |
| `src/hooks/`      | Polling hooks: all positions, one ship's history, a shared clock. |
| `src/components/` | The map, the ship markers, the selected ship's panel and track.   |

`npm run build` type-checks and bundles to `dist/`; `npm run lint` runs Oxlint.
