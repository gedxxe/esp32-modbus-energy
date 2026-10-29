import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
import WifiPicker from "./WifiPicker.jsx";
import { validNetworkView, validAPIP } from "../validation.js";

export default function NetworkControls({ device }) {
  const { t } = useText();
  const network = validNetworkView(device.data?.network) ? device.data.network : null;
  const [mode, setMode] = useState("ap"), [ssid, setSSID] = useState("");
  const [password, setPassword] = useState(""), [open, setOpen] = useState(false);
  const [apIP,setAPIP]=useState("192.168.4.1"),[scanBusy,setScanBusy]=useState(false);
  const [auth,setAuth]=useState("personal"),[identity,setIdentity]=useState(""),[username,setUsername]=useState(""),[eapPassword,setEapPassword]=useState("");
  const [serverName,setServerName]=useState(""),[ca,setCA]=useState(""),[keepEnterprise,setKeepEnterprise]=useState(false),[keepCA,setKeepCA]=useState(false);
  const [autoIdentity,setAutoIdentity]=useState(true);
  const remoteConfig = useRef("");
  useEffect(() => {
    if (!network) return;
    const signature=JSON.stringify([network.mode,network.ssid,network.hasPassword,network.configuredAPIP,network.auth,network.hasEnterpriseCredentials,network.serverName,network.caFingerprint,network.hasCustomIdentity]);
    if(signature===remoteConfig.current)return;
    remoteConfig.current=signature;
    setAPIP(network.configuredAPIP);setMode(network.mode); setSSID(network.ssid); setPassword("");
    setOpen(network.auth==="open"||(network.auth!=="peap"&&Boolean(network.ssid)&&!network.hasPassword));
    setAutoIdentity(!network.hasCustomIdentity);setAuth(network.auth==="peap"?"peap":"personal");setIdentity("");setUsername("");setEapPassword("");setCA("");setServerName(network.serverName||"");setKeepEnterprise(Boolean(network.hasEnterpriseCredentials));setKeepCA(Boolean(network.caFingerprint));
  }, [network?.mode, network?.ssid, network?.hasPassword,network?.configuredAPIP,network?.auth,network?.hasEnterpriseCredentials,network?.serverName,network?.caFingerprint,network?.hasCustomIdentity]);
  const blocked = !network || !device.fresh || device.pending || network.pending ||
    device.data?.discoveryRunning || device.data?.diagnosticActive || device.data?.scan?.phase === "running";
  const formBlocked=blocked||scanBusy;
  const keep = auth!=="peap" && !open && !password && network?.hasPassword && ssid === network.ssid;
  async function submit(event) {
    event.preventDefault();
    if(!validAPIP(apIP)){device.setNotice({key:"invalid_ap_ip"});return;}
    if(mode!=="ap"&&(!ssid||new TextEncoder().encode(ssid).length>32||/[\x00-\x1f\x7f]/.test(ssid))){device.setNotice({key:"invalid_network"});return;}
    if(mode!=="ap"&&auth==="peap"){
      const validText=value=>new TextEncoder().encode(value).length>0&&new TextEncoder().encode(value).length<=127&&!/[\x00-\x1f\x7f]/.test(value);
      if(!network?.enterpriseProvisioning){device.setNotice({key:"enterprise_ap_required"});return;}
      if(!keepCA&&!ca){device.setNotice({key:"enterprise_profile_missing"});return;}
      if(!ssid||(!keepEnterprise&&![username,eapPassword].every(validText))||serverName.length>253||(serverName&&!/^([a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i.test(serverName))||(!keepCA&&!ca)||new TextEncoder().encode(ca).length>4096){device.setNotice({key:"invalid_enterprise"});return;}
    }
    if (mode !== "ap" && auth!=="peap" && (!ssid || new TextEncoder().encode(ssid).length > 32 ||
      /[\x00-\x1f\x7f]/.test(ssid) || (!open && !keep &&
        !(/^[ -~]{8,63}$/.test(password) || /^[0-9a-fA-F]{64}$/.test(password))))) {
      device.setNotice({ key: "invalid_network" }); return;
    }
    const result = await device.act("/api/network", { mode, ssid, password, apIP, auth:auth==="peap"?"peap":open?"open":"personal", open: open ? 1 : 0, keep: keep ? 1 : 0, identity,autoIdentity:autoIdentity?1:0,username,eapPassword,serverName,ca,keepEnterprise:keepEnterprise?1:0,keepCA:keepCA?1:0 });
    if (result?.ok) { setPassword("");setEapPassword("");setIdentity("");setUsername("");setCA("");if(auth==="peap"){setKeepEnterprise(true);setKeepCA(true);} device.setNotice({ key: "network_saved" }); }
  }
  return <section className="panel network-panel" id="network-panel" aria-labelledby="network-title">
    <h2 id="network-title">{t("network_title")}</h2>
    <p>{t("network_intro")}</p>
    {!network ? <p role="status" className="hint">{t("network_unavailable")}</p> : <>
      <div className="network-state" role="status" id="network-status">
        <strong>{t(`network_phase_${network.phase}`)}</strong>
        <span>{t("network_signal", { rssi: network.connected||network.associated ? network.rssi : "—" })}</span>
      </div>
      <dl className="network-addresses">
        <div><dt>{t("network_client_ip")}</dt><dd>{network.clientIP || "—"}</dd></div>
        <div><dt>{t("network_ap_ip")}</dt><dd>{network.apIP || "—"}</dd></div>
      </dl>
      <p className="small muted">{t("network_recovery", { ssid: network.apSSID })}</p>
      {network.error&&<p id="network-error" className="hint" role="status">{t(`network_error_${network.error}`)}<span className="small mono"> · {network.disconnectReason||0} / {network.apiCode||0}</span></p>}
      {!network.stored && <p className="hint">{t("network_storage_failed")}</p>}
    </>}
    <form id="network-form" onSubmit={submit}>
      <label htmlFor="network-mode">{t("network_mode")}</label>
      <select id="network-mode" value={mode} onChange={(event) => setMode(event.target.value)} disabled={formBlocked}>
        {["ap", "client", "dual"].map((value) => <option key={value} value={value}>{t(`network_mode_${value}`)}</option>)}
      </select>
      {mode !== "ap" && <div className="network-credentials">
        <WifiPicker device={device} ssid={ssid} open={open} auth={auth} disabled={blocked} onBusy={setScanBusy} onChoose={(row)=>{setSSID(row.ssid);setOpen(row.open);setAuth(row.security==="wpa2-enterprise"?"peap":"personal");setPassword("");setIdentity("");setUsername("");setEapPassword("");setKeepEnterprise(false);setKeepCA(Boolean(row.ssid===network?.ssid&&network.auth==="peap"&&network.caFingerprint));setServerName(row.ssid===network?.ssid?network.serverName||"":"");setCA("");}} />
        <label htmlFor="network-auth">{t("enterprise_auth_label")}</label>
        <select id="network-auth" value={auth} disabled={blocked} onChange={event=>{setAuth(event.target.value);setOpen(false);setPassword("");setKeepEnterprise(false);setKeepCA(false);}}><option value="personal">{t("enterprise_personal")}</option><option value="peap">{t("enterprise_company_login")}</option></select>
        {auth==="peap"?<div className="enterprise-fields">

          {!network?.enterpriseProvisioning&&<p className="hint">{t("enterprise_ap_required")}</p>}
          {!keepCA&&!ca&&<p className="hint">{t("enterprise_profile_missing")}</p>}
          {network?.hasEnterpriseCredentials&&ssid===network.ssid&&network.auth==="peap"&&<label className="checkbox-line"><input id="enterprise-keep" type="checkbox" checked={keepEnterprise} disabled={blocked} onChange={e=>setKeepEnterprise(e.target.checked)}/>{t("enterprise_keep_credentials")}</label>}
          <label htmlFor="enterprise-username">{t("enterprise_username")}</label><input id="enterprise-username" autoComplete="off" maxLength={127} value={username} required={!keepEnterprise} disabled={blocked||keepEnterprise} onChange={e=>setUsername(e.target.value)}/>
          <label htmlFor="enterprise-password">{t("network_password")}</label><input id="enterprise-password" type="password" autoComplete="new-password" maxLength={127} value={eapPassword} required={!keepEnterprise} disabled={blocked||keepEnterprise} onChange={e=>setEapPassword(e.target.value)}/>
          <details id="enterprise-advanced"><summary>{t("enterprise_advanced")}</summary><p className="small muted">{t("enterprise_method_note")}</p>
          <label className="checkbox-line"><input id="enterprise-auto-identity" type="checkbox" checked={autoIdentity} disabled={blocked} onChange={e=>{setAutoIdentity(e.target.checked);setKeepEnterprise(false);}}/>{t("enterprise_auto_identity")}</label>
          <label htmlFor="enterprise-identity">{t("enterprise_identity")}</label><input id="enterprise-identity" autoComplete="off" maxLength={127} value={identity}  disabled={blocked||keepEnterprise||autoIdentity} onChange={e=>setIdentity(e.target.value)}/>
          <label htmlFor="enterprise-server">{t("enterprise_server")}</label><input id="enterprise-server" autoComplete="off" maxLength={253} value={serverName} disabled={blocked} onChange={e=>{setServerName(e.target.value);setKeepEnterprise(false);}}/>
          <p className="small muted">{t("enterprise_server_note")}</p>
          {network?.caFingerprint&&<><p className="small mono enterprise-fingerprint">CA SHA256 {network.caFingerprint}</p><label className="checkbox-line"><input id="enterprise-keep-ca" type="checkbox" checked={keepCA} disabled={blocked} onChange={e=>{setKeepCA(e.target.checked);setKeepEnterprise(false);setCA("");}}/>{t("enterprise_keep_ca")}</label></>}
          <label htmlFor="enterprise-ca-file">{t("enterprise_ca")}</label><input id="enterprise-ca-file" type="file" accept=".pem,.crt,.cer,text/plain" disabled={blocked||keepCA} onChange={async e=>{const file=e.target.files?.[0];if(!file)return;if(file.size>4096){device.setNotice({key:"invalid_enterprise_ca"});e.target.value="";return;}try{setCA(await file.text());setKeepEnterprise(false);}catch{device.setNotice({key:"invalid_enterprise_ca"});}}}/>
          <textarea id="enterprise-ca" aria-label={t("enterprise_ca")} value={ca} rows={5} maxLength={4096} disabled={blocked||keepCA} placeholder="-----BEGIN CERTIFICATE-----" onChange={e=>{setCA(e.target.value);setKeepEnterprise(false);}}/>
          <p className="small muted">{t("enterprise_ca_note")}</p><p className="hint">{t(network?.flashEncrypted?"enterprise_storage_encrypted":"enterprise_storage_plain")}</p></details>
        </div>:<>
          <label htmlFor="network-password">{t("network_password")}</label>
          <input id="network-password" type="password" autoComplete="new-password" value={password} maxLength={64} disabled={blocked || open} onChange={(event) => setPassword(event.target.value)} />
          <p className="small muted">{t(keep ? "network_keep" : "network_key_help")}</p>
          <label className="checkbox-line" htmlFor="network-open"><input id="network-open" type="checkbox" checked={open} disabled={blocked} onChange={(event) => { setOpen(event.target.checked); setPassword(""); }} />{t("network_open")}</label>
        </>}
      </div>}
      <label htmlFor="network-ap-ip">{t("ap_ip_label")}</label>
      <input id="network-ap-ip" value={apIP} onChange={(event)=>setAPIP(event.target.value)} maxLength={15} inputMode="decimal" required disabled={blocked} />
      <p className="small muted">{t("ap_ip_note")}</p>
      <p className="small muted">{t(`network_note_${mode}`)}</p>
      <div className="actions"><button id="network-save" className="button primary" disabled={formBlocked||(mode!=="ap"&&auth==="peap"&&!network?.enterpriseProvisioning)}>{t("network_save")}</button></div>
    </form>
    <div className="hint">{t("network_transition")}</div>
  </section>;
}
