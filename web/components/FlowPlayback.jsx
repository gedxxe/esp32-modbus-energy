import { useEffect, useRef, useState } from "react";
import { useReducedMotion } from "motion/react";
import WakeSlider from "../vendor/react-bits/WakeSlider.jsx";
import { flowLeg, flowPhaseDuration, nextFlowPhase } from "../flow-playback.js";
import { useText } from "../ui.js";

export default function FlowPlayback({ item, enabled, resetKey }) {
  const { t } = useText();
  const reduced = useReducedMotion();
  const [view,setView] = useState({ job:null,phase:"idle",progress:0 });
  const actor = useRef(null), pending = useRef(null), raf = useRef(0), handled = useRef(null);
  const allowed = useRef(false);
  allowed.current = enabled && !reduced && !document.hidden;
  function tick(now) {
    raf.current = 0;
    const run = actor.current;
    if (!run || !allowed.current) return;
    // A delayed browser frame cannot teleport the train to the opposite end.
    run.elapsed += Math.min(48,Math.max(0,now-run.last)); run.last = now;
    if (run.elapsed >= flowPhaseDuration(run.phase)) {
      const next=nextFlowPhase(run.phase,Boolean(run.job.rx));
      run.elapsed = 0;
      if(next)run.phase=next;
      else {
        actor.current = null;
        if (pending.current) { const pendingJob=pending.current;pending.current=null;start(pendingJob,now);return; }
        setView({ job:run.job,phase:"idle",progress:1 });return;
      }
    }
    setView({ job:run.job,phase:run.phase,progress:run.elapsed/flowPhaseDuration(run.phase) });
    raf.current=requestAnimationFrame(tick);
  }
  function start(job,now=performance.now()) {
    actor.current={ job,phase:"tx",elapsed:0,last:now };
    setView({ job,phase:"tx",progress:0 });
    if (!raf.current) raf.current=requestAnimationFrame(tick);
  }
  useEffect(() => {
    const stop = (publish=true) => {
      cancelAnimationFrame(raf.current);raf.current=0;actor.current=pending.current=null;
      if(publish)setView({ job:null,phase:"idle",progress:0 });
    };
    stop();handled.current=null;
    const visibility = () => { if(document.hidden)stop(); };
    document.addEventListener("visibilitychange",visibility);
    return () => { stop(false);document.removeEventListener("visibilitychange",visibility); };
  },[resetKey,enabled,reduced]);
  useEffect(() => {
    if (!item || handled.current === item.key) return;
    handled.current=item.key;
    if (!allowed.current) return;
    if (actor.current) pending.current=item; // One active + newest pending, never a growing backlog.
    else start(item);
  },[item]);
  const job=view.job;
  return <div className="flow-playback" data-phase={view.phase} data-transaction={job?.key ?? ""}>
    <div className="flow-playback-heading"><span className="flow-replay-badge">{t("flow_readable")}</span><span className="mono">{job ? `#${job.request.seq}–${job.outcome.seq} · FC03 0x${job.request.address.toString(16).padStart(4,"0")}` : t("flow_waiting")}</span></div>
    {["tx","rx"].map(direction => {
      const transmitting=direction==="tx",event=transmitting?job?.tx:job?.rx;
      const leg=flowLeg(view.phase,view.progress,direction);
      const visible=Boolean(event)&&leg.visible;
      const percent=leg.position*100;
      const coordinate=transmitting?percent:100-percent;
      const fault=!transmitting&&event&&event.result!=="valid";
      return <div key={direction} className={`flow-rail ${direction}${fault?" fault":""}`} data-active={visible}>
        <div className="flow-rail-heading"><strong>{transmitting?"TX →":"← RX"}</strong><span>{t(transmitting?"flow_request":"flow_response")}</span><code>{event ? `${event.bytes.split(" ").length}${event.overflow?"+":""} B` : "—"}</code></div>
        <div className="flow-track" role="img" aria-label={t(transmitting?"flow_tx_path":"flow_rx_path")}>
          <div className="flow-rest-bars" aria-hidden="true">{Array.from({length:44},(_,index)=><span key={index}/>)}</div>
          <div className="flow-wake" data-running={visible} style={{opacity:visible?leg.opacity:0}} aria-hidden="true" inert>
            <WakeSlider key={`${job?.key}/${direction}`} value={percent} min={0} max={100} bars={44} height={40} restHeight={4} gap={4} glide={0.08} smoothing={60} sensitivity={2.3} reach={9} skew={0.5} fillColor={fault?"#ffd19c":transmitting?"#88caff":"#7ce5d2"} trackColor="#a1c8ff26" crestColor={fault?"#ffe6c4":transmitting?"#d4edff":"#c0fff0"} disabled />
          </div>
          {visible && <span key={`${job.key}/${direction}`} className={`flow-train ${direction}`} data-event={event.seq} data-settling={view.phase===`${direction}-settle`} style={{opacity:leg.opacity,left:`${coordinate}%`,transform:`translateX(-${coordinate}%)`}}>{transmitting?"TX →":"← RX"}<small>{event.bytes.split(" ").length} B</small></span>}
        </div>
        <div className="flow-rail-endpoints"><span>{transmitting?"ESP32 · UART TX":"ESP32 · UART RX"}</span><span>{transmitting?"FORT · A/B":"FORT · A/B"}</span></div>
      </div>;
    })}
    {job && !job.rx && <p className="flow-no-response">{t("trace_no_rx")}</p>}
  </div>;
}
