import { createContext, useContext } from "react";

export const Language = createContext(null);
export const localeFor = (lang) => (lang === "id" ? "id-ID" : "en-GB");
export const legacyQuality = {
  menunggu: "waiting",
  berjalan: "active",
  "lengkap-estimasi": "complete_estimated",
  parsial: "partial",
  "tidak-tersedia": "missing",
};
export const qualityOf = (row) =>
  row.quality_code || legacyQuality[row.quality] || row.quality || "invalid";
export const timeOf = (minutes) =>
  `${Math.floor(minutes / 60)
    .toString()
    .padStart(2, "0")}:${(minutes % 60).toString().padStart(2, "0")}`;
export const isNumber = (value) =>
  typeof value === "number" && Number.isFinite(value);
export const useText = () => useContext(Language);
