import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
export default function DisplayControls({ device }) {
 const {t}=useText();const raw=device.data?.display;
 const display=raw&&Number.isInteger(raw.idleSeconds)&&(raw.idleSeconds===0||(raw.idleSeconds>=10&&raw.idleSeconds<=3600))&&[raw.available,raw.stored,raw.storageOK,raw.sleeping].every(value=>typeof value==="boolean")?raw:null;
 const [enabled,setEnabled]=useState(true),[seconds,setSeconds]=useState(60);const previous=useRef(null);
 useEffect(()=>{if(display&&display.idleSeconds!==previous.current){previous.current=display.idleSeconds;setEnabled(display.idleSeconds!==0);if(display.idleSeconds)setSeconds(display.idleSeconds);}},[display?.idleSeconds]);
 const blocked=!display||!device.fresh||device.pending;
 return <section id="display-panel" className="panel"><h2>{t("display_title")}</h2><p>{t("display_intro")}</p>
  <form onSubmit={(event)=>{event.preventDefault();device.act("/api/display",{idleSeconds:enabled?seconds:0});}}>
   <label className="checkbox-line" htmlFor="display-enabled"><input id="display-enabled" type="checkbox" checked={enabled} disabled={blocked} onChange={(event)=>setEnabled(event.target.checked)}/>{t("display_enabled")}</label>
   <label htmlFor="display-idle">{t("display_idle")}</label><input id="display-idle" type="number" min="10" max="3600" step="1" value={seconds} required disabled={blocked||!enabled} onChange={(event)=>setSeconds(event.target.value)} />
   <p className="small muted">{t("display_range")}</p><div className="actions"><button id="display-save" className="button primary" disabled={blocked}>{t("display_save")}</button></div>
  </form><p id="display-state" className="small muted" role="status">{t(!display?"display_unavailable":!display.available?"display_absent":display.sleeping?"display_asleep":"display_awake")}</p>
  {display&&!display.storageOK&&<p className="hint">{t("display_storage_failed")}</p>}
  <div className="hint">{t("display_wake")}</div>
 </section>;
}
