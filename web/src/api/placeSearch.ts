/**
 * Place-name search against Kartverket's Stedsnavn API. Called straight from
 * the browser; the API allows cross-origin requests and needs no key.
 * https://ws.geonorge.no/stedsnavn/v1/
 */

const ENDPOINT = "https://ws.geonorge.no/stedsnavn/v1/navn";

export interface Place {
  name: string;
  /** Kartverket's object type, in Norwegian: "By", "Fjord", "Øy i sjø", ... */
  kind: string;
  municipality: string | null;
  lat: number;
  lon: number;
}

/** The parts of a Stedsnavn result this app reads. */
interface StedsnavnResponse {
  navn?: {
    skrivemåte: string;
    navneobjekttype: string;
    kommuner?: { kommunenavn: string }[];
    representasjonspunkt: { nord: number; øst: number };
  }[];
}

export async function searchPlaces(query: string, signal?: AbortSignal): Promise<Place[]> {
  const params = new URLSearchParams({
    // Prefix match. The API's fuzzy mode is too loose to be useful here:
    // "Brei" returns hiking paths in Agder before Breisundet.
    sok: `${query.trim()}*`,
    // EUREF89 lat/lon, which matches WGS84 to well under a metre.
    utkoordsys: "4258",
    treffPerSide: "8",
    side: "1",
  });

  const response = await fetch(`${ENDPOINT}?${params}`, { signal });
  if (!response.ok) throw new Error(`place search returned ${response.status}`);

  const body = (await response.json()) as StedsnavnResponse;

  return (body.navn ?? []).map((result) => ({
    name: result.skrivemåte,
    kind: result.navneobjekttype,
    municipality: result.kommuner?.[0]?.kommunenavn ?? null,
    lat: result.representasjonspunkt.nord,
    lon: result.representasjonspunkt.øst,
  }));
}
