import { meterProfile, meterProfiles } from "../meter-profile.js";
import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
import { deviceFetch } from "../request.js";
import { validFlow } from "../validation.js";
import FlowPlayback from "./FlowPlayback.jsx";
import { FrameAssembler } from "../flow-playback.js";

// Fetch only new device events. Animation never starts without a new UART event.
export default function UartFlow({ device }) {
  const { t } = useText();
  const savedProfile=meterProfile(device?.data);
  const [events, setEvents] = useState([]),
    [failed, setFailed] = useState(false),
    [gap, setGap] = useState(false);
  const [failureReason,setFailureReason]=useState("");
  const [automatic, setAutomatic] = useState(false);
  const [active, setActive] = useState(false),
    [latest, setLatest] = useState(null);
  const profile=latest?.profile==="unknown"?{name:"Modbus device",terminals:"A / B"}:meterProfiles(device?.data).find(p=>p.key===latest?.profile)||savedProfile;
  const [replay,setReplay] = useState(null);
  const assembler = useRef(new FrameAssembler());
  const cursor = useRef(0),
    boot = useRef(null),
    warmed = useRef(false);
  useEffect(() => {
    let alive = true,
      timer,
      controller;
    async function poll() {
      let nextPoll=500;
      if (document.hidden) {
        timer = setTimeout(poll, 500);
        return;
      }
      const requestedCursor = cursor.current;
      controller = new AbortController();
      try {
        const response = await deviceFetch(`/api/uart?after=${cursor.current}`, {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const next = await response.json();
        if (!validFlow(next)) throw new Error("schema");
        if (!alive) return;
        const initial = !warmed.current || boot.current !== next.boot;
        if (boot.current !== next.boot) {
          cursor.current = 0;
          assembler.current.reset();setReplay(null);
          boot.current = next.boot;
          warmed.current = false;
          setEvents([]);
          setLatest(null);
          setGap(false);
          if (requestedCursor !== 0) return;
        }
        nextPoll=next.more?50:500;
        setFailed(false);
        setGap((old) => old || Boolean(next.gap));
        setActive(Boolean(next.diagnosticActive));
        setAutomatic(next.autoDirection !== false);
        const fresh = next.events.filter(
          (e) =>
            !cursor.current ||
            ((e.seq - cursor.current) >>> 0 < 0x80000000 &&
              e.seq !== cursor.current),
        );
        if (!next.more) warmed.current = true;
        if (fresh.length) {
          setEvents((old) => [...(initial ? [] : old), ...fresh].slice(-32));
          setLatest(fresh[fresh.length - 1]);
          if(initial || next.gap) assembler.current.reset();
          else for(const event of fresh) {
            const transaction=assembler.current.accept(event);
            if(transaction && ((next.nowUs-event.atUs)>>>0)<1500000)setReplay(transaction);
          }
          cursor.current = fresh[fresh.length - 1].seq;
        }
      } catch(error) {
        nextPoll=1200;
        if (alive){assembler.current.reset();setReplay(null);setFailed(true);setFailureReason(error.name==="AbortError"?"timeout":error.message==="schema"?"schema":"network");}
      } finally {
        if (alive) timer = setTimeout(poll, nextPoll);
      }
    }
    poll();
    return () => {
      alive = false;
      clearTimeout(timer);
      controller?.abort();
      assembler.current.reset();
    };
  }, []);
  const kind = latest?.kind;
  const phase = failed
    ? "flow_disconnected"
    : !latest
      ? "flow_waiting"
      : kind === 1
        ? "flow_transmitting"
        : kind === 2
          ? "flow_waiting_rx"
          : kind === 3
            ? "flow_receiving"
            : kind === 6
              ? "trace_result_timeout"
              : kind === 5
                ? "trace_result_short"
                : kind === 8
                  ? "local_uart_error"
                  : kind === 4
                ? `trace_result_${latest.result}`
                : "flow_idle";
  const lastRequest = [...events].reverse().find((event) => event.kind === 1);
  const transaction = lastRequest ? events.filter((event) => ((event.seq-lastRequest.seq)>>>0)<0x80000000) : [];
  const tx = transaction.find((event) => event.kind === 2);
  const rx = [...transaction].reverse().find((event) => [4,5].includes(event.kind));
  const completed = [...transaction].reverse().find((event) => [4,5,6,8].includes(event.kind));
  const dirPin = latest?.dirPin ?? (automatic ? -1 : 16);
  const high = kind === 1;
  const elapsed = (from,to) => from && to ? `${(((to.atUs-from.atUs)>>>0)/1000).toFixed(2)} ms` : "—";
  const steps = [1,2,3,4].map((step) => transaction.find((event) => step === 4 ? [4,5,6,8].includes(event.kind) : event.kind === step));
  return (
    <section className="flow-panel" id="uart-flow" aria-labelledby="flow-title">
      <div className="flow-head">
        <div><span className="eyebrow">UART 1 / RS485</span><h2 id="flow-title">{t("flow_title")}</h2></div>
        <span className={`flow-phase ${failed ? "failed" : ""}`} id="flow-phase" role="status">
          {t(phase)}{failed && <small className="flow-reason">{t(`flow_reason_${failureReason}`)}</small>}
        </span>
      </div>
      <div className="flow-instruments">
        <div><span>{t("flow_link")}</span><strong>{latest ? `ID ${latest.slave} · ${latest.baud} / 8${latest.parity}${latest.stopBits||1}` : "—"}</strong></div>
        <div><span>{t("flow_direction")}</span><strong id="flow-direction">{failed || !latest || [7,8].includes(kind) ? t("unknown") : dirPin < 0 ? t("flow_auto") : `DE = ${high ? 1 : 0} · /RE = ${high ? 1 : 0}`}</strong></div>
        <div><span>{t("flow_result")}</span><strong id="flow-result">{completed ? t(completed.kind === 8 ? "local_uart_error" : completed.kind === 5 ? "trace_result_short" : `trace_result_${completed.result}`) : t("flow_waiting")}</strong></div>
      </div>
      <div className="flow-equipment-grid" aria-label={t("flow_diagram")}>
        {[
          {title:"ESP32-S3",detail:`TX${latest?.txPin ?? 17} / RX${latest?.rxPin ?? 18}`,box:"60 42 80 80",paths:["M75 57H125V107H75ZM86 70H114V94H86ZM83 47V57M100 47V57M117 47V57M83 107V117M100 107V117M117 107V117M65 65H75M65 83H75M65 100H75M125 65H135M125 83H135M125 100H135"]},
          {title:"RS485",detail:dirPin<0?"AUTO":`GPIO${dirPin} · DE + /RE`,box:"505 43 90 90",paths:["M520 60L551 78L520 96ZM580 78L549 96L580 114ZM510 78H520M551 78H590M510 96H549M580 96H590M550 53V65M550 108V120"]},
          {title:profile.name,detail:profile.terminals,box:"955 45 90 90",paths:["M970 55H1030V115H970ZM979 64H1021V84H979ZM979 98H1021M989 94V102M1000 94V102M1011 94V102","M984 75H989L993 68L999 80L1004 73H1016"]},
        ].map(node=><div className="flow-device" key={node.title}><svg className="flow-symbol" viewBox={node.box} aria-hidden="true">{node.paths.map(path=><path key={path} d={path}/>)}</svg><strong>{node.title}</strong><code>{node.detail}</code></div>)}
      </div>
      <FlowPlayback meterName={profile.name} profiles={meterProfiles(device?.data)} item={replay} enabled={!failed} resetKey={boot.current} />
      <h3 className="flow-observed-title">{t("flow_observed")}</h3>
      <ol className="flow-timeline" aria-label={t("flow_timeline")}>
        {steps.map((event,index) => <li key={index} className={event ? "observed" : ""}><span className="flow-step">0{index+1}</span><div><strong>{t(`flow_step_${index+1}`)}</strong><span>{event ? index === 0 ? `#${event.seq}` : elapsed(steps[0],event) : "—"}</span></div></li>)}
      </ol>
      <div className="flow-frames">
        <div><span className="flow-frame-label">TX <small>{tx ? `${tx.bytes.split(" ").length} B` : "—"}</small></span><code>{tx?.bytes || t("flow_no_tx")}</code></div>
        <div><span className="flow-frame-label">RX <small>{rx?.bytes ? `${rx.bytes.split(" ").length}${rx.overflow ? "+" : ""} B` : "—"}</small></span><code>{rx?.bytes || t(completed?.kind === 6 ? "trace_no_rx" : "flow_no_rx")}</code></div>
      </div>
      <div className="flow-foot">
        <span>{active ? t("flow_diagnostic") : t("flow_production")}</span>
        <span className="mono">
          {latest
            ? `#${latest.seq} · ${(latest.atUs / 1000000).toFixed(6)} s`
            : "—"}
        </span>
      </div>
      <p className="flow-note">{t("flow_note")}</p>
      {gap && <p className="flow-note warning">{t("flow_gap")}</p>}
      <details>
        <summary>{t("flow_events")}</summary>
        <div className="flow-events">
          {events
            .slice()
            .reverse()
            .map((event) => (
              <div className="flow-event" key={`${boot.current}-${event.seq}`}>
                <span className="mono">
                  #{event.seq} · {(event.atUs / 1000000).toFixed(6)} s
                </span>
                <span>
                  {t(event.diagnostic ? "trace_diagnostic" : "flow_production")}{" "}
                  · {t(`flow_kind_${event.kind}`)} · ID {event.slave} ·{" "}
                  {event.baud} / 8{event.parity}1 · 0x
                  {event.address.toString(16).padStart(4, "0")}
                </span>
                <code>
                  {event.bytes || "—"}
                  {event.overflow ? " …" : ""}
                </code>
              </div>
            ))}
        </div>
      </details>
    </section>
  );
}
