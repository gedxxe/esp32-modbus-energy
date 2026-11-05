import { useText } from "../ui.js";
import Icon from "./Icon.jsx";

export default function NetworkResult({ network, fresh, scanBusy }) {
  const { t } = useText();
  if (!network) return null;
  const attempt = network.attempt;
  const stage = network.pending ? "applying" : scanBusy ? "scanning" : network.activeMode === "ap" ? (network.apActive ? "ap" : "ap_unavailable") : attempt?.stage || network.phase;
  const error = !network.pending && !scanBusy && network.error;
  return <div id="network-result" className={`hint network-result ${error?"status-error":""}`} role="status" aria-live="polite" aria-atomic="true">
    <strong className="panel-title"><Icon name={error?"warning":"wifi"} size={20}/>{t("network_connection_result", { ssid: network.ssid || network.apSSID })}</strong>
    <p id="network-result-stage">{!fresh ? t("network_attempt_unreachable") : t(`network_attempt_${stage}`)}</p>
    {attempt?.id > 0 && <p className="small mono">{t("network_attempt_number", { id: attempt.id, seconds: Math.floor(attempt.elapsedMs / 1000) })}</p>}
    {error && <p id="network-error"><span id="network-result-error">{t(`network_error_${error}`)}</span>{(network.disconnectReason||network.apiCode||attempt?.apiStep)?<code className="network-error-code">reason {network.disconnectReason} / API {network.apiCode}{attempt?.apiStep ? ` / ${attempt.apiStep}` : ""}</code>:null}</p>}
    {attempt?.lastFailedId > 0 && <details id="network-last-failure"><summary>{t("network_last_failure", { id: attempt.lastFailedId })}</summary>
      <p>{t(`network_error_${attempt.lastError}`)}</p><p className="small mono">reason {attempt.lastReason} / API {attempt.lastApiCode}</p>
    </details>}
  </div>;
}
