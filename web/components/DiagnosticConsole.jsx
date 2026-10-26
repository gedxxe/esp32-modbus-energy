import { useEffect, useState } from "react";
import { useText } from "../ui.js";
const recipes = [
  ["diagnose", "diagnose"],
  ["basic", "basic"],
  ["probe", "intermediate"],
  ["map", "deeper"],
  ["rx", "rxcheck"],
  ["report", "report"],
  ["connected", "check connected"],
  ["isolated", "check isolated"],
  ["compare", "compare"],
];
function ConsoleData({ device }) {
  const { t } = useText();
  const [view, setView] = useState(null),
    [command, setCommand] = useState("diagnose"),
    [failed, setFailed] = useState(false);
  useEffect(() => {
    let alive = true,
      timer,
      controller;
    async function poll() {
      controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 4000);
      try {
        const response = await fetch("/api/diagnostic", {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error();
        const body = await response.json();
        if (typeof body.log !== "string" || body.log.length > 8192)
          throw new Error();
        if (alive) {
          setView(body);
          setFailed(false);
        }
      } catch {
        if (alive) setFailed(true);
      } finally {
        clearTimeout(timeout);
        if (alive) timer = setTimeout(poll, 1000);
      }
    }
    poll();
    return () => {
      alive = false;
      clearTimeout(timer);
      controller?.abort();
    };
  }, []);
  const blocked =
    !view ||
    !device.fresh ||
    device.pending ||
    device.data?.scan?.phase === "running" ||
    view?.busy ||
    view?.pendingCommands > 0 ||
    failed;
  function send(line) {
    device.act("/api/diagnostic", { command: line }, { priority: ["x", "stop", "resume"].includes(line) });
  }
  function download() {
    const url = URL.createObjectURL(
      new Blob(
        [
          `Fort firmware ${view.version} | session ${view.generation}\nOld log bytes discarded: ${view.discardedBytes}\n\n${view.log}`,
        ],
        { type: "text/plain;charset=utf-8" },
      ),
    );
    const link = document.createElement("a");
    link.href = url;
    link.download = `fort-diagnostic-${view.generation}.txt`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  return (
    <>
      <p>{t("diag_intro")}</p>
      <div className="hint">{t("diag_pause")}</div>
      <div className="diag-recipes">
        {recipes.map(([label, line]) => (
          <button
            key={label}
            type="button"
            disabled={blocked}
            onClick={() => setCommand(line)}
          >
            {t(`diag_${label}`)}
          </button>
        ))}
      </div>
      <form
        className="diag-input"
        onSubmit={(event) => {
          event.preventDefault();
          send(command);
        }}
      >
        <label htmlFor="diagnostic-command">{t("diag_input")}</label>
        <div>
          <input
            id="diagnostic-command"
            className="mono"
            value={command}
            maxLength="95"
            pattern="[ -~]+"
            required
            onChange={(event) => setCommand(event.target.value)}
            disabled={blocked}
            autoCapitalize="none"
            autoCorrect="off"
            spellCheck="false"
            onPaste={(event) => {
              if (/[\r\n]/.test(event.clipboardData.getData("text"))) {
                event.preventDefault();
                device.setNotice({ key: "invalid_diagnostic_command" });
              }
            }}
          />
          <button
            id="diagnostic-send"
            className="button primary"
            disabled={blocked}
          >
            {t("diag_run")}
          </button>
        </div>
      </form>
      <div className="actions">
        <button
          id="diagnostic-stop"
          type="button"
          className="button danger"
          disabled={!device.fresh || device.priorityPending}
          onClick={() => send("x")}
        >
          {t("diag_stop")}
        </button>
        <button
          id="diagnostic-resume"
          type="button"
          className="button"
          disabled={!device.fresh || device.priorityPending || !view?.active}
          onClick={() => send("resume")}
        >
          {t("diag_resume")}
        </button>
        <button
          id="diagnostic-download"
          type="button"
          className="button"
          disabled={!view?.log}
          onClick={download}
        >
          {t("diag_download")}
        </button>
      </div>
      <p id="diagnostic-status" role="status" className="small muted">
        {failed
          ? t("diag_failed")
          : view?.pendingCommands > 0
            ? t("diag_queued")
            : view?.active
              ? t(view.busy ? "diag_busy" : "diag_ready")
              : t("diag_inactive")}
      </p>
      {!!view?.discardedBytes && (
        <p className="small warning">
          {t("diag_truncated", { count: view.discardedBytes })}
        </p>
      )}
      <pre
        className="diagnostic-output"
        id="diagnostic-output"
        tabIndex="0"
        aria-label={t("diag_output")}
      >
        {view?.log || t("diag_empty")}
      </pre>
      <details className="diag-reference">
        <summary>{t("diag_commands")}</summary>
        <p className="small">{t("diag_reference")}</p>
        <code>
          id 7 · baud 19200 · parity E · pins 18 17 · timeout 1200 · interval
          3000
          <br />
          probe 0080 1 · reg 0082 2 · range 1 16 · scanmode current
          <br />
          bauds 9600 19200 · parities NE · scan · x<br />
          plan clear · plan add 0080 1 · plan show
          <br />
          energy 0082 · ratios 002C · scale 0.001 · order swap · decode off
          <br />
          listen 5 · read · poll · stats clear · status · help · resume
        </code>
      </details>
      <p className="small muted">{t("diag_limits")}</p>
    </>
  );
}
export default function DiagnosticConsole({ device }) {
  const { t } = useText();
  const [open, setOpen] = useState(false);
  return (
    <details
      id="diagnostic-console"
      className="panel raw-trace"
      onToggle={(event) => setOpen(event.currentTarget.open)}
    >
      <summary>{t("diag_title")}</summary>
      {open ? <ConsoleData device={device} /> : null}
    </details>
  );
}
