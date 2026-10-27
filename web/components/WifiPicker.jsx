import { useEffect, useState } from "react";
import { useText } from "../ui.js";
import { validWifiScan } from "../validation.js";

export default function WifiPicker({ device, ssid, open, onChoose, onBusy, disabled }) {
  const { t } = useText();
  const [rows, setRows] = useState([]), [phase, setPhase] = useState("idle");
  const [refresh, setRefresh] = useState(0);
  useEffect(() => {
    if (!device.fresh || !device.data?.token) return;
    let alive=true, timer, controller;
    async function request(start=false) {
      controller=new AbortController();const timeout=setTimeout(()=>controller.abort(),5000);
      try {
        if(start){const reply=await fetch("/api/network/scan",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams({token:device.data.token}),signal:controller.signal});if(!reply.ok&&reply.status!==409)throw new Error();}
        const response=await fetch("/api/network/scan",{cache:"no-store",signal:controller.signal});if(!response.ok)throw new Error();
        const data=await response.json();if(!validWifiScan(data))throw new Error();if(!alive)return;
        if(!start&&data.phase!=="scanning"&&(data.phase==="idle"||data.ageMs>30000)){clearTimeout(timeout);await request(true);return;}
        setRows(data.networks);setPhase(data.phase);onBusy(data.phase==="scanning");
        if(data.phase==="scanning")timer=setTimeout(()=>request(),750);
      }catch{if(alive){setPhase("failed");onBusy(false);}}
      finally{clearTimeout(timeout);}
    }
    setPhase("scanning");onBusy(true);request(refresh>0);
    return ()=>{alive=false;clearTimeout(timer);controller?.abort();onBusy(false);};
  },[device.fresh,device.data?.token,refresh]);
  const selected=rows.find((row)=>row.ssid===ssid&&row.open===open);
  const value=selected?`${selected.ssid}/${selected.open}`:ssid?"saved":"";
  return <div className="wifi-picker">
    <div className="wifi-scan-heading"><label htmlFor="network-ssid">{t("network_ssid")}</label><button type="button" id="wifi-rescan" className="button compact" disabled={disabled||phase==="scanning"} onClick={()=>setRefresh((value)=>value+1)}>{t("wifi_refresh")}</button></div>
    <select id="network-ssid" value={value} disabled={disabled||phase==="scanning"} onChange={(event)=>{const row=rows.find((item)=>`${item.ssid}/${item.open}`===event.target.value);if(row)onChoose(row);}} required>
      <option value="">{t("wifi_choose")}</option>
      {ssid&&!selected&&<option value="saved">{ssid} · {t("wifi_saved")}</option>}
      {rows.map((row)=><option key={`${row.ssid}/${row.open}`} value={`${row.ssid}/${row.open}`} disabled={!row.supported}>{row.ssid} · {row.rssi} dBm · {t(!row.supported?"wifi_enterprise":row.open?"wifi_open":"wifi_secured")}</option>)}
    </select>
    <p className="small muted" id="wifi-scan-status" role="status">{t(phase==="scanning"?"wifi_scanning":phase==="failed"?"wifi_scan_failed":rows.length?"wifi_scan_ready":"wifi_scan_empty",{count:rows.length})}</p>
  </div>;
}
