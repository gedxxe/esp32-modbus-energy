import { useEffect, useMemo, useState } from "react";
import FadeContent from "./vendor/react-bits/FadeContent.jsx";
import UartFlow from "./components/UartFlow.jsx";
import Icon from "./components/Icon.jsx";
import Overview from "./views/Overview.jsx";
import MeterSetup from "./views/MeterSetup.jsx";
import DeviceRecords from "./views/DeviceRecords.jsx";
import useDevice from "./device.js";
import { catalog } from "./i18n.js";
import { Language, localeFor, useText, isNumber } from "./ui.js";

function useReducedMotion() {
  const [reduced, setReduced] = useState(
    () => matchMedia("(prefers-reduced-motion: reduce)").matches,
  );
  useEffect(() => {
    const query = matchMedia("(prefers-reduced-motion: reduce)");
    const update = () => setReduced(query.matches);
    query.addEventListener("change", update);
    return () => query.removeEventListener("change", update);
  }, []);
  return reduced;
}
function Health({ device }) {
  const { t } = useText();
  const s = device.data;
  let key = "connecting";
  if (s)
    key = !device.fresh
      ? "health_stale"
      : s.diagnosticActive
        ? "health_diagnostic"
        : s.scan?.phase === "running"
          ? "health_scan"
          : !s.storage
            ? "health_storage"
            : !s.rtc
              ? "health_rtc"
              : !s.online
                ? "health_offline"
                : "health_ok";
  return (
    <div
      id="health"
      role="status"
      className={`health ${key !== "health_ok" && key !== "connecting" ? "warning" : ""}`}
    >
      <span className="dot" />
      <span>{t(key)}</span>
    </div>
  );
}
export default function Application() {
  const [lang, setLang] = useState(() => {
    try {
      return localStorage.getItem("fort.language") === "id" ? "id" : "en";
    } catch {
      return "en";
    }
  });
  const [view, setView] = useState(() =>
    ["setup", "device"].includes(location.hash.slice(1))
      ? location.hash.slice(1)
      : "overview",
  );
  const device = useDevice();
  const reducedMotion = useReducedMotion();
  const text = useMemo(
    () => ({
      lang,
      t: (key, values = {}) => {
        const template = catalog[lang][key] || catalog.en[key] || key;
        return template.replace(/\{(\w+)\}/g, (_, field) =>
          String(values[field] ?? "—"),
        );
      },
      number: (value, digits = 3) =>
        isNumber(value)
          ? value.toLocaleString(localeFor(lang), {
              minimumFractionDigits: digits,
              maximumFractionDigits: digits,
            })
          : "—",
    }),
    [lang],
  );
  const { t } = text;
  useEffect(() => {
    document.documentElement.lang = lang;
    document.title = `${t("title")} | Midea`;
    try {
      localStorage.setItem("fort.language", lang);
    } catch {}
  }, [lang, text]);
  useEffect(() => {
    const change = () =>
      setView(
        ["setup", "device"].includes(location.hash.slice(1))
          ? location.hash.slice(1)
          : "overview",
      );
    window.addEventListener("hashchange", change);
    return () => window.removeEventListener("hashchange", change);
  }, []);
  const s = device.data;
  const status = !s
    ? "connecting"
    : !device.fresh
      ? "disconnected"
      : s.scan?.phase === "running"
        ? "scanning"
        : s.online
          ? "connected"
          : "offline";
  const content =
    view === "setup" ? (
      <MeterSetup device={device} />
    ) : view === "device" ? (
      <DeviceRecords device={device} />
    ) : (
      <Overview device={device} />
    );
  const navigate = (next) => {
    setView(next);
    history.replaceState(null, "", `#${next}`);
  };
  return (
    <Language.Provider value={text}>
      <header className="topbar">
        <div className="shell topbar-inner">
          <div className="identity">
            <span className="brand-mark">
              <Icon name="meter" size={22} />
            </span>
            <div>
              <div className="brand-name">
                midea <span style={{ fontWeight: 400 }}>engineering</span>
              </div>
              <div className="brand-sub">FORT ENERGY MONITOR</div>
            </div>
          </div>
          <div className="top-actions">
            <div
              id="connection"
              role="status"
              className={`connection ${status === "disconnected" ? "off" : status === "offline" || status === "scanning" ? "warn" : ""}`}
            >
              <span className="dot" />
              <span className="status-text">{t(status)}</span>
            </div>
            <button
              id="refresh"
              className="button text"
              title={lang === "id" ? "Segarkan" : "Refresh"}
              aria-label={lang === "id" ? "Segarkan" : "Refresh"}
              onClick={device.poll}
            >
              <Icon name="refresh" />
            </button>
            <select
              id="language"
              className="language"
              aria-label={t("language")}
              value={lang}
              onChange={(event) => setLang(event.target.value)}
            >
              <option value="en">English</option>
              <option value="id">Indonesia</option>
            </select>
          </div>
        </div>
      </header>
      <main className="shell">
        <div className="page-heading">
          <div>
            <div className="eyebrow">MIDEA / FORT</div>
            <h1>{t("title")}</h1>
            <p>{t("subtitle")}</p>
          </div>
          <div className="device-id mono">
            ESP32-S3 N16R8
            <br />
            192.168.4.1
          </div>
        </div>
        <nav
          className="main-nav"
          aria-label={lang === "id" ? "Halaman" : "Pages"}
        >
          {["overview", "setup", "device"].map((page) => (
            <button
              key={page}
              id={`nav-${page}`}
              aria-current={view === page ? "page" : undefined}
              onClick={() => navigate(page)}
            >
              <Icon name={page} />
              {t(page)}
            </button>
          ))}
        </nav>
        <Health device={device} />
        <UartFlow />
        {reducedMotion ? (
          <div key={view}>{content}</div>
        ) : (
          <FadeContent
            key={view}
            trigger="mount"
            duration={0.3}
            initialOpacity={0.6}
          >
            {content}
          </FadeContent>
        )}
        {device.notice && (
          <div id="message" className="notice" role="status" aria-live="polite">
            {device.notice.key ? t(device.notice.key) : device.notice.text}
            <button
              id="dismiss"
              className="dismiss"
              aria-label={lang === "id" ? "Tutup pesan" : "Dismiss message"}
              onClick={() => device.setNotice(null)}
            >
              ×
            </button>
          </div>
        )}
        <footer>
          <div>
            <span id="version">Firmware {s?.version || "—"}</span> · ESP32-S3 ·
            Fort RTU
            <br />
            {t("footer_note")}
          </div>
          <a href="/assets/third-party.txt" target="_blank" rel="noopener">
            {t("licenses")}
          </a>
        </footer>
      </main>
    </Language.Provider>
  );
}
