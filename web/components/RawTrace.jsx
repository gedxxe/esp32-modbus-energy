import { useEffect, useState } from "react";
import { useText } from "../ui.js";

function TraceData({ device }) {
  const { t } = useText();
  const [capture, setCapture] = useState(null);
  const [failed, setFailed] = useState(false);
  useEffect(() => {
    let alive = true,
      timer,
      controller;
    const poll = async () => {
      controller = new AbortController();
      const deadline = setTimeout(() => controller.abort(), 4000);
      try {
        const response = await fetch("/api/trace", {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error("trace");
        const next = await response.json();
        if (
          !Array.isArray(next.frames) ||
          next.frames.length > 12 ||
          typeof next.active !== "boolean"
        )
          throw new Error("schema");
        if (alive) {
          setCapture(next);
          setFailed(false);
        }
      } catch {
        if (alive) setFailed(true);
      } finally {
        clearTimeout(deadline);
        if (alive) timer = setTimeout(poll, 1000);
      }
    };
    poll();
    return () => {
      alive = false;
      clearTimeout(timer);
      controller?.abort();
    };
  }, []);
  const blocked = !device.fresh || device.pending || !capture || failed;
  const download = () => {
    const lines = [
      `Fort raw capture | firmware ${capture.version} | capture ${capture.generation}`,
      `ESP32 TX GPIO${capture.txPin} / RX GPIO${capture.rxPin}`,
      "Times are milliseconds since boot; UART-buffer observations, not a wire-level trace.",
      "",
    ];
    capture.frames.forEach((frame, index) =>
      lines.push(
        `#${index + 1} t=${frame.atMs}ms ID=${frame.slave} ${frame.baud} 8${frame.parity}1 address=0x${frame.address.toString(16).padStart(4, "0")} words=${frame.words} attempt=${frame.attempt} source=${frame.diagnostic ? "diagnostic" : frame.scan ? "scan" : "poll"} result=${frame.result} overflow=${frame.overflow}`,
        `TX (${frame.tx.split(" ").length} bytes): ${frame.tx}`,
        `RX (${frame.rxBytes} captured bytes): ${frame.rx || "<none>"}`,
        "",
      ),
    );
    const url = URL.createObjectURL(
      new Blob([lines.join("\n")], { type: "text/plain;charset=utf-8" }),
    );
    const link = document.createElement("a");
    link.href = url;
    link.download = `fort-raw-${capture.generation}.txt`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };
  return (
    <>
      <p>{t("trace_intro")}</p>
      <div className="actions">
        <button
          id="trace-start"
          className="button primary"
          disabled={blocked || capture.active}
          onClick={() => device.act("/api/trace/start")}
        >
          {t("trace_start")}
        </button>
        <button
          id="trace-stop"
          className="button"
          disabled={blocked || !capture.active}
          onClick={() => device.act("/api/trace/stop")}
        >
          {t("trace_stop")}
        </button>
        <button
          id="trace-download"
          className="button"
          disabled={!capture?.frames.length}
          onClick={download}
        >
          {t("trace_download")}
        </button>
      </div>
      <p id="trace-status" role="status" className="small muted">
        {failed
          ? t("trace_failed")
          : !capture
            ? t("connecting")
            : t(
                capture.active
                  ? "trace_recording"
                  : capture.generation
                    ? "trace_frozen"
                    : "trace_empty",
                { count: capture.count, capacity: capture.capacity },
              )}
      </p>
      <div id="trace-frames" className="trace-frames">
        {capture?.frames.map((frame, index) => (
          <article
            className="trace-frame"
            key={`${capture.generation}-${index}`}
          >
            <div className="trace-frame-head">
              <strong>
                #{index + 1} · ID {frame.slave} · {frame.baud} / 8{frame.parity}
                1
              </strong>
              <span>{t("trace_result_" + frame.result)}</span>
            </div>
            <p className="small muted mono">
              {frame.atMs} ms · 0x
              {frame.address.toString(16).padStart(4, "0").toUpperCase()} ·{" "}
              {t(
                frame.diagnostic
                  ? "trace_diagnostic"
                  : frame.scan
                    ? "trace_scan"
                    : "trace_poll",
              )}{" "}
              · {t("trace_attempt", { count: frame.attempt })}
            </p>
            <div className="trace-bytes">
              <b>TX</b>
              <code>{frame.tx}</code>
              <b>RX</b>
              <code>
                {frame.rx || t("trace_no_rx")}
                {frame.overflow ? " …" : ""}
              </code>
            </div>
          </article>
        ))}
      </div>
      <div className="hint">{t("trace_note")}</div>
    </>
  );
}

export default function RawTrace({ device }) {
  const { t } = useText();
  const [open, setOpen] = useState(false);
  return (
    <details
      id="raw-trace"
      className="panel raw-trace"
      onToggle={(event) => setOpen(event.currentTarget.open)}
    >
      <summary>{t("trace_title")}</summary>
      {open ? <TraceData device={device} /> : null}
    </details>
  );
}
