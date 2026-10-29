import { meterProfile, meterProfiles, addressHex, probeRegister, energyRegister } from "../meter-profile.js";
import { deviceFetch } from "../request.js";
import { useEffect, useState } from "react";
import { useText } from "../ui.js";
import { validDiagnostic } from "../validation.js";
const recipes = [
  ["diagnose", "diagnose"],
  ["basic", "basic"],
  ["probe", "intermediate"],
  ["map", "deeper"],
  ["rx", "rxcheck"],
  ["report", "report"],
  ["primary", "primary"],
  ["status", "status"],
  ["connected", "check connected"],
  ["isolated", "check isolated"],
  ["compare", "compare"],
];
function ConsoleData({ device }) {
  const { t } = useText();

  const [view, setView] = useState(null),
    [command, setCommand] = useState("diagnose"),
    [failed, setFailed] = useState(false);
  const [address, setAddress] = useState("0058"), [words, setWords] = useState(2);
  const [setting, setSetting] = useState("timeout"), [value, setValue] = useState("2000");
  const profile=meterProfiles(device.data).find(p=>p.key===view?.settings?.profile)||meterProfile(device.data);
  useEffect(()=>{if(!profile.ratioWords&&setting==="ratios"){setSetting("energy");setValue(addressHex(energyRegister(profile).address));}},[profile.key,setting]);
  useEffect(()=>{setAddress(addressHex(probeRegister(profile).address));setWords(probeRegister(profile).words);},[profile.key]);
  useEffect(() => {
    let alive = true,
      timer,
      controller;
    async function poll() {
      controller = new AbortController();

      try {
        const response = await deviceFetch("/api/diagnostic", {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error();
        const body = await response.json();
        if (!validDiagnostic(body))
          throw new Error();
        if (alive) {
          setView(body);
          setFailed(false);
        }
      } catch {
        if (alive) setFailed(true);
      } finally {

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
    device.data?.discoveryRunning || device.data?.scan?.phase === "running" ||
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
          `Energy station firmware ${view.version} | session ${view.generation}\nOld log bytes discarded: ${view.discardedBytes}\n\n${view.log}`,
        ],
        { type: "text/plain;charset=utf-8" },
      ),
    );
    const link = document.createElement("a");
    link.href = url;
    link.download = `meter-diagnostic-${view.generation}.txt`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  return (
    <>
      <p>{t("diag_intro")}</p>
      <div className="hint">{t("diag_pause")}</div>
      <div className="diag-recipes">
        {recipes.filter(([key])=>key!=="primary"||profile.primaryFloat).map(([label, line]) => (
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
      {view?.settings && <div className="diagnostic-summary">
        <h3>{t("diag_values")}</h3><p className="small muted">{t("diag_values_intro")}</p>
        <div className="pins" id="diagnostic-settings">
          <code>ID {view.settings.slave} / {view.settings.baud} / 8{view.settings.parity}{view.settings.stopBits||1}</code>
          <code>RX{view.settings.rx} / TX{view.settings.tx} / DIR{view.settings.dir} / {view.settings.direction}</code>
          <code>timeout {view.settings.timeout} ms / gap {view.settings.gap} ms / settle {view.settings.settle} µs</code>
          <code>Modbus 0x{view.settings.status.toString(16).padStart(2, "0").toUpperCase()}</code>
        </div>
      </div>}
      {view?.evidence && <details className="diag-reference" id="diagnostic-evidence">
        <summary>{t("diag_counts")}</summary><dl className="diagnostic-counts">
          {["requests", "rxBytes", "data", "exceptions", "crcBad", "shortFrames", "timeouts", "echoes", "wrongFrames", "localErrors"].map((key) =>
            <div key={key}><dt>{t(`diag_count_${key}`)}</dt><dd>{view.evidence[key] ?? 0}</dd></div>)}
        </dl>
      </details>}
      <details className="diag-reference" id="diagnostic-tools">
        <summary>{t("diag_read_title")} / {t("diag_setting_title")}</summary>
        <form id="diagnostic-register-form" onSubmit={(event) => {
          event.preventDefault();
          const start = /^[0-9a-fA-F]{1,4}$/.test(address) ? parseInt(address, 16) : NaN;
          if (!Number.isInteger(start) || start + Number(words) > 65536) { device.setNotice({key:"invalid_diagnostic_command"}); return; }
          const line = `reg ${address} ${words}`;setCommand(line);send(line);
        }}>
          <div className="form-grid"><div><label htmlFor="diagnostic-address">{t("diag_address")}</label><input id="diagnostic-address" value={address} pattern="[0-9a-fA-F]{1,4}" maxLength={4} required disabled={blocked} onChange={(event) => setAddress(event.target.value)} /></div>
          <div><label htmlFor="diagnostic-words">{t("diag_words")}</label><select id="diagnostic-words" value={words} disabled={blocked} onChange={(event) => setWords(Number(event.target.value))}>{[1,2,3].map((n) => <option key={n}>{n}</option>)}</select></div></div>
          <button id="diagnostic-read" type="submit" className="button" disabled={blocked}>{t("diag_read_action")}</button>
        </form>
        <form id="diagnostic-setting-form" onSubmit={(event) => { event.preventDefault();const line=`${setting} ${value.trim()}`;setCommand(line);send(line); }}>
          <div className="form-grid"><div><label htmlFor="diagnostic-setting">{t("diag_setting")}</label><select id="diagnostic-setting" value={setting} disabled={blocked} onChange={(event) => {setSetting(event.target.value);setValue(({profile:profile.key,id:"1",baud:"9600",parity:"N",stops:"1",direction:"manual",timeout:"2000",gap:"200",settle:"100",interval:"2000",probe:`${addressHex(probeRegister(profile).address)} ${probeRegister(profile).words}`,energy:addressHex(energyRegister(profile).address),ratios:addressHex(profile.ratios),scale:String(profile.energyScale),order:"normal",decode:"on",pins:"18 17",range:"1 16",scanmode:"current",bauds:"9600 19200",parities:"NE"})[event.target.value]);}}>
            {["profile","id","baud","parity","stops","direction","timeout","gap","settle","interval","probe","energy","ratios","scale","order","decode","pins","range","scanmode","bauds","parities"].filter(key=>key!=="ratios"||profile.ratioWords>0).map((key) => <option key={key}>{key}</option>)}
          </select></div><div><label htmlFor="diagnostic-setting-value">{t("diag_setting_value")}</label><input id="diagnostic-setting-value" value={value} required maxLength={60} pattern="[ -~]+" disabled={blocked} autoCapitalize="none" spellCheck={false} onChange={(event) => setValue(event.target.value)} /></div></div>
          <p className="small muted">{t("diag_setting_note")}</p><button id="diagnostic-setting-apply" type="submit" className="button" disabled={blocked}>{t("diag_setting_action")}</button>
        </form>
      </details>
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
          id 7 · baud 19200 · parity E · pins 18 17 · timeout 2000 · interval
          3000
          <br />
          direction manual · settle 100 · gap 200
          <br />probe {addressHex(probeRegister(profile).address)} {probeRegister(profile).words} · reg {addressHex(energyRegister(profile).address)} 2 · range 1 16 · scanmode current
          <br />
          bauds 9600 19200 · parities NE · scan · x<br />
          plan clear · plan add {addressHex(profile.frequency.address)} {profile.frequency.words} · plan show
          <br />
          energy {addressHex(energyRegister(profile).address)} · {profile.ratioWords>0?`ratios ${addressHex(profile.ratios)} · `:""}scale {profile.energyScale} · order swap · decode off
          <br />
          {meterProfiles(device.data).map(p=>`profile ${p.key}`).join(" · ")} · primary · listen 5 · read · poll · stats clear · status · help · resume
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
