import { MapContainer, TileLayer, useMapEvents } from "react-leaflet";
import type { PositionRecord } from "../api/types";
import { ShipMarkers } from "./ShipMarkers";
import { ShipTrack } from "./ShipTrack";

// Without this the tiles load but lay out as an unstyled pile of images.
// Leaflet ships its own CSS and the bundler will not find it on its own.
import "leaflet/dist/leaflet.css";

/** Ålesund, the area PLAN.md targets. */
const ALESUND: [number, number] = [62.4722, 6.1495];

/**
 * Clears the selection when the background is clicked. Leaflet stops click
 * propagation from vector layers, so selecting a ship does not immediately
 * deselect it. Has to live inside MapContainer to reach the map instance.
 */
function DeselectOnBackgroundClick({ onClear }: { onClear: () => void }) {
  useMapEvents({
    click: (event) => {
      // Popup chrome -- the close button above all -- routes its clicks
      // through the map. Dismissing the popup should uncover the track, not
      // clear the selection and remove it.
      const target = event.originalEvent.target;
      if (target instanceof Element && target.closest(".leaflet-popup")) return;

      onClear();
    },
  });
  return null;
}

export interface ShipMapProps {
  records: PositionRecord[];
  now: number;
  selectedMmsi: number | null;
  onSelect: (mmsi: number | null) => void;
  track: PositionRecord[];
}

export function ShipMap({ records, now, selectedMmsi, onSelect, track }: ShipMapProps) {
  return (
    <MapContainer center={ALESUND} zoom={7} className="map" scrollWheelZoom>
      {/* OpenStreetMap's tile usage policy requires this attribution. */}
      <TileLayer
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
        url="https://tile.openstreetmap.org/{z}/{x}/{y}.png"
        maxZoom={19}
      />

      <DeselectOnBackgroundClick onClear={() => onSelect(null)} />

      {/* Under the markers, so a ship is never hidden by its own track. */}
      <ShipTrack records={track} />

      <ShipMarkers
        records={records}
        now={now}
        selectedMmsi={selectedMmsi}
        onSelect={onSelect}
      />
    </MapContainer>
  );
}
