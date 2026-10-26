import { useEffect, useState } from "react";
import { useText } from "../ui.js";
import { validSystem } from "../validation.js";
function ProcessData() {
  const { t } = useText();
  const [data, setData] = useState(null),
    [failed, setFailed] = useState(false);
  useEffect(() => {
    let alive = true,
      timer,
      controller;
    async function poll() {
      controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 4000);
      try {
        const response = await fetch("/api/system", {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error();
        const next = await response.json();
        if (!validSystem(next))
          throw new Error();
        if (alive) {
          setData(next);
          setFailed(false);
        }
      } catch {
        if (alive) setFailed(true);
      } finally {
        clearTimeout(timeout);
        if (alive) timer = setTimeout(poll, 1500);
      }
    }
    poll();
    return () => {
      alive = false;
      clearTimeout(timer);
      controller?.abort();
    };
  }, []);
  const kb = (value) =>
    Number.isFinite(value) ? `${(value / 1024).toFixed(1)} KiB` : "—";
  return (
    <>
      <p>{t("process_intro")}</p>
      {failed && (
        <p role="status" className="warning">
          {t("process_failed")}
        </p>
      )}
      <div className="core-cards">
        {(data?.cores || [{ id: 0 }, { id: 1 }]).map((core) => (
          <article key={core.id}>
            <span className="eyebrow">CORE {core.id}</span>
            <strong>
              {Number.isFinite(core.load) ? `${core.load.toFixed(1)}%` : "—"}
            </strong>
            <progress
              aria-label={`Core ${core.id}`}
              value={core.load || 0}
              max="100"
            />
            <span className="small muted">
              {t(core.id === 0 ? "process_core0" : "process_core1")}
            </span>
          </article>
        ))}
      </div>
      <div className="process-memory">
        {[
          ["process_heap", data?.heapFree],
          ["process_lowheap", data?.heapMin],
          ["process_largest", data?.heapLargest],
          ["process_psram", data?.psramFree],
        ].map(([key, value]) => (
          <div key={key}>
            <span>{t(key)}</span>
            <strong className="mono">{kb(value)}</strong>
          </div>
        ))}
      </div>
      <div className="process-table">
        <table>
          <thead>
            <tr>
              {[
                "process_task",
                "process_core",
                "process_state",
                "process_priority",
                "process_stack",
              ].map((key) => (
                <th key={key}>{t(key)}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {data?.tasks.map((task) => (
              <tr key={task.name}>
                <td>{task.name}</td>
                <td>{task.core}</td>
                <td>{t(`task_${task.state}`)}</td>
                <td>{task.priority}</td>
                <td className="mono">{kb(task.stackFreeMin)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <p className="small muted">
        {t("process_runtime", {
          tasks: data?.taskCount ?? "—",
          mhz: data?.cpuMHz ?? "—",
          ui: data?.uiCycleMs ?? "—",
          uart: data?.acquisitionCycleMs ?? "—",
          queue: data?.commandQueue ?? "—",
          debug: data?.diagnosticQueue ?? "—",
        })}
      </p>
      <div className="hint">{t("process_limits")}</div>
    </>
  );
}
export default function ProcessMonitor() {
  const { t } = useText();
  const [open, setOpen] = useState(false);
  return (
    <details
      id="process-monitor"
      className="panel raw-trace"
      onToggle={(event) => setOpen(event.currentTarget.open)}
    >
      <summary>{t("process_title")}</summary>
      {open ? <ProcessData /> : null}
    </details>
  );
}
