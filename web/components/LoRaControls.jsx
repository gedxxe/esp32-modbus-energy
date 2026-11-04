import { useEffect, useRef, useState } from "react";
import { validLoRaView } from "../validation.js";
import { useText } from "../ui.js";
const initial={enabled:false,frequencyHz:915000000,bandwidthKHz:125,sf:7,cr:5,powerDbm:14,preamble:8,syncWord:18,nodeId:2,gatewayId:1,networkId:1,intervalMs:60000,ackMs:10000,retries:2};
const fields=Object.keys(initial);
export default function LoRaControls({device}){
  const {t,number}=useText();const v=validLoRaView(device.data?.lora)?device.data.lora:null;
  const [draft,setDraft]=useState(initial),[key,setKey]=useState(""),[keepKey,setKeepKey]=useState(false),[showKey,setShowKey]=useState(false);
  const signature=useRef("");
  useEffect(()=>{if(!v?.ready)return;const next=JSON.stringify([...fields.map(f=>v[f]),v.hasKey,v.stored,v.revision]);if(signature.current===next)return;signature.current=next;setDraft(Object.fromEntries(fields.map(f=>[f,v[f]])));setKeepKey(v.hasKey);setKey("");},[v]);
  const blocked=!v?.ready||!device.fresh||device.pending;
  const provisioning=Boolean(device.data?.network?.enterpriseProvisioning);
  function set(name,value){setDraft(d=>({...d,[name]:value}));}
  function generate(){const bytes=new Uint8Array(32);crypto.getRandomValues(bytes);setKey(Array.from(bytes,x=>x.toString(16).padStart(2,"0")).join(""));setKeepKey(false);setShowKey(true);}
  async function save(event){event.preventDefault();if(draft.nodeId===draft.gatewayId){device.setNotice({key:"invalid_lora"});return;}
    if(key&&!provisioning){device.setNotice({key:"lora_ap_required"});return;}
    if((key&&(!/^[0-9a-f]{64}$/i.test(key)||/^0{64}$/.test(key)))||(draft.enabled&&!keepKey&&!/^[0-9a-f]{64}$/i.test(key))){device.setNotice({key:"invalid_lora_key"});return;}
    const result=await device.act("/api/lora/config",{...draft,revision:v.revision,enabled:draft.enabled?1:0,key:keepKey?"":key,keepKey:keepKey?1:0});
    if(result?.ok){setKey("");setShowKey(false);device.setNotice({key:"lora_saved"});}
  }
  const input=(name,min,max,step=1)=> <input id={`lora-${name}`} type="number" min={min} max={max} step={step} value={draft[name]} required disabled={blocked} onChange={e=>set(name,e.target.value===""?"":Number(e.target.value))}/>;
  return <section className="panel" id="lora-panel" aria-labelledby="lora-title">
    <h2 id="lora-title">{t("lora_title")}</h2><p>{t("lora_intro")}</p>
    <div className="network-state" role="status"><strong id="lora-status">{v?t(`lora_phase_${v.phase}`):t("lora_unavailable")}</strong><span>{v?.paired?t("lora_paired"):t("lora_unpaired")}</span></div>
    {v?.code!==0&&v?.code!==undefined&&<p className="hint" id="lora-error">{t("lora_driver_error",{code:v.code})}</p>}
    <dl className="metrics"><div><dt>TX / RX / ACK</dt><dd>{v?`${v.tx} / ${v.rx} / ${v.acks}`:"—"}</dd></div><div><dt>{t("lora_failures")}</dt><dd>{v?`${v.timeouts} / ${v.rejected} / ${v.crcErrors}`:"—"}</dd></div><div><dt>{t("lora_auth_counts")}</dt><dd>{v?`${v.authErrors} / ${v.addressErrors} / ${v.replayErrors}`:"—"}</dd></div><div><dt>{t("lora_sequences")}</dt><dd>{v?`${v.lastTxSequence} / ${v.lastRxSequence}`:"—"}</dd></div><div><dt>RSSI / SNR</dt><dd>{v?.lastRxAgeMs?`${number(v.rssi,1)} dBm / ${number(v.snr,1)} dB`:"—"}</dd></div><div><dt>{t("lora_airtime")}</dt><dd>{v?`${v.airtimeMs} ms / ${Math.ceil(v.waitMs/1000)} s`:"—"}</dd></div></dl>
    <form id="lora-form" onSubmit={save}>
      <label className="checkbox-line"><input id="lora-enabled" type="checkbox" disabled={blocked} checked={draft.enabled} onChange={e=>set("enabled",e.target.checked)}/>{t("lora_enable")}</label>
      <label htmlFor="lora-frequency">{t("lora_frequency")}</label><input id="lora-frequency" type="number" min="803" max="930" step="0.001" required disabled={blocked} value={draft.frequencyHz===""?"":draft.frequencyHz/1e6} onChange={e=>set("frequencyHz",e.target.value===""?"":Math.round(Number(e.target.value)*1e6))}/>
      <div className="lora-grid">{[["nodeId",1,65534],["gatewayId",1,65534],["networkId",1,65535]].map(([name,min,max])=><label key={name} htmlFor={`lora-${name}`}>{t(`lora_${name}`)}{input(name,min,max)}</label>)}</div>
      <p className="small muted">{t("lora_ids_note")}</p><p className="hint" id="lora-ptmp-note">{t("lora_ptmp_note")}</p>
      {v?.hasKey&&<label className="checkbox-line"><input id="lora-keep-key" type="checkbox" checked={keepKey} disabled={blocked} onChange={e=>{setKeepKey(e.target.checked);setKey("");}}/>{t("lora_keep_key")}</label>}
      {!keepKey&&<><label htmlFor="lora-key">{t("lora_key")}</label><input id="lora-key" type={showKey?"text":"password"} autoComplete="new-password" spellCheck="false" maxLength={64} value={key} disabled={blocked} required={draft.enabled} onChange={e=>setKey(e.target.value)}/><div className="actions"><button id="lora-generate" type="button" className="button" disabled={blocked} onClick={generate}>{t("lora_generate")}</button><button type="button" className="button" disabled={blocked} onClick={()=>setShowKey(!showKey)}>{t(showKey?"lora_hide_key":"lora_show_key")}</button></div><p className="small muted">{t("lora_key_note")}</p></>}
      {!provisioning&&<p className="hint">{t("lora_ap_required")}</p>}
      <details><summary>{t("lora_advanced")}</summary><div className="lora-grid">
        <label htmlFor="lora-bandwidthKHz">{t("lora_bandwidth")}<select id="lora-bandwidthKHz" value={draft.bandwidthKHz} disabled={blocked} onChange={e=>set("bandwidthKHz",Number(e.target.value))}>{[125,250,500].map(n=><option key={n} value={n}>{n} kHz</option>)}</select></label>
        {[["sf",7,12],["cr",5,8],["powerDbm",0,22],["preamble",8,64],["syncWord",0,255],["retries",0,4]].map(([name,min,max])=><label key={name} htmlFor={`lora-${name}`}>{t(`lora_${name}`)}{input(name,min,max)}</label>)}
        <label htmlFor="lora-interval">{t("lora_interval")}<input id="lora-interval" type="number" min="30" max="3600" required disabled={blocked} value={draft.intervalMs/1000} onChange={e=>set("intervalMs",Number(e.target.value)*1000)}/></label>
        <label htmlFor="lora-ack">{t("lora_ack")}<input id="lora-ack" type="number" min="1" max="60" required disabled={blocked} value={draft.ackMs/1000} onChange={e=>set("ackMs",Number(e.target.value)*1000)}/></label>
      </div></details>
      <p className="small muted">{t("lora_timing_note")}</p><div className="actions"><button id="lora-save" className="button primary" disabled={blocked}>{t("lora_save")}</button><button id="lora-pair" type="button" className="button" disabled={blocked||!v?.enabled||!v?.available} onClick={()=>device.act("/api/lora/control",{action:"pair"})}>{t("lora_pair")}</button><button id="lora-sample" type="button" className="button" disabled={blocked||!v?.enabled||!v?.available} onClick={()=>device.act("/api/lora/control",{action:"sample"})}>{t("lora_sample")}</button></div>
    </form><p className="hint">{t("lora_gateway_note")}</p>
  </section>;
}
