/** "62.4722° N, 6.1495° E" */
export function formatLatLon(lat: number, lon: number, digits: number): string {
  const latText = `${Math.abs(lat).toFixed(digits)}° ${lat >= 0 ? "N" : "S"}`;
  const lonText = `${Math.abs(lon).toFixed(digits)}° ${lon >= 0 ? "E" : "W"}`;
  return `${latText}, ${lonText}`;
}

/**
 * How long ago, in the coarsest unit that still says something: "just now",
 * "40 s ago", "3 min ago", "2 h 5 min ago". A negative age -- the browser's
 * clock a little behind the pipeline's -- counts as just now.
 */
export function formatAge(seconds: number): string {
  if (seconds < 10) return "just now";
  if (seconds < 60) return `${Math.floor(seconds)} s ago`;

  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes} min ago`;

  const hours = Math.floor(minutes / 60);
  const rest = minutes % 60;
  return rest === 0 ? `${hours} h ago` : `${hours} h ${rest} min ago`;
}
