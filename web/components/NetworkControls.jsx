import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
import WifiPicker from "./WifiPicker.jsx";
import NetworkResult from "./NetworkResult.jsx";
import Icon from "./Icon.jsx";
import { validNetworkView, validAPIP } from "../validation.js";
import { validateFormFields } from "../form-validation.js";

export default function NetworkControls({ device }) {
  const { t } = useText();
  const network = validNetworkView(device.data?.network) ? device.data.network : null;
  const [mode, setMode] = useState("ap"), [ssid, setSSID] = useState("");
  const [password, setPassword] = useState(""), [open, setOpen] = useState(false);
  const [apIP,setAPIP]=useState("192.168.4.1"),[scanBusy,setScanBusy]=useState(false);
  const [auth,setAuth]=useState("personal"),[identity,setIdentity]=useState(""),[username,setUsername]=useState(""),[eapPassword,setEapPassword]=useState("");
  const [serverName,setServerName]=useState(""),[ca,setCA]=useState(""),[keepEnterprise,setKeepEnterprise]=useState(false),[keepCA,setKeepCA]=useState(false);
  const [autoIdentity,setAutoIdentity]=useState(true);
  const [feedback,setFeedback]=useState(null);
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
  const apAddress=validAPIP(network?.apIP)?network.apIP:null;
  const keep = auth!=="peap" && !open && !password && network?.hasPassword && ssid === network.ssid;
  function explain(key,values={}){setFeedback({key,values});device.setNotice(Object.keys(values).length?{text:t(key,values)}:{key});}
  async function submit(event) {
    event.preventDefault();
    if(formBlocked){explain(scanBusy?"wifi_scanning":"network_unavailable");return;}
    if(mode!=="ap"&&auth==="peap"&&!network?.enterpriseProvisioning&&!keepEnterprise){explain("enterprise_ap_required");return;}
    if(!validateFormFields(event.currentTarget,t,explain))return;
    if(!validAPIP(apIP)){explain("invalid_ap_ip");return;}
    if(mode!=="ap"&&(!ssid||new TextEncoder().encode(ssid).length>32||/[\x00-\x1f\x7f]/.test(ssid))){explain("invalid_network");return;}
    if(mode!=="ap"&&auth==="peap"){
      const validText=value=>new TextEncoder().encode(value).length>0&&new TextEncoder().encode(value).length<=127&&!/[\x00-\x1f\x7f]/.test(value);
      if(!ssid||(!keepEnterprise&&(![username,eapPassword].every(validText)||(!autoIdentity&&!validText(identity))))||serverName.length>253||(serverName&&!/^([a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i.test(serverName))||new TextEncoder().encode(ca).length>4096){explain("invalid_enterprise");return;}
    }
    if (mode !== "ap" && auth!=="peap" && (!ssid || new TextEncoder().encode(ssid).length > 32 ||
      /[\x00-\x1f\x7f]/.test(ssid) || (!open && !keep &&
        !(/^[ -~]{8,63}$/.test(password) || /^[0-9a-fA-F]{64}$/.test(password))))) {
      explain("invalid_network"); return;
    }
    const enterprise=mode!=="ap"&&auth==="peap";
    setFeedback({key:"network_saving"});
    const result = await device.act("/api/network", {
      mode, ssid, password:enterprise?"":password, apIP,
      auth:auth==="peap"?"peap":open?"open":"personal", open:open?1:0, keep:keep?1:0,
      ...(enterprise?{identity,autoIdentity:autoIdentity?1:0,username,eapPassword,serverName,ca,keepEnterprise:keepEnterprise?1:0,keepCA:keepCA?1:0}:{})
    });
    if (result?.ok) { setPassword("");setEapPassword("");setIdentity("");setUsername("");setCA("");if(auth==="peap"){setKeepEnterprise(true);setKeepCA(Boolean(keepCA||ca));} explain("network_saved"); }
    else setFeedback({key:result?.error||result?.body?.code||"request_failed"});
  }
  return <section className="panel network-panel" id="network-panel" aria-labelledby="network-title">
    <h2 id="network-title" className="panel-title"><Icon name="wifi" size={23}/>{t("network_title")}</h2>
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
      {!network.stored && <p className="hint">{t("network_storage_failed")}</p>}
    </>}
    <form id="network-form" onSubmit={submit} onChange={()=>setFeedback(null)} noValidate>
      <label htmlFor="network-mode">{t("network_mode")}</label>
      <select id="network-mode" value={mode} onChange={(event) => setMode(event.target.value)} disabled={formBlocked}>
        {["ap", "client", "dual"].map((value) => <option key={value} value={value}>{t(`network_mode_${value}`)}</option>)}
      </select>
      {mode !== "ap" && <div className="network-credentials">
        <WifiPicker device={device} ssid={ssid} open={open} auth={auth} disabled={blocked} onBusy={setScanBusy} onChoose={(row)=>{const enterprise=row.security==="wpa2-enterprise";const saved=row.ssid===network?.ssid&&((network.auth==="peap")===enterprise)&&((network.auth==="open")===row.open);setFeedback(null);setSSID(row.ssid);setOpen(row.open);setAuth(enterprise?"peap":"personal");setAutoIdentity(true);setPassword("");setIdentity("");setUsername("");setEapPassword("");setKeepEnterprise(Boolean(saved&&enterprise&&network.hasEnterpriseCredentials));setKeepCA(Boolean(saved&&enterprise&&network.caFingerprint));setServerName(saved&&enterprise?network.serverName||"":"");setCA("");}} onManual={(value)=>{if(value===ssid)return;const saved=value===network?.ssid;setFeedback(null);setSSID(value);setOpen(saved&&network.auth==="open");setPassword("");setIdentity("");setUsername("");setEapPassword("");setAutoIdentity(true);setKeepEnterprise(Boolean(saved&&auth==="peap"&&network.auth==="peap"&&network.hasEnterpriseCredentials));setKeepCA(Boolean(saved&&auth==="peap"&&network.auth==="peap"&&network.caFingerprint));setServerName(saved&&auth==="peap"?network.serverName||"":"");setCA("");}} />
        <label htmlFor="network-auth">{t("enterprise_auth_label")}</label>
        <select id="network-auth" value={auth} disabled={blocked} onChange={event=>{setFeedback(null);setAuth(event.target.value);setAutoIdentity(true);setIdentity("");setUsername("");setEapPassword("");setOpen(false);setPassword("");setKeepEnterprise(false);setKeepCA(false);}}><option value="personal">{t("enterprise_personal")}</option><option value="peap">{t("enterprise_company_login")}</option></select>
        {auth==="peap"?<div className="enterprise-fields">
          {!network?.enterpriseProvisioning&&<div id="enterprise-access-help" className="hint" role="status">
            <p>{t(apAddress?"enterprise_ap_access":"enterprise_ap_inactive",{ssid:network?.apSSID||"",address:apAddress||""})}</p>
            {apAddress&&<a id="enterprise-ap-link" href={`http://${apAddress}/#device`}>{t("enterprise_open_ap")}</a>}
          </div>}
          {!keepCA&&!ca&&<p id="enterprise-trust-note" className="small muted">{t("enterprise_public_trust")}</p>}
          {network?.hasEnterpriseCredentials&&ssid===network.ssid&&network.auth==="peap"&&<label className="checkbox-line"><input id="enterprise-keep" type="checkbox" checked={keepEnterprise} disabled={blocked} onChange={e=>{setKeepEnterprise(e.target.checked);if(!e.target.checked&&!identity)setAutoIdentity(true);}}/>{t("enterprise_keep_credentials")}</label>}
          <label htmlFor="enterprise-username">{t("enterprise_username")}</label><input id="enterprise-username" autoComplete="off" maxLength={127} value={username} required={!keepEnterprise} disabled={blocked||!network?.enterpriseProvisioning} onChange={e=>{setUsername(e.target.value);setKeepEnterprise(false);if(keepEnterprise)setAutoIdentity(true);}}/>
          <label htmlFor="enterprise-password">{t("network_password")}</label><input id="enterprise-password" type="password" autoComplete="new-password" maxLength={127} value={eapPassword} required={!keepEnterprise} disabled={blocked||!network?.enterpriseProvisioning} onChange={e=>{setEapPassword(e.target.value);setKeepEnterprise(false);if(keepEnterprise)setAutoIdentity(true);}}/>
          <details id="enterprise-advanced"><summary>{t("enterprise_advanced")}</summary><p className="small muted">{t("enterprise_method_note")}</p>
          <label className="checkbox-line"><input id="enterprise-auto-identity" type="checkbox" checked={autoIdentity} disabled={blocked} onChange={e=>{setAutoIdentity(e.target.checked);setKeepEnterprise(false);}}/>{t("enterprise_auto_identity")}</label>
          <label htmlFor="enterprise-identity">{t("enterprise_identity")}</label><input id="enterprise-identity" autoComplete="off" maxLength={127} value={identity} required={!keepEnterprise&&!autoIdentity} disabled={blocked||keepEnterprise||autoIdentity} onChange={e=>setIdentity(e.target.value)}/>
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
      <p className="small muted">{t(`network_note_${mode}`,{ssid:network?.apSSID||""})}</p>
      <div className="actions"><button id="network-save" className="button primary" disabled={formBlocked}>{t("network_save")}</button></div>
      {feedback&&<p id="network-feedback" className="hint" role="status" aria-live="polite">{t(feedback.key,feedback.values)}</p>}
      <NetworkResult network={network} fresh={device.fresh} scanBusy={scanBusy} />
      {formBlocked&&<p id="network-blocked" className="small muted" role="status">{t(scanBusy?"wifi_scanning":network?.pending||device.pending?"network_pending":device.data?.diagnosticActive?"diagnostic_busy":device.data?.discoveryRunning||device.data?.scan?.phase==="running"?"scan_busy":"network_unavailable")}</p>}
    </form>
    <div className="hint">{t("network_transition")}</div>
  </section>;
}
