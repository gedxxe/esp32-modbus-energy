import { profileName, meterProfiles } from "../meter-profile.js";
import { useEffect, useRef, useState } from "react";
import { deviceFetch } from "../request.js";
import { validCheckpoints, validHistory } from "../validation.js";
import { useText, timeOf } from "../ui.js";
import Icon from "./Icon.jsx";

const observedTime=utc=>utc==null?"—":new Date((utc+25200)*1000).toISOString().slice(11,19);

export default function CheckpointRecords({ device }){
  const { t,number }=useText();
  const [shift,setShift]=useState(0),[differences,setDifferences]=useState(false);
  const [selected,setSelected]=useState(""),[historyOpen,setHistoryOpen]=useState(false),[history,setHistory]=useState([]);
  const [archive,setArchive]=useState(null),[failed,setFailed]=useState(false),[loading,setLoading]=useState(false),[historyFailed,setHistoryFailed]=useState(false);
  const lastLive=useRef(null),live=device.data?.checkpoints;
  const valid=validCheckpoints(live);
  useEffect(()=>{if(valid)lastLive.current=live;},[live,valid]);
  const current=valid?live:lastLive.current;
  useEffect(()=>{
    if(!historyOpen||!device.fresh)return;
    let alive=true,controller;
    const load=async()=>{
      controller=new AbortController();
      try{
        const response=await deviceFetch("/api/history?kind=checkpoints",{cache:"no-store",signal:controller.signal});
        const rows=await response.json();if(!response.ok||!validHistory(rows))throw new Error("history");
        if(alive){setHistory(rows);setHistoryFailed(false);}
      }catch{if(alive)setHistoryFailed(true);}
    };
    load();const timer=setInterval(load,60000);
    return()=>{alive=false;clearInterval(timer);controller?.abort();};
  },[historyOpen,device.fresh]);
  useEffect(()=>{
    setArchive(null);setFailed(false);
    if(!selected){setLoading(false);return;}
    let alive=true;const controller=new AbortController();setLoading(true);
    (async()=>{
      try{
        const response=await deviceFetch(`/api/checkpoints?day=${encodeURIComponent(selected)}`,{cache:"no-store",signal:controller.signal});
        const value=await response.json();if(!response.ok||!validCheckpoints(value)||String(value.day)!==selected)throw new Error("archive");
        if(alive)setArchive(value);
      }catch{if(alive)setFailed(true);}finally{if(alive)setLoading(false);}
    })();
    return()=>{alive=false;controller.abort();};
  },[selected]);
  const view=selected?archive:current;
  const rows=view?.rows.slice(shift?5:0,shift?10:5)||[];
  const unavailable=!selected&&!valid;
  const ready=view?.day>0;
  return <section id="checkpoints" className="panel checkpoint-panel" aria-labelledby="checkpoint-title">
    <div className="checkpoint-heading"><div><p className="eyebrow">{t("cp_eyebrow")}</p><h2 id="checkpoint-title">{t("cp_title")}</h2><p>{t("cp_intro")}</p></div>
      <span className={`checkpoint-save ${ready&&view.durable?"good":"bad"}`}>{t(ready&&view.durable?"cp_saved":"cp_storage")}</span>
    </div>
    <div className="checkpoint-tools"><div className="segmented" aria-label={t("cp_shift")}>
      {[0,1].map(value=><button key={value} data-capture-shift={value} aria-pressed={shift===value} onClick={()=>setShift(value)}>{t(value?"cp_evening":"cp_morning")}</button>)}
    </div><span className="mono" id="checkpoint-date">{view?.date||"—"} · WIB</span></div>
    {(!device.fresh||unavailable)&&<p className="hint" role="status">{t("cp_stale")}</p>}
    {ready&&(!view.durable||view.backlog||!view.archived)&&<p className="hint" role="status">{t("cp_storage_help")}</p>}
    {(device.data?.diagnosticActive||device.data?.discoveryRunning)&&<p className="hint">{t("cp_paused")}</p>}
    {loading||failed||!ready?<p className="checkpoint-empty" role="status">{t(loading?"cp_loading":failed?"cp_archive_error":"cp_waiting_clock")}</p>:<div className="checkpoint-scroll"><table className="data-table checkpoint-table">
      <thead><tr><th>{t("cp_scheduled")}</th><th className="amount">{t("cp_kwh")}</th><th>{t("cp_observed")}</th><th>{t("quality")}</th>{differences&&<th className="amount">{t("cp_difference")}</th>}</tr></thead>
      <tbody id="checkpoint-rows">{rows.map(row=><tr key={row.minute} data-minute={row.minute}>
        <td className="mono">{timeOf(row.minute)}</td><td className="amount checkpoint-value">{number(row.kwh)}{row.status==="recorded"&&<small className="checkpoint-delay">{profileName(row.profile,meterProfiles(device.data))}{row.meterId?` · #${row.meterId}`:""}</small>}</td>
        <td className="mono">{observedTime(row.capturedUTC)}{row.capturedUTC!=null&&<small className="checkpoint-delay">+{row.delaySeconds}s</small>}</td>
        <td><span className={`quality checkpoint-${row.status}`}>{t(`cp_${row.status}`)}</span></td>{differences&&<td className="amount">{number(row.difference)}</td>}
      </tr>)}</tbody>
    </table></div>}
    <div className="checkpoint-bottom"><label className="checkbox-line"><input id="checkpoint-differences" type="checkbox" checked={differences} onChange={event=>setDifferences(event.target.checked)} />{t("cp_show_differences")}</label>
      {ready&&!loading?<a id="checkpoint-export" className="button primary" href={`/api/checkpoints?day=${view.day}&format=csv`}><Icon name="download" />{t("cp_export")}</a>:<button className="button" disabled>{t("cp_export")}</button>}
    </div><p className="small muted">{t(differences?"cp_difference_help":"cp_timing_help")}</p>
    <details id="checkpoint-history" onToggle={event=>setHistoryOpen(event.currentTarget.open)}><summary>{t("cp_history")}</summary>
      <label htmlFor="checkpoint-day">{t("archive_date")}</label><select id="checkpoint-day" value={selected} onChange={event=>setSelected(event.target.value)} disabled={!device.fresh}>
        <option value="">{t("cp_today")}{current?.date?` · ${current.date}`:""}</option>
        {history.filter(row=>row.day!==current?.day).map(row=><option key={row.day} value={row.day}>{row.date}</option>)}
      </select>{historyFailed&&<p role="status">{t("cp_archive_error")}</p>}
    </details>
  </section>;
}
