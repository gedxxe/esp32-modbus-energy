import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
import { deviceFetch } from "../request.js";
import { validFlow } from "../validation.js";

// Fetch only new device events. Animation never starts without a new UART event.
export default function UartFlow() {
  const { t } = useText();
  const [events, setEvents] = useState([]),
    [failed, setFailed] = useState(false),
    [gap, setGap] = useState(false);
  const [failureReason,setFailureReason]=useState("");
  const [automatic, setAutomatic] = useState(true);
  const [active, setActive] = useState(false),
    [latest, setLatest] = useState(null);
  const svg = useRef(null),
    cursor = useRef(0),
    boot = useRef(null),
    previous = useRef(new Map()),
    warmed = useRef(false);
  useEffect(() => {
    let alive = true,
      timer,
      controller;
    const animations = new Set(),
      replayTimers = new Set();
    const reduced = matchMedia("(prefers-reduced-motion: reduce)");
    function pulse(event, duration) {
      if (!svg.current || reduced.matches || !alive) return;
      const ns = "http://www.w3.org/2000/svg", packet = document.createElementNS(ns, "g");
      const tx = event.kind === 2, start = tx ? 100 : 1000, end = tx ? 1000 : 100;
      packet.setAttribute("data-event", String(event.seq));
      packet.setAttribute("class", `flow-packet ${tx ? "tx" : "rx"}${!tx && event.result !== "valid" ? " fault" : ""}`);
      packet.setAttribute("transform", `translate(${start},${tx ? 210 : 260})`);
      const box = document.createElementNS(ns, "rect");
      for (const [key,value] of Object.entries({x:-44,y:-14,width:88,height:28,rx:8})) box.setAttribute(key,String(value));
      const label = document.createElementNS(ns, "text");
      label.setAttribute("text-anchor","middle"); label.setAttribute("y","5");
      label.textContent = `${tx ? "TX →" : "← RX"} ${event.bytes ? event.bytes.split(" ").length : 0}B`;
      packet.append(box,label); svg.current.append(packet);
      const movement = packet.animate([
        { transform: `translate(${start}px,${tx ? 210 : 260}px)` },
        { transform: `translate(${end}px,${tx ? 210 : 260}px)` },
      ], { duration: Math.max(1,duration), fill:"forwards" });
      animations.add(movement);
      movement.onfinish = () => {
        animations.delete(movement);
        packet.setAttribute("transform", `translate(${end},${tx ? 210 : 260})`);
        movement.cancel();
        if (!alive) return;
        const fade = packet.animate([{opacity:1},{opacity:1,offset:.65},{opacity:0}],{duration:700});
        animations.add(fade);
        fade.onfinish = () => { animations.delete(fade); packet.remove(); };
      };
    }
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
          previous.current.clear();
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
          for (const event of fresh) {
            const key = `${event.slave}/${event.baud}/${event.parity}/${event.address}`;
            if (previous.current.size > 64) previous.current.clear();
            if (event.kind === 1 || event.kind === 3)
              previous.current.set(`${key}/${event.kind}`, event.atUs);
            if (event.kind === 2 || event.kind === 4) {
              const origin = previous.current.get(
                `${key}/${event.kind === 2 ? 1 : 3}`,
              );
              if (
                !initial &&
                origin !== undefined &&
                !next.gap &&
                (next.nowUs - event.atUs) >>> 0 < 1500000
              ) {
                const relative = (origin - fresh[0].atUs) >>> 0;
                const delay = relative < 0x80000000 ? relative / 1000 : 0;
                const replay = setTimeout(() => {
                  replayTimers.delete(replay);
                  pulse(event, ((event.atUs - origin) >>> 0) / 1000);
                }, delay);
                replayTimers.add(replay);
              }
              previous.current.delete(`${key}/${event.kind === 2 ? 1 : 3}`);
            }
          }
          cursor.current = fresh[fresh.length - 1].seq;
        }
      } catch(error) {
        nextPoll=1200;
        if (alive){setFailed(true);setFailureReason(error.name==="AbortError"?"timeout":error.message==="schema"?"schema":"network");}
      } finally {
        if (alive) timer = setTimeout(poll, nextPoll);
      }
    }
    poll();
    return () => {
      alive = false;
      clearTimeout(timer);
      controller?.abort();
      for (const animation of animations) animation.cancel();
      for (const replay of replayTimers) clearTimeout(replay);
      previous.current.clear();
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
        <div><span>{t("flow_link")}</span><strong>{latest ? `ID ${latest.slave} · ${latest.baud} / 8${latest.parity}1` : "—"}</strong></div>
        <div><span>{t("flow_direction")}</span><strong id="flow-direction">{failed || !latest || [7,8].includes(kind) ? t("unknown") : dirPin < 0 ? t("flow_auto") : `DE = ${high ? 1 : 0} · /RE = ${high ? 1 : 0}`}</strong></div>
        <div><span>{t("flow_result")}</span><strong id="flow-result">{completed ? t(completed.kind === 8 ? "local_uart_error" : completed.kind === 5 ? "trace_result_short" : `trace_result_${completed.result}`) : t("flow_waiting")}</strong></div>
      </div>
      <div className="flow-diagram" tabIndex="0" aria-label={t("flow_diagram")}>
        <svg ref={svg} viewBox="0 0 1100 300" role="img" aria-label={t("flow_diagram")}>
          <defs>
            <linearGradient id="flow-card" x2="1" y2="1"><stop stopColor="#304d84"/><stop offset="1" stopColor="#192f5e"/></linearGradient>
            <marker id="flow-arrow-tx" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M1 1L9 5L1 9"/></marker>
            <marker id="flow-arrow-rx" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M1 1L9 5L1 9"/></marker>
          </defs>
          <path className="flow-tap" d="M100 164V260M550 164V260M1000 164V260"/>
          <path className="flow-lane tx" d="M100 210H1000" markerEnd="url(#flow-arrow-tx)"/>
          <path className="flow-lane rx" d="M1000 260H100" markerEnd="url(#flow-arrow-rx)"/>
          <g className="flow-equipment">
            <rect x="8" y="12" width="184" height="152" rx="19"/>
            <rect x="458" y="12" width="184" height="152" rx="19"/>
            <rect x="908" y="12" width="184" height="152" rx="19"/>
          </g>
          <g className="flow-symbol">
            <path d="M75 57H125V107H75ZM86 70H114V94H86ZM83 47V57M100 47V57M117 47V57M83 107V117M100 107V117M117 107V117M65 65H75M65 83H75M65 100H75M125 65H135M125 83H135M125 100H135"/>
            <path d="M520 60L551 78L520 96ZM580 78L549 96L580 114ZM510 78H520M551 78H590M510 96H549M580 96H590M550 53V65M550 108V120"/>
            <path d="M970 55H1030V115H970ZM979 64H1021V84H979ZM979 98H1021M989 94V102M1000 94V102M1011 94V102"/>
            <path d="M984 75H989L993 68L999 80L1004 73H1016"/>
          </g>
          <g className="flow-label"><text x="100" y="37">ESP32-S3</text><text x="550" y="37">RS485</text><text x="1000" y="37">FORT</text></g>
          <g className="flow-pin"><text x="100" y="143">TX{latest?.txPin ?? 17} / RX{latest?.rxPin ?? 18}</text><text x="550" y="143">{dirPin < 0 ? "AUTO" : `GPIO${dirPin} · DE + /RE`}</text><text x="1000" y="143">58 / A · 59 / B</text></g>
          <g className="flow-lane-label"><text x="325" y="198">TX · TTL → DI</text><text x="775" y="198">{t("flow_request")} → A/B</text><text x="325" y="285">RX · RO → TTL</text><text x="775" y="285">← {t("flow_response")}</text></g>
        </svg>
      </div>
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
