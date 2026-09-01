import { MapContainer, TileLayer } from "react-leaflet";
import type { PositionRecord } from "../api/types";
import { ShipMarkers } from "./ShipMarkers";

// Without this the tiles load but lay out as an unstyled pile of images.
// Leaflet ships its own CSS and the bundler will not find it on its own.
import "leaflet/dist/leaflet.css";

/** Ålesund, the area PLAN.md targets. */
const ALESUND: [number, number] = [62.4722, 6.1495];

export function ShipMap({ records, now }: { records: PositionRecord[]; now: number }) {
  return (
    <MapContainer center={ALESUND} zoom={7} className="map" scrollWheelZoom>
      {/* OpenStreetMap's tile usage policy requires this attribution. */}
      <TileLayer
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
        url="https://tile.openstreetmap.org/{z}/{x}/{y}.png"
        maxZoom={19}
      />
      <ShipMarkers records={records} now={now} />
    </MapContainer>
  );
}
