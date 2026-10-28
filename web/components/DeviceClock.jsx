import { memo, useEffect, useState } from "react";
import Counter from "../vendor/react-bits/Counter.jsx";
import { useText } from "../ui.js";
const RollingCounter=memo(Counter);
const CLOCK_FORMAT={useGrouping:false};
// Presentation clock only. It never posts a time or changes the RTC.
export default function DeviceClock({ device, id }) {
  const { t }=useText();
  const [anchor,setAnchor]=useState(null),[now,setNow]=useState(()=>performance.now());
  const data=device.data;
  const valid=data?.rtc&&Number.isInteger(data.clockEpoch)&&data.clockEpoch>=1704067200&&data.clockEpoch<4102444800&&Number.isFinite(data.clockSampleAgeMs)&&data.clockSampleAgeMs>=0&&data.clockSampleAgeMs<=0xffffffff;
  useEffect(()=>{
    const at=performance.now();
    if(!device.fresh)return;
    setAnchor(previous=>!valid?null:(!previous||previous.token!==data.token||previous.revision!==data.clockRevision||at-previous.synced>=300000||at-previous.lastSeen>=15000)
      ?{epoch:data.clockEpoch+data.clockSampleAgeMs/1000,at,synced:at,token:data.token,revision:data.clockRevision,lastSeen:at}:{...previous,lastSeen:at});
  },[valid,device.fresh,data]);
  useEffect(()=>{const timer=setInterval(()=>setNow(performance.now()),250);return()=>clearInterval(timer);},[]);
  const epoch=valid&&anchor&&now-anchor.lastSeen<15000?anchor.epoch+Math.max(0,now-anchor.at)/1000+7*3600:null;
  const text=epoch==null?"—":new Date(epoch*1000).toISOString().slice(11,19);
  return <span id={id} className="device-clock" role="timer" aria-live="off" aria-label={text==="—"?t("rtc_unset"):`${text} WIB`} data-time={text}>
    {epoch==null ? "—" : <span className="clock-digits" aria-hidden="true">{text.split(":").map((part,index)=><span className="clock-part" key={index}>{index>0&&<span className="clock-separator">:</span>}<span className="clock-unit"><RollingCounter value={Number(part)} minDigits={2} format={CLOCK_FORMAT} fontSize="inherit" fontWeight="inherit" duration={0.3} bounce={0} stagger={0} motionBlur={false} intro={false} fade={0.08} direction="up" /></span></span>)}</span>}
  </span>;
}
