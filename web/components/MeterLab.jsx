import { useEffect, useState } from "react";
import { useText } from "../ui.js";
import { deviceFetch } from "../request.js";
import { meterProfiles, addressHex } from "../meter-profile.js";
import { validMeterLab, parseProbePlan, wordCandidates } from "../meter-lab.js";

function Evidence({ device }) {
  const { t }=useText();
  return <details className="lab-evidence"><summary>{t("lab_frames")}</summary>
    <p className="small muted">{t("lab_decode_note")}</p>
    {device.probes.map((probe,index)=>{const candidate=wordCandidates(probe);return <article className="lab-probe" key={`${probe.address}-${index}`}>
      <div className="lab-probe-title"><strong>FC03 · 0x{addressHex(probe.address)} · {probe.words} words</strong><span>{t(probe.confirmed?probe.data?"lab_data":"lab_exception":"lab_unconfirmed")}</span></div>
      {probe.replies.map((reply,i)=><div key={i} className="lab-reply"><span>#{i+1} · 0x{reply.status.toString(16).padStart(2,"0")} · {reply.attemptMs} ms{reply.truncated?` · ${t("lab_truncated")}`:""}</span><code>TX {(reply.tx??probe.tx)||"∅"}</code><code>RX {reply.rx||"∅"}</code></div>)}
      {candidate&&<div className="small mono lab-candidates"><span>U16: {candidate.words.join(" / ")}</span>{candidate.unsigned!==undefined&&<><span>U32 ABCD: {candidate.unsigned} · CDAB: {candidate.swapped}</span><span>I32: {candidate.signed}</span><span>F32 ABCD: {candidate.float??"non-finite"} · CDAB: {candidate.floatSwapped??"non-finite"}</span></>}</div>}
    </article>;})}
  </details>;
}
function SaveFinding({ finding, run, device, disabled, onSaved }) {
  const { t }=useText();const [name,setName]=useState(""),[model,setModel]=useState(""),[notes,setNotes]=useState(""),[profile,setProfile]=useState("unknown"),[verified,setVerified]=useState(false);
  return <form className="lab-save" onSubmit={async event=>{event.preventDefault();const result=await device.act("/api/meter-lab",{action:"save",run,index:finding.index,name:name.trim(),model:model.trim(),notes:notes.trim(),profile,verified:verified?1:0});if(result?.ok)onSaved();}}>
    <div className="form-grid"><div><label>{t("lab_name")}<input aria-label={t("lab_name")} value={name} required maxLength={31} pattern="[ -~]+" onChange={e=>setName(e.target.value)} disabled={disabled}/></label></div>
    <div><label>{t("lab_model")}<input aria-label={t("lab_model")} value={model} required maxLength={39} pattern="[ -~]+" onChange={e=>setModel(e.target.value)} disabled={disabled}/></label></div>
    <div className="wide"><label>{t("lab_binding")}<select aria-label={t("lab_binding")} value={profile} onChange={e=>{setProfile(e.target.value);setVerified(false);}} disabled={disabled}><option value="unknown">{t("lab_unknown")}</option>{meterProfiles(device.data).map(p=><option value={p.key} key={p.key}>{p.name}</option>)}</select></label></div>
    <div className="wide"><label>{t("lab_notes")}<input aria-label={t("lab_notes")} value={notes} maxLength={63} pattern="[ -~]*" onChange={e=>setNotes(e.target.value)} disabled={disabled}/></label></div></div>
    <label className="checkbox-line"><input type="checkbox" checked={verified} disabled={disabled||profile==="unknown"} onChange={e=>setVerified(e.target.checked)}/>{t("lab_verified")}</label>
    <p className="small muted">{t("lab_save_note")}</p><button className="button primary" disabled={disabled}>{t("lab_save")}</button>
  </form>;
}
function MeterLabPanel({ device }) {
  const { t }=useText();const [view,setView]=useState(null),[failed,setFailed]=useState(false),[refresh,setRefresh]=useState(0);
  const [first,setFirst]=useState(1),[last,setLast]=useState(16),[mode,setMode]=useState("current"),[timeout,setTimeoutValue]=useState(350),[plan,setPlan]=useState(""),[planEdited,setPlanEdited]=useState(false);
  useEffect(()=>{if(!planEdited&&view?.discovery?.plan)setPlan(view.discovery.plan.map(p=>`${addressHex(p.address)}:${p.words}`).join(","));},[view?.discovery?.run,Boolean(view),planEdited]);
  useEffect(()=>{let alive=true,timer,abort;async function poll(){abort=new AbortController();try{const response=await deviceFetch("/api/meter-lab",{cache:"no-store",signal:abort.signal});const next=await response.json();if(!response.ok||!validMeterLab(next))throw new Error("schema");if(alive){setView(next);setFailed(false);}}catch{if(alive)setFailed(true);}finally{if(alive)timer=setTimeout(poll,1000);}}poll();return()=>{alive=false;clearTimeout(timer);abort?.abort();};},[refresh]);
  const scan=view?.discovery,running=scan?.phase==="running",blocked=device.pending||!device.fresh||failed||!view?.ready||device.data?.diagnosticActive||device.data?.scan?.phase==="running";
  const refreshNow=()=>setRefresh(v=>v+1);
  const start=event=>{event.preventDefault();if(!parseProbePlan(plan)){device.setNotice({key:"lab_plan_error"});return;}device.act("/api/meter-lab",{action:"start",first,last,mode,timeout,plan});};
  return <div className="meter-lab-content">
    <p>{t("lab_intro")}</p><p className="small muted">{t("lab_limits")}</p>
    {failed&&<p className="hint" role="status">{t("lab_failed")}</p>}
    <form id="lab-start-form" onSubmit={start}><div className="form-grid">
      <div><label htmlFor="lab-first">{t("first_id")}</label><input id="lab-first" type="number" min="1" max="247" required value={first} onChange={e=>setFirst(e.target.value)} disabled={blocked||running}/></div>
      <div><label htmlFor="lab-last">{t("last_id")}</label><input id="lab-last" type="number" min={first} max="247" required value={last} onChange={e=>setLast(e.target.value)} disabled={blocked||running}/></div>
      <div><label htmlFor="lab-mode">{t("serial_format")}</label><select id="lab-mode" value={mode} onChange={e=>setMode(e.target.value)} disabled={blocked||running}><option value="current">{t("scan_current")}</option><option value="matrix">{t("lab_matrix")}</option></select></div>
      <div><label htmlFor="lab-timeout">{t("lab_timeout")}</label><input id="lab-timeout" type="number" min="150" max="3000" value={timeout} onChange={e=>setTimeoutValue(e.target.value)} disabled={blocked||running} required/></div>
      <div className="wide"><label htmlFor="lab-plan">{t("lab_plan")}</label><input id="lab-plan" className="mono" value={plan} onChange={e=>{setPlanEdited(true);setPlan(e.target.value);}} maxLength={63} disabled={blocked||running} required/><p className="small muted">{t("lab_plan_help")}</p></div>
    </div><div className="actions"><button id="lab-start" className="button primary" disabled={blocked||running}>{t("lab_start")}</button><button id="lab-stop" className="button danger" type="button" disabled={!running||device.priorityPending||!device.fresh} onClick={()=>device.act("/api/meter-lab/stop",{run:scan.run},{priority:true})}>{t("lab_stop")}</button>
      <a className="button" id="lab-export" href="/api/meter-lab?download=1">{t("lab_export")}</a></div></form>
    {scan&&<div className="lab-progress" role="status"><strong>{t(`lab_phase_${scan.phase}`)}</strong><progress value={scan.tested} max={scan.total||1}/><span className="mono">{scan.tested} / {scan.total} · {Math.floor(scan.elapsedMs/1000)} s · RX CRC {scan.crcErrors} · timeout {scan.timeouts}</span></div>}
    {running&&<p className="hint">{t("lab_paused")}</p>}
    <div id="lab-findings">{view?.findings.map(finding=><article className="lab-device" key={`${scan.run}-${finding.index}`}><h3>ID {finding.slave} · {finding.baud} / 8{finding.parity}{finding.stopBits}</h3>
      <p>{t("lab_confirmed_note")}</p>{finding.mapEvidence!==0&&<p className="small muted">{t("lab_map_hint")} {(finding.mappingHints||[]).map(key=>meterProfiles(device.data).find(p=>p.key===key)?.name||key).join(" / ")}</p>}
      <Evidence device={finding}/><SaveFinding finding={finding} run={scan.run} device={device} disabled={blocked||running||view.saved.length>=4} onSaved={refreshNow}/></article>)}</div>
    {view?.current.probes.length>0&&<details className="lab-current"><summary>{t("lab_current")}</summary><p className="mono">ID {view.current.slave} · {view.current.baud} / 8{view.current.parity}{view.current.stopBits}</p><Evidence device={view.current}/></details>}
    <h3>{t("lab_saved")}</h3>{view&&!view.storageOK&&<p className="hint">{t("lab_storage")}</p>}
    <div id="lab-saved">{view?.saved.map(meter=><article className="lab-device" key={meter.id}><div className="lab-probe-title"><h4>{meter.name}</h4><span className="quality">#{meter.id}{(device.data?.savedMeterId??view.activeId)===meter.id?` · ${t("lab_active")}`:""}</span></div><p>{meter.model} · ID {meter.evidence.slave} · {meter.evidence.baud} / 8{meter.evidence.parity}{meter.evidence.stopBits}</p><p className="small muted">{meter.notes}</p>
      <Evidence device={meter.evidence}/><div className="actions"><button className="button" disabled={blocked||running||!meter.activatable||meter.id===(device.data?.savedMeterId??view.activeId)} onClick={()=>device.act("/api/meter-lab",{action:"select",id:meter.id}).then(refreshNow)}>{t("lab_use")}</button><button className="button danger" disabled={blocked||running||meter.id===(device.data?.savedMeterId??view.activeId)} onClick={()=>device.act("/api/meter-lab",{action:"remove",id:meter.id}).then(refreshNow)}>{t("lab_remove")}</button></div>
      {!meter.activatable&&<p className="small muted">{t("lab_observation_only")}</p>}</article>)}</div>
  </div>;
}
export default function MeterLab({ device }) {
  const { t }=useText();const [open,setOpen]=useState(false);
  return <details id="meter-lab" className="panel meter-lab" onToggle={event=>setOpen(event.currentTarget.open)}><summary>{t("lab_title")}</summary>{open&&<MeterLabPanel device={device}/>}</details>;
}
