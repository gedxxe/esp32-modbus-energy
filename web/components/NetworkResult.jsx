import { useText } from "../ui.js";

export default function NetworkResult({ network, fresh, scanBusy }) {
  const { t } = useText();
  if (!network) return null;
  const attempt = network.attempt;
  const stage = network.pending ? "applying" : scanBusy ? "scanning" : network.activeMode === "ap" ? "ap" : attempt?.stage || network.phase;
  const error = !network.pending && !scanBusy && network.error;
  return <div id="network-result" className="hint network-result" role="status" aria-live="polite" aria-atomic="true">
    <strong>{t("network_connection_result", { ssid: network.ssid || network.apSSID })}</strong>
    <p id="network-result-stage">{!fresh ? t("network_attempt_unreachable") : t(`network_attempt_${stage}`)}</p>
    {attempt?.id > 0 && <p className="small mono">{t("network_attempt_number", { id: attempt.id, seconds: Math.floor(attempt.elapsedMs / 1000) })}</p>}
    {error && <p id="network-result-error">{t(`network_error_${error}`)}<span className="small mono"> · reason {network.disconnectReason} / API {network.apiCode}{attempt?.apiStep ? ` / ${attempt.apiStep}` : ""}</span></p>}
    {attempt?.lastFailedId > 0 && <details id="network-last-failure"><summary>{t("network_last_failure", { id: attempt.lastFailedId })}</summary>
      <p>{t(`network_error_${attempt.lastError}`)}</p><p className="small mono">reason {attempt.lastReason} / API {attempt.lastApiCode}</p>
    </details>}
  </div>;
}
