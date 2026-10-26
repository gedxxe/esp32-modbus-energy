export default function Icon({ name, size = 16 }) {
  const paths = {
    meter: "M3 17V7h18v10H3Zm4-4 3-3 3 3 4-5M7 20h10",
    overview: "M4 20V10m8 10V4m8 16v-7",
    setup: "M4 7h16M4 17h16M8 4v6m8 4v6",
    device: "M5 4h14v16H5V4Zm4 4h6m-6 4h6m-6 4h2",
    download: "M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5",
    refresh: "M20 10a8 8 0 1 0-2 8M20 4v6h-6",
    arrow: "M4 12h16m-6-6 6 6-6 6",
    scan: "M8 3H3v5m13-5h5v5M3 16v5h5m13-5v5h-5M7 12h10",
  };
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d={paths[name] || paths.meter} />
    </svg>
  );
}
