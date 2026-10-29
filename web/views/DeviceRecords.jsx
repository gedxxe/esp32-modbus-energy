import { deviceFetch } from "../request.js";
import DeviceClock from "../components/DeviceClock.jsx";
import { useEffect, useState } from "react";
import ProcessMonitor from "../components/ProcessMonitor.jsx";
import DisplayControls from "../components/DisplayControls.jsx";
import NetworkControls from "../components/NetworkControls.jsx";
import LedControls from "../components/LedControls.jsx";
import Icon from "../components/Icon.jsx";
import { useText } from "../ui.js";
import { validHistory } from "../validation.js";

export default function DeviceRecords({ device }) {
  const { t, number } = useText();
  const s = device.data;
  const [history, setHistory] = useState([]);
  const [selected, setSelected] = useState("");
  const [historyError, setHistoryError] = useState(false);
  useEffect(() => {
    let alive = true;
    let controller;
    const load = async () => {
      controller = new AbortController();

      try {
        const response = await deviceFetch("/api/history", {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error("history");
        const rows = await response.json();
        if (!validHistory(rows)) throw new Error("history");
        if (alive) {
          setHistory(rows);
          setHistoryError(false);
          setSelected((value) =>
            rows.some((row) => String(row.day) === value)
              ? value
              : String(rows[0]?.day || ""),
          );
        }
      } catch {
        if (alive) setHistoryError(true);
      }
    };
    load();
    const timer = setInterval(load, 60000);
    return () => {
      alive = false;
      clearInterval(timer);
      controller?.abort();
    };
  }, []);
  const blocked = device.pending || !device.fresh;
  const stats = [
    ["crc_errors", s?.crcErrors],
    ["timeouts", s?.timeouts],
    ["rejected", s?.rejected],
    ["heap", s ? `${number(s.heap / 1024, 0)} KB` : null],
    ["ratio_label", s?.ratios ? number(s.ratio) : null],
    [
      "uptime",
      s
        ? `${Math.floor(s.uptime / 3600)}h ${Math.floor(s.uptime / 60) % 60}m`
        : null,
    ],
  ];
  return (
    <div className="device-columns">
      <section className="panel">
        <h2>{t("diagnostics")}</h2>
        <dl className="metrics">
          {stats.map(([key, value]) => (
            <div key={key}>
              <dt>{t(key)}</dt>
              <dd>{value ?? "—"}</dd>
            </div>
          ))}
        </dl>
        <div className="error-detail">
          {t("last_error")}
          <code>{s?.error_code ? t(s.error_code) : s?.error || "—"}</code>
        </div>
        <div className="actions">
          <button
            id="mute"
            className="button"
            disabled={blocked}
            onClick={() => device.act("/api/mute")}
          >
            {t(s?.muted ? "unmute" : "mute")}
          </button>
        </div>
        <div className="hint">
          <h3>{t("format_help")}</h3>
          <p style={{ marginTop: 8 }}>{t("format_note")}</p>
        </div>
      </section>
      <div className="device-right">
        <NetworkControls device={device} />
        <DisplayControls device={device} />
        <LedControls device={device} />
        <section className="panel">
          <h2>{t("clock_title")}</h2>
          <p>{t("clock_intro")}</p>
          <div className="secondary-number mono">
            <DeviceClock device={device} /> <small>WIB</small>
          </div>
          <div className="actions">
            <button
              id="sync"
              className="button primary"
              disabled={blocked || Boolean(s?.diagnosticActive||s?.discoveryRunning)}
              onClick={() =>
                device.act("/api/time", {
                  epoch: Math.floor(Date.now() / 1000),
                })
              }
            >
              {t("sync")}
            </button>
          </div>
          <div className="hint">{t("clock_warning")}</div>
        </section>
        <section className="panel">
          <h2>{t("records")}</h2>
          <p>{t("records_intro")}</p>
          <label htmlFor="history">{t("archive_date")}</label>
          <select
            id="history"
            value={selected}
            disabled={!history.length || !device.fresh}
            onChange={(event) => setSelected(event.target.value)}
          >
            {!history.length && <option value="">{t("no_archives")}</option>}
            {history.map((row) => (
              <option key={row.day} value={row.day}>
                {row.date}
              </option>
            ))}
          </select>
          <div className="actions">
            <button
              id="download"
              className="button"
              disabled={!selected || !device.fresh}
              onClick={() => {
                location.href = `/api/export?day=${encodeURIComponent(selected)}`;
              }}
            >
              <Icon name="download" />
              {t("download_archive")}
            </button>
          </div>
          {historyError && <p className="hint">{t("history_failed")}</p>}
        </section>
      </div>
      <ProcessMonitor />
    </div>
  );
}
