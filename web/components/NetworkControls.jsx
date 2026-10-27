import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
import { validNetworkView } from "../validation.js";

export default function NetworkControls({ device }) {
  const { t } = useText();
  const network = validNetworkView(device.data?.network) ? device.data.network : null;
  const [mode, setMode] = useState("ap"), [ssid, setSSID] = useState("");
  const [password, setPassword] = useState(""), [open, setOpen] = useState(false);
  const remoteConfig = useRef("");
  useEffect(() => {
    if (!network) return;
    const signature=JSON.stringify([network.mode,network.ssid,network.hasPassword]);
    if(signature===remoteConfig.current)return;
    remoteConfig.current=signature;
    setMode(network.mode); setSSID(network.ssid); setPassword("");
    setOpen(Boolean(network.ssid) && !network.hasPassword);
  }, [network?.mode, network?.ssid, network?.hasPassword]);
  const blocked = !network || !device.fresh || device.pending || network.pending ||
    device.data?.diagnosticActive || device.data?.scan?.phase === "running";
  const keep = !open && !password && network?.hasPassword && ssid === network.ssid;
  async function submit(event) {
    event.preventDefault();
    if (mode !== "ap" && (!ssid || new TextEncoder().encode(ssid).length > 32 ||
      /[\x00-\x1f\x7f]/.test(ssid) || (!open && !keep &&
        !(/^[ -~]{8,63}$/.test(password) || /^[0-9a-fA-F]{64}$/.test(password))))) {
      device.setNotice({ key: "invalid_network" }); return;
    }
    const result = await device.act("/api/network", { mode, ssid, password, open: open ? 1 : 0, keep: keep ? 1 : 0 });
    if (result?.ok) { setPassword(""); device.setNotice({ key: "network_saved" }); }
  }
  return <section className="panel network-panel" id="network-panel" aria-labelledby="network-title">
    <h2 id="network-title">{t("network_title")}</h2>
    <p>{t("network_intro")}</p>
    {!network ? <p role="status" className="hint">{t("network_unavailable")}</p> : <>
      <div className="network-state" role="status" id="network-status">
        <strong>{t(`network_phase_${network.phase}`)}</strong>
        <span>{t("network_signal", { rssi: network.connected ? network.rssi : "—" })}</span>
      </div>
      <dl className="network-addresses">
        <div><dt>{t("network_client_ip")}</dt><dd>{network.clientIP || "—"}</dd></div>
        <div><dt>{t("network_ap_ip")}</dt><dd>{network.apIP || "—"}</dd></div>
      </dl>
      <p className="small muted">{t("network_recovery", { ssid: network.apSSID })}</p>
      {!network.stored && <p className="hint">{t("network_storage_failed")}</p>}
    </>}
    <form id="network-form" onSubmit={submit}>
      <label htmlFor="network-mode">{t("network_mode")}</label>
      <select id="network-mode" value={mode} onChange={(event) => setMode(event.target.value)} disabled={blocked}>
        {["ap", "client", "dual"].map((value) => <option key={value} value={value}>{t(`network_mode_${value}`)}</option>)}
      </select>
      {mode !== "ap" && <div className="network-credentials">
        <label htmlFor="network-ssid">{t("network_ssid")}</label>
        <input id="network-ssid" value={ssid} onChange={(event) => setSSID(event.target.value)} disabled={blocked} maxLength={32} required autoCapitalize="none" autoCorrect="off" spellCheck={false} />
        <label htmlFor="network-password">{t("network_password")}</label>
        <input id="network-password" type="password" autoComplete="new-password" value={password} maxLength={64} disabled={blocked || open} onChange={(event) => setPassword(event.target.value)} />
        <p className="small muted">{t(keep ? "network_keep" : "network_key_help")}</p>
        <label className="checkbox-line" htmlFor="network-open"><input id="network-open" type="checkbox" checked={open} disabled={blocked} onChange={(event) => { setOpen(event.target.checked); setPassword(""); }} />{t("network_open")}</label>
      </div>}
      <p className="small muted">{t(`network_note_${mode}`)}</p>
      <div className="actions"><button id="network-save" className="button primary" disabled={blocked}>{t("network_save")}</button></div>
    </form>
    <div className="hint">{t("network_transition")}</div>
  </section>;
}
