import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";

// Fetch only new device events. Animation never starts without a new UART event.
export default function UartFlow() {
  const { t } = useText();
  const [events, setEvents] = useState([]),
    [failed, setFailed] = useState(false),
    [gap, setGap] = useState(false);
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
      const dot = document.createElementNS(
        "http://www.w3.org/2000/svg",
        "circle",
      );
      dot.setAttribute("r", "4");
      dot.setAttribute(
        "fill",
        event.kind === 2
          ? "#78e5d3"
          : event.result === "valid"
            ? "#78e5d3"
            : "#efb276",
      );
      dot.setAttribute("data-event", String(event.seq));
      const y = event.kind === 2 ? 100 : 140,
        start = event.kind === 2 ? 130 : 790,
        end = event.kind === 2 ? 790 : 130;
      dot.setAttribute("cy", String(y));
      dot.setAttribute("cx", String(start));
      svg.current.append(dot);
      // Actual observed window at 1x. Afterglow aids visibility; it is not extra traffic.
      const movement = dot.animate(
        [
          { transform: "translateX(0px)" },
          { transform: `translateX(${end - start}px)` },
        ],
        { duration: Math.max(1, duration), fill: "forwards" },
      );
      animations.add(movement);
      movement.onfinish = () => {
        animations.delete(movement);
        if (!alive) return;
        const fade = dot.animate([{ opacity: 1 }, { opacity: 0 }], {
          duration: 250,
        });
        animations.add(fade);
        fade.onfinish = () => {
          animations.delete(fade);
          dot.remove();
        };
      };
    }
    async function poll() {
      if (document.hidden) {
        timer = setTimeout(poll, 500);
        return;
      }
      const requestedCursor = cursor.current;
      controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 2500);
      try {
        const response = await fetch(`/api/uart?after=${cursor.current}`, {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error("flow");
        const next = await response.json();
        if (!Array.isArray(next.events) || next.events.length > 32)
          throw new Error("schema");
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
        if (requestedCursor === 0) warmed.current = true;
        if (fresh.length) {
          setEvents((old) => [...(initial ? [] : old), ...fresh].slice(-12));
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
      } catch {
        if (alive) setFailed(true);
      } finally {
        clearTimeout(timeout);
        if (alive) timer = setTimeout(poll, 200);
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
  return (
    <section className="flow-panel" id="uart-flow" aria-labelledby="flow-title">
      <div className="flow-head">
        <div>
          <span className="eyebrow">UART 1 / RS485</span>
          <h2 id="flow-title">{t("flow_title")}</h2>
        </div>
        <span
          className={`flow-phase ${failed ? "failed" : ""}`}
          id="flow-phase"
        >
          {t(phase)}
        </span>
      </div>
      <svg
        ref={svg}
        viewBox="0 0 920 210"
        role="img"
        aria-label={t("flow_diagram")}
      >
        <defs>
          <linearGradient id="flow-line">
            <stop stopColor="#57cdbb" stopOpacity=".75" />
            <stop offset="1" stopColor="#57cdbb" stopOpacity=".12" />
          </linearGradient>
        </defs>
        <path
          d="M130 100H790 M130 140H790"
          fill="none"
          stroke="url(#flow-line)"
          strokeWidth="1.5"
        />
        {[
          {
            x: 65,
            label: "ESP32-S3",
            sub: `TX${latest?.txPin ?? 17} / RX${latest?.rxPin ?? 18}`,
          },
          {
            x: 405,
            label: "RS485",
            sub: automatic ? "AUTO DIRECTION" : "GPIO16 / DIR",
          },
          { x: 745, label: "FORT", sub: "58/A · 59/B" },
        ].map((node) => (
          <g key={node.x}>
            <rect x={node.x - 20} y="35" width="130" height="135" rx="12" />
            <text x={node.x + 45} y="73" textAnchor="middle">
              {node.label}
            </text>
            <text
              className="flow-sub"
              x={node.x + 45}
              y="157"
              textAnchor="middle"
            >
              {node.sub}
            </text>
          </g>
        ))}
        <text x="275" y="91" textAnchor="middle" className="flow-sub">
          TX →
        </text>
        <text x="615" y="132" textAnchor="middle" className="flow-sub">
          ← RX
        </text>
      </svg>
      <div className="flow-node-labels">
        <span>ESP32-S3</span>
        <span>RS485</span>
        <span>FORT</span>
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
