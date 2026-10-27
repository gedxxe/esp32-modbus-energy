import { useEffect, useState } from "react";
// Presentation clock only. It never posts a time or changes the RTC.
export default function DeviceClock({ device, id }) {
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
  return <span id={id} aria-label="WIB">{text}</span>;
}
