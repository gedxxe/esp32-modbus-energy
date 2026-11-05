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
    wifi: "M2 8a16 16 0 0 1 20 0M5 12a11 11 0 0 1 14 0M8 16a6 6 0 0 1 8 0M12 20h.01",
    radio: "M8 8a6 6 0 0 0 0 8m8-8a6 6 0 0 1 0 8M4 4a12 12 0 0 0 0 16M20 4a12 12 0 0 1 0 16M12 10v4m0 3v4",
    display: "M3 4h18v13H3V4Zm5 17h8m-4-4v4M7 8h10M7 12h6",
    shield: "M12 3 4 6v6c0 5 8 9 8 9s8-4 8-9V6l-8-3Zm-4 9 3 3 5-6",
    storage: "M4 4h16v16H4V4Zm4 0v6h8V4M8 20v-6h8v6",
    info: "M12 8h.01M12 11v6M22 12a10 10 0 1 1-20 0 10 10 0 0 1 20 0",
    warning: "m12 3 10 18H2L12 3Zm0 6v5m0 3h.01",
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
