import { useEffect, useState } from "react";
import DiagnosticConsole from "../components/DiagnosticConsole.jsx";
import RawTrace from "../components/RawTrace.jsx";
import Icon from "../components/Icon.jsx";
import { useText } from "../ui.js";

export default function MeterSetup({ device }) {
  const { t } = useText();
  const s = device.data;
  const scan = s?.scan || { phase: "idle", tested: 0, total: 0 };
  const running = scan.phase === "running";
  const blocked =
    device.pending || !device.fresh || Boolean(s?.diagnosticActive);
  const [config, setConfig] = useState({ slave: 1, baud: 9600, parity: "N" });
  const [probe, setProbe] = useState({ address: "0034", words: 1 });
  const [scanMode, setScanMode] = useState("current");
  const [range, setRange] = useState({ first: 1, last: 16 });
  useEffect(() => {
    if (s) setConfig({ slave: s.slave, baud: s.baud, parity: s.parity });
  }, [s?.slave, s?.baud, s?.parity]);
  useEffect(() => {
    if (running) {
      setRange({ first: scan.first, last: scan.last });
      if (scan.mode) setScanMode(scan.mode);
    }
  }, [running, scan.first, scan.last]);
  useEffect(() => {
    if (
      scan.phase !== "idle" &&
      Number.isInteger(scan.address) &&
      Number.isInteger(scan.words)
    )
      setProbe({
        address: scan.address.toString(16).padStart(4, "0"),
        words: scan.words,
      });
  }, [scan.phase, scan.address, scan.words]);
  const applied =
    scan.result &&
    s?.slave === scan.result.slave &&
    s?.baud === scan.result.baud &&
    s?.parity === scan.result.parity;
  const candidate =
    scan.phase === "found" ? scan.result : running ? scan : null;
  const elapsed = `${Math.floor((scan.elapsed || 0) / 60)}:${String((scan.elapsed || 0) % 60).padStart(2, "0")}`;
  const scanMessage =
    scan.phase === "found"
      ? applied
        ? t("scan_applied")
        : t("scan_found", scan.result)
      : scan.phase === "running"
        ? t("scan_running", scan)
        : scan.phase === "cancelled"
          ? t("scan_cancelled")
          : scan.phase === "exhausted"
            ? t("scan_exhausted", scan)
            : t("scan_idle");
  const submitScan = (event) => {
    event.preventDefault();
    const first = Number(range.first),
      last = Number(range.last);
    if (
      !Number.isInteger(first) ||
      !Number.isInteger(last) ||
      first < 1 ||
      last > 247 ||
      first > last
    ) {
      device.setNotice({ key: "invalid_scan_range" });
      return;
    }
    const address = /^[0-9a-fA-F]{1,4}$/.test(probe.address)
      ? parseInt(probe.address, 16)
      : NaN;
    const words = Number(probe.words);
    if (
      !Number.isInteger(address) ||
      !Number.isInteger(words) ||
      words < 1 ||
      words > 3 ||
      address + words > 65536
    ) {
      device.setNotice({ key: "invalid_diagnostic_command" });
      return;
    }
    device.act("/api/scan/start", {
      first,
      last,
      address,
      words,
      mode: scanMode,
    });
  };
  return (
    <>
      <div className="view-grid">
        <section className="panel" aria-labelledby="connection-title">
          <h2 id="connection-title">{t("manual_title")}</h2>
          <p>{t("manual_intro")}</p>
          <form
            id="config"
            onSubmit={(event) => {
              event.preventDefault();
              device.act("/api/config", config);
            }}
          >
            <div className="form-grid">
              <div>
                <label htmlFor="slave">{t("slave")}</label>
                <input
                  id="slave"
                  type="number"
                  min="1"
                  max="247"
                  required
                  value={config.slave}
                  disabled={blocked || running}
                  onChange={(event) =>
                    setConfig({ ...config, slave: event.target.value })
                  }
                />
              </div>
              <div>
                <label htmlFor="baud">{t("baud")}</label>
                <select
                  id="baud"
                  value={config.baud}
                  disabled={blocked || running}
                  onChange={(event) =>
                    setConfig({ ...config, baud: event.target.value })
                  }
                >
                  {[1200, 2400, 4800, 9600, 19200, 38400].map((value) => (
                    <option key={value} value={value}>
                      {value}
                    </option>
                  ))}
                </select>
              </div>
              <div className="wide">
                <label htmlFor="parity">{t("parity")}</label>
                <select
                  id="parity"
                  value={config.parity}
                  disabled={blocked || running}
                  onChange={(event) =>
                    setConfig({ ...config, parity: event.target.value })
                  }
                >
                  {[
                    ["N", "none"],
                    ["E", "even"],
                    ["O", "odd"],
                  ].map(([value, key]) => (
                    <option key={value} value={value}>
                      {t(key)}
                    </option>
                  ))}
                </select>
              </div>
            </div>
            <div className="actions">
              <button
                className="button primary"
                type="submit"
                disabled={blocked || running}
              >
                {t("save")}
                <Icon name="arrow" />
              </button>
            </div>
          </form>
          <div className="hint">{t("manual_note")}</div>
        </section>
        <section className="panel" id="scan-panel" aria-labelledby="scan-title">
          <h2 id="scan-title">{t("scan_title")}</h2>
          <p>{t("scan_intro")}</p>
          <form id="scanform" onSubmit={submitScan}>
            <div className="form-grid">
              <div>
                <label htmlFor="scanfirst">{t("first_id")}</label>
                <input
                  id="scanfirst"
                  type="number"
                  min="1"
                  max="247"
                  required
                  disabled={blocked || running}
                  value={range.first}
                  onChange={(event) =>
                    setRange({ ...range, first: event.target.value })
                  }
                />
              </div>
              <div>
                <label htmlFor="scanlast">{t("last_id")}</label>
                <input
                  id="scanlast"
                  type="number"
                  min="1"
                  max="247"
                  required
                  disabled={blocked || running}
                  value={range.last}
                  onChange={(event) =>
                    setRange({ ...range, last: event.target.value })
                  }
                />
              </div>
            </div>
            <div className="form-grid">
              <div>
                <label htmlFor="scan-probe">{t("scan_probe")}</label>
                <input
                  id="scan-probe"
                  value={probe.address}
                  maxLength="4"
                  pattern="[0-9a-fA-F]{1,4}"
                  required
                  disabled={blocked || running}
                  onChange={(event) =>
                    setProbe({ ...probe, address: event.target.value })
                  }
                />
              </div>
              <div>
                <label htmlFor="scan-words">{t("scan_words")}</label>
                <select
                  id="scan-words"
                  value={probe.words}
                  disabled={blocked || running}
                  onChange={(event) =>
                    setProbe({ ...probe, words: event.target.value })
                  }
                >
                  {[1, 2, 3].map((n) => (
                    <option key={n}>{n}</option>
                  ))}
                </select>
              </div>
            </div>
            <div className="range-tools">
              <span>{t("quick_range")}</span>
              <button
                type="button"
                disabled={blocked || running}
                onClick={() => setRange({ first: 1, last: 16 })}
              >
                1–16
              </button>
              <button
                type="button"
                disabled={blocked || running}
                onClick={() => setRange({ first: 1, last: 247 })}
              >
                {t("all_ids")}
              </button>
            </div>
            <label htmlFor="scan-mode">{t("scan_formats")}</label>
            <select
              id="scan-mode"
              value={scanMode}
              disabled={blocked || running}
              onChange={(event) => setScanMode(event.target.value)}
            >
              <option value="current">{t("scan_current")}</option>
              <option value="matrix">{t("scan_matrix")}</option>
            </select>
            <p className="small muted">
              {t(
                scanMode === "current" ? "scan_current_limits" : "scan_limits",
              )}
            </p>
            <div className="actions">
              <button
                id="scanstart"
                className="button primary"
                disabled={blocked || running}
                type="submit"
              >
                <Icon name="scan" />
                {t("scan_start")}
              </button>
              <button
                id="scancancel"
                className="button danger"
                disabled={blocked || !running}
                type="button"
                onClick={() => device.act("/api/scan/cancel")}
              >
                {t("scan_cancel")}
              </button>
              <button
                id="scanapply"
                className="button"
                disabled={blocked || scan.phase !== "found" || applied}
                type="button"
                onClick={() => device.act("/api/scan/apply")}
              >
                {t("scan_apply")}
              </button>
            </div>
          </form>
          <div className="scan-candidate">
            <div>
              {t("candidate")}
              <strong>{candidate ? `ID ${candidate.slave}` : "—"}</strong>
            </div>
            <div>
              {t("baud")}
              <strong>{candidate?.baud || "—"}</strong>
            </div>
            <div>
              {t("serial_format")}
              <strong>{candidate ? `8${candidate.parity}1` : "—"}</strong>
            </div>
          </div>
          <progress
            id="scanprogress"
            aria-label={t("progress")}
            value={scan.tested || 0}
            max={scan.total || 1}
          />
          <div className="scan-meta">
            <span className="mono">
              {scan.tested || 0} / {scan.total || 0}
            </span>
            <span>{t("scan_elapsed", { time: elapsed })}</span>
          </div>
          <p
            className="scan-status"
            id="scanstatus"
            role="status"
            aria-live="polite"
          >
            {scanMessage}
          </p>
          {scan.phase !== "idle" && (
            <p className="small muted mono">
              {t("scan_counts", {
                timeouts: scan.timeouts || 0,
                crc: scan.crcErrors || 0,
                rejected: scan.rejected || 0,
              })}
            </p>
          )}
          {scan.phase === "found" && scan.exception ? (
            <p className="hint">{t("scan_exception")}</p>
          ) : null}
          <div className="hint">{t("scan_pause_note")}</div>
        </section>
      </div>
      <DiagnosticConsole device={device} />
      <RawTrace device={device} />
      <details className="wiring-help">
        <summary>{t("wiring_title")}</summary>
        <p className="small muted">{t("wiring_auto")}</p>
        <div className="pins">
          <code>TXD → GPIO18 / RX</code>
          <code>RXD ← GPIO17 / TX</code>
          <code>GPIO16: unconnected</code>
          <code>A → Fort 58 / B → Fort 59</code>
        </div>
        <p className="small muted" style={{ marginTop: 12 }}>
          {t("wiring_note")}
        </p>
        <p className="small muted">{t("wiring_direction")}</p>
      </details>
    </>
  );
}
