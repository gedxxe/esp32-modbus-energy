import { meterProfile, addressHex } from "../meter-profile.js";
import CheckpointRecords from "../components/CheckpointRecords.jsx";
import EnergyCounter from "../components/EnergyCounter.jsx";
import DeviceClock from "../components/DeviceClock.jsx";
import { useState } from "react";
import SpotlightCard from "../vendor/react-bits/SpotlightCard.jsx";
import Icon from "../components/Icon.jsx";
import { useText, isNumber, qualityOf, timeOf } from "../ui.js";

function IntervalChart({ rows }) {
  const { t, number } = useText();
  const known = rows.filter((row) => isNumber(row.kwh));
  const max = Math.max(1, ...known.map((row) => row.kwh));
  const width = 400,
    left = 32,
    plot = 350,
    baseline = 156,
    height = 118;
  const step = plot / rows.length,
    barWidth = Math.min(33, step * 0.5);
  const completed = rows.filter(
    (row) => qualityOf(row) === "complete_estimated" && isNumber(row.kwh),
  );
  const total = completed.length
    ? completed.reduce((sum, row) => sum + row.kwh, 0)
    : null;
  return (
    <div className="chart-panel">
      <div className="chart-heading">
        <div>
          {t("completed_total")}
          <div className="shift-total">
            {number(total)} <small>kWh</small>
          </div>
        </div>
        <span className="mono">kWh / 30 min</span>
      </div>
      <svg
        className="chart"
        viewBox={`0 0 ${width} 196`}
        role="img"
        aria-label={t("chart_label")}
      >
        {[0, 0.5, 1].map((fraction) => (
          <g key={fraction}>
            <line
              className="gridline"
              x1={left}
              x2={width - 9}
              y1={baseline - height * fraction}
              y2={baseline - height * fraction}
            />
            <text x="0" y={baseline - height * fraction + 4}>
              {number(max * fraction, 0)}
            </text>
          </g>
        ))}
        {rows.map((row, i) => {
          const knownValue = isNumber(row.kwh);
          const barHeight = knownValue
            ? Math.max(2, (row.kwh / max) * height)
            : 5;
          const x = left + i * step + step / 2;
          const quality = qualityOf(row);
          return (
            <g key={row.start}>
              <title>
                {timeOf(row.start)}:{" "}
                {knownValue
                  ? number(row.kwh) + " kWh"
                  : t(`quality_${quality}`)}
              </title>
              <rect
                className={
                  !knownValue
                    ? "bar-missing"
                    : quality === "complete_estimated"
                      ? "bar-fill"
                      : "bar-active"
                }
                x={x - barWidth / 2}
                y={baseline - barHeight}
                width={barWidth}
                height={barHeight}
                rx="3"
              />
              <text x={x} y="180" textAnchor="middle">
                {timeOf(row.start)}
              </text>
            </g>
          );
        })}
        {!known.length && (
          <text x="212" y="91" textAnchor="middle">
            {t("no_intervals")}
          </text>
        )}
      </svg>
      <div className="legend">
        <span>
          <i />
          {t("complete_legend")}
        </span>
        <span>
          <i className="estimated" />
          {t("active_legend")}
        </span>
      </div>
    </div>
  );
}

export default function Overview({ device }) {
  const { t, number } = useText();
  const [shift, setShift] = useState(0);
  const s = device.data;
  const profile=meterProfile(s);
  const rows = s?.slots.filter((_, i) => (shift === 0 ? i < 6 : i >= 6)) || [];
  const live = device.fresh && s?.online;
  const energyFresh = live && s?.ratios && s?.energy != null && s?.age <= 15;
  return (
    <>
      <div className="summary">
        <SpotlightCard
          className="energy-surface"
          theme="dark"
          spotlightColor="#b4efe2"
          intensity={0.12}
          borderGlow={0.3}
          ambient={false}
          flare={false}
          proximity={0}
          style={{
            "--spotlight-card-surface": "#103d40",
            "--spotlight-card-border": "#42676a",
          }}
        >
          <div className="summary-top">
            <span className="label">{t("energy")}</span>
            <span className="live-stamp">
              <span className="dot" />
              {energyFresh ? "MODBUS RTU" : t("counter")}
            </span>
          </div>
          <div className="reading">
            <EnergyCounter value={s?.energy} />
            <span className="unit">kWh</span>
          </div>
          <div className="summary-bottom">
            <span id="age">
              {s?.age == null
                ? t("no_sample")
                : t("sample", { seconds: s.age })}
            </span>
            <span className="path-label">
              {profile.name} / {String(s?.slave || 1).padStart(2, "0")}
            </span>
          </div>
        </SpotlightCard>
        <div className="secondary-readings">
          <div>
            <div className="secondary-label">
              {t("power")}
              <div className="secondary-number">
                <span id="power">{live ? number(s?.power, 2) : "—"}</span>
                <small>kW</small>
              </div>
            </div>
            <Icon name="overview" size={21} />
          </div>
          <div>
            <div className="secondary-label">
              {t("rtc_clock")}
              <div className="secondary-number mono" id="clock">
                <DeviceClock device={device} />
              </div>
            </div>
            <span className="small muted" id="date">
              {s?.date || t("date_unset")}
            </span>
          </div>
        </div>
      </div>
      <CheckpointRecords device={device} />
      <section className="panel register-summary" aria-labelledby="register-title">
        <div><h2 id="register-title">{t("register_title")}</h2><p>{t("register_intro")}</p></div>
        <dl className="register-values">
          {[["current_a", "currentA", "A"], ["voltage_a", "voltageA", "V"], ["frequency", "frequency", "Hz"], ["secondary_energy", "secondaryEnergy", "kWh"]].map(([label, key, unit, address]) => (
            <div key={key}><dt>{t(label)} <code>0x{addressHex(profile[key].address)}</code></dt><dd id={`value-${key}`}>{live ? number(s?.[key], key === "frequency" ? 2 : 3) : "—"} <small>{unit}</small>{key === "secondaryEnergy" && s?.secondaryAge != null && <div className="small muted">{t("sample", { seconds: s.secondaryAge })}</div>}</dd></div>
          ))}
        </dl><p className="small muted">{t("register_note")}</p>
      </section>
      <details id="legacy-intervals" className="legacy-intervals"><summary>{t("cp_legacy")}</summary>
      <section aria-labelledby="interval-title">
        <div className="section-heading">
          <div>
            <h2 id="interval-title">{t("intervals")}</h2>
            <p>{t("interval_sub")}</p>
          </div>
          <div className="segmented">
            {[0, 1].map((value) => (
              <button
                key={value}
                data-shift={value}
                aria-pressed={shift === value}
                onClick={() => setShift(value)}
              >
                {t(value ? "evening" : "morning")}
              </button>
            ))}
          </div>
        </div>
        <div className="interval-panel">
          <div className="interval-grid">
            <IntervalChart rows={rows} />
            <table className="data-table">
              <thead>
                <tr>
                  <th>{t("interval")}</th>
                  <th className="amount">{t("consumption")}</th>
                  <th>{t("quality")}</th>
                </tr>
              </thead>
              <tbody id="intervals">
                {rows.map((row) => (
                  <tr key={row.start}>
                    <td>
                      {timeOf(row.start)} – {timeOf(row.start + 30)}
                    </td>
                    <td className="amount">{number(row.kwh)}</td>
                    <td>
                      <span className={`quality ${qualityOf(row)}`}>
                        {t(`quality_${qualityOf(row)}`)}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <div className="panel-foot">
            <p>{t("interval_note")}</p>
            <a className="button" href="/api/export">
              <Icon name="download" />
              {t("export_today")}
            </a>
          </div>
        </div>
      </section>
      </details>
      <div className="status-strip">
        <span>
          {t("rtc")}
          <b className={s?.rtc ? "good" : "bad"}>
            {t(s?.rtc ? "ok" : "attention")}
          </b>
        </span>
        <span>
          {t("storage")}
          <b className={s?.storage ? "good" : "bad"}>
            {t(s?.storage ? "ok" : "attention")}
          </b>
        </span>
        <span>
          {t("ratio")}
          <b>{s?.ratios ? number(s.ratio, 0) : "—"}</b>
        </span>
        <span className="mono">
          ID {s?.slave || "—"} / {s?.baud || "—"} / 8{s?.parity || "N"}1
        </span>
        <span className="mono">{s?.dayDate || "—"}</span>
      </div>
    </>
  );
}
