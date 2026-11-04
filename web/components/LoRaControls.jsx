import { useEffect, useRef, useState } from "react";
import { validLoRaView } from "../validation.js";
import { validateFormFields } from "../form-validation.js";
import { useText } from "../ui.js";

const initial = {
  enabled: false, frequencyHz: 915000000, bandwidthKHz: 125, sf: 7, cr: 5,
  powerDbm: 14, preamble: 8, syncWord: 18, nodeId: 2, gatewayId: 1,
  networkId: 1, intervalMs: 60000, ackMs: 10000, retries: 2,
};
const fields = Object.keys(initial);

export default function LoRaControls({ device }) {
  const { t, number } = useText();
  const v = validLoRaView(device.data?.lora) ? device.data.lora : null;
  const [draft, setDraft] = useState(initial);
  const [key, setKey] = useState("");
  const [keepKey, setKeepKey] = useState(false);
  const [showKey, setShowKey] = useState(false);
  const [feedback, setFeedback] = useState(null);
  const signature = useRef("");
  useEffect(() => {
    if (!v?.ready) return;
    const next = JSON.stringify([...fields.map(f => v[f]), v.hasKey, v.stored, v.revision]);
    if (signature.current === next) return;
    signature.current = next;
    setDraft(Object.fromEntries(fields.map(f => [f, v[f]])));
    setKeepKey(v.hasKey);
    setKey("");
    setShowKey(false);
  }, [v]);

  const blocked = !v?.ready || !device.fresh || device.pending;
  const dirty = Boolean(v && (fields.some(f => draft[f] !== v[f]) || key || keepKey !== v.hasKey));
  const actionBlocked = blocked || dirty || !v?.enabled || !v?.available;
  const set = (name, value) => setDraft(d => ({ ...d, [name]: value }));
  function explain(code, values = {}) {
    setFeedback({ key: code, values });
    device.setNotice(Object.keys(values).length ? { text: t(code, values) } : { key: code });
  }
  function generate() {
    const bytes = new Uint8Array(32);
    crypto.getRandomValues(bytes);
    setKey(Array.from(bytes, x => x.toString(16).padStart(2, "0")).join(""));
    setKeepKey(false);
    setShowKey(true);
    setFeedback(null);
  }
  async function save(event) {
    event.preventDefault();
    if (blocked) { explain("lora_unavailable"); return; }
    if (!validateFormFields(event.currentTarget, t, explain)) return;
    if (draft.nodeId === draft.gatewayId) { explain("invalid_lora"); return; }
    if ((key && (!/^[0-9a-f]{64}$/i.test(key) || /^0{64}$/.test(key))) ||
        (draft.enabled && !keepKey && !/^[0-9a-f]{64}$/i.test(key))) {
      explain("invalid_lora_key"); return;
    }
    setFeedback({ key: "lora_working" });
    const result = await device.act("/api/lora/config", {
      ...draft, revision: v.revision, enabled: draft.enabled ? 1 : 0,
      key: keepKey ? "" : key, keepKey: keepKey ? 1 : 0,
    });
    if (result?.ok) { setKey(""); setShowKey(false); explain("lora_saved"); }
    else setFeedback({ key: result?.error || result?.body?.code || "request_failed" });
  }
  async function command(action) {
    if (dirty) { explain("lora_save_first"); return; }
    if (actionBlocked) { explain("lora_unavailable"); return; }
    setFeedback({ key: "lora_working" });
    const result = await device.act("/api/lora/control", { action });
    setFeedback({ key: result?.ok ? "lora_control_queued" : result?.error || result?.body?.code || "request_failed" });
  }
  function numericField(name, min, max) {
    return <label className="lora-field" key={name} htmlFor={`lora-${name}`}>
      <span>{t(`lora_${name}`)}</span>
      <input id={`lora-${name}`} type="number" min={min} max={max} step="1"
        value={draft[name]} required disabled={blocked}
        onChange={e => set(name, e.target.value === "" ? "" : Number(e.target.value))} />
    </label>;
  }
  function secondsField(name, id, label, min, max) {
    return <label className="lora-field" htmlFor={id}>
      <span>{t(label)}</span><input id={id} type="number" min={min} max={max} step="1"
        required disabled={blocked} value={draft[name] === "" ? "" : draft[name] / 1000}
        onChange={e => set(name, e.target.value === "" ? "" : Number(e.target.value) * 1000)} />
    </label>;
  }
  return <section className="panel" id="lora-panel" aria-labelledby="lora-title">
    <h2 id="lora-title">{t("lora_title")}</h2><p>{t("lora_intro")}</p>
    <div className="network-state" role="status"><strong id="lora-status">{v ? t(`lora_phase_${v.phase}`) : t("lora_unavailable")}</strong><span>{v?.paired ? t("lora_paired") : t("lora_unpaired")}</span></div>
    {v?.code !== 0 && v?.code !== undefined && <p className="hint" id="lora-error">{t("lora_driver_error", { code: v.code })}</p>}
    <dl className="metrics">
      <div><dt>TX / RX / ACK</dt><dd>{v ? `${v.tx} / ${v.rx} / ${v.acks}` : "—"}</dd></div>
      <div><dt>{t("lora_failures")}</dt><dd>{v ? `${v.timeouts} / ${v.rejected} / ${v.crcErrors}` : "—"}</dd></div>
      <div><dt>{t("lora_auth_counts")}</dt><dd>{v ? `${v.authErrors} / ${v.addressErrors} / ${v.replayErrors}` : "—"}</dd></div>
      <div><dt>{t("lora_sequences")}</dt><dd>{v ? `${v.lastTxSequence} / ${v.lastRxSequence}` : "—"}</dd></div>
      <div><dt>RSSI / SNR</dt><dd>{v?.lastRxAgeMs ? `${number(v.rssi, 1)} dBm / ${number(v.snr, 1)} dB` : "—"}</dd></div>
      <div><dt>{t("lora_airtime")}</dt><dd>{v ? `${v.airtimeMs} ms / ${Math.ceil(v.waitMs / 1000)} s` : "—"}</dd></div>
    </dl>
    <form id="lora-form" onSubmit={save} onChange={() => setFeedback(null)} noValidate>
      <label className="checkbox-line lora-toggle"><input id="lora-enabled" type="checkbox" disabled={blocked} checked={draft.enabled} onChange={e => set("enabled", e.target.checked)} /><span>{t("lora_enable")}</span></label>
      <label className="lora-field" htmlFor="lora-frequency"><span>{t("lora_frequency")}</span>
        <input id="lora-frequency" type="number" min="803" max="930" step="0.001" required disabled={blocked}
          value={draft.frequencyHz === "" ? "" : draft.frequencyHz / 1e6}
          onChange={e => set("frequencyHz", e.target.value === "" ? "" : Math.round(Number(e.target.value) * 1e6))} />
      </label>
      <fieldset className="lora-section"><legend>{t("lora_addresses")}</legend>
        <div className="lora-grid">{[["nodeId", 1, 65534], ["gatewayId", 1, 65534], ["networkId", 1, 65535]].map(args => numericField(...args))}</div>
        <p className="small muted">{t("lora_ids_note")}</p><p className="hint" id="lora-ptmp-note">{t("lora_ptmp_note")}</p>
      </fieldset>
      <fieldset className="lora-section"><legend>{t("lora_pairing_key")}</legend>
        {v?.hasKey && <label className="checkbox-line lora-toggle"><input id="lora-keep-key" type="checkbox" checked={keepKey} disabled={blocked} onChange={e => { setKeepKey(e.target.checked); setKey(""); }} /><span>{t("lora_keep_key")}</span></label>}
        {!keepKey && <>
          <label className="lora-field" htmlFor="lora-key"><span>{t("lora_key")}</span><input id="lora-key" type={showKey ? "text" : "password"} autoComplete="new-password" spellCheck="false" maxLength={64} value={key} disabled={blocked} required={draft.enabled} onChange={e => setKey(e.target.value)} /></label>
          <div className="lora-key-actions"><button id="lora-generate" type="button" className="button" disabled={blocked} onClick={generate}>{t("lora_generate")}</button><button type="button" className="button" disabled={blocked} onClick={() => setShowKey(!showKey)}>{t(showKey ? "lora_hide_key" : "lora_show_key")}</button></div>
          <p className="small muted">{t("lora_key_note")}</p>
        </>}
      </fieldset>
      <details id="lora-advanced"><summary>{t("lora_advanced")}</summary><div className="lora-grid">
        <label className="lora-field" htmlFor="lora-bandwidthKHz"><span>{t("lora_bandwidth")}</span><select id="lora-bandwidthKHz" value={draft.bandwidthKHz} disabled={blocked} onChange={e => set("bandwidthKHz", Number(e.target.value))}>{[125, 250, 500].map(n => <option key={n} value={n}>{n} kHz</option>)}</select></label>
        {[["sf", 7, 12], ["cr", 5, 8], ["powerDbm", 0, 22], ["preamble", 8, 64], ["syncWord", 0, 255], ["retries", 0, 4]].map(args => numericField(...args))}
        {secondsField("intervalMs", "lora-interval", "lora_interval", 30, 3600)}
        {secondsField("ackMs", "lora-ack", "lora_ack", 1, 60)}
      </div></details>
      <p className="small muted">{t("lora_timing_note")}</p>
      <div className="lora-actions">
        <button id="lora-save" className="button primary" disabled={blocked}>{t("lora_save")}</button>
        {dirty && <p id="lora-unsaved" className="small muted" role="status">{t("lora_save_first")}</p>}
        <div className="lora-secondary-actions"><button id="lora-pair" type="button" className="button" disabled={actionBlocked} onClick={() => command("pair")}>{t("lora_pair")}</button><button id="lora-sample" type="button" className="button" disabled={actionBlocked} onClick={() => command("sample")}>{t("lora_sample")}</button></div>
      </div>
      {feedback && <p id="lora-feedback" className="hint" role="status" aria-live="polite">{t(feedback.key, feedback.values)}</p>}
    </form><p className="hint">{t("lora_gateway_note")}</p>
  </section>;
}
