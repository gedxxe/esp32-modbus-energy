import { useEffect, useRef, useState } from "react";
import { useText } from "../ui.js";
import { deviceFetch } from "../request.js";
import { validWifiScan } from "../validation.js";

export default function WifiPicker({ device, ssid, open, onChoose, onBusy, disabled }) {
  const { t } = useText();
  const [rows, setRows] = useState([]), [phase, setPhase] = useState("idle");
  const [reason, setReason] = useState(""), [detail, setDetail] = useState(null);
  const [refresh, setRefresh] = useState(0);
  const busyCallback = useRef(onBusy);
  busyCallback.current = onBusy;
  useEffect(() => {
    if (!device.fresh || !device.data?.token) return;
    let alive = true, timer, retry = 0, controller = new AbortController();
    const busy = (value) => busyCallback.current(value);
    async function request(start = false) {
      try {
        if (start) {
          const reply = await deviceFetch("/api/network/scan", {
            method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: new URLSearchParams({ token: device.data.token }), signal: controller.signal,
          });
          if (!reply.ok && reply.status !== 409) throw new Error("network");
        }
        const response = await deviceFetch("/api/network/scan", { cache: "no-store", signal: controller.signal });
        if (!response.ok) throw new Error("network");
        const data = await response.json();
        if (!validWifiScan(data)) throw new Error("schema");
        if (!alive) return;
        retry = 0;
        if (!start && (data.phase === "idle" || (data.phase === "ready" && data.ageMs > 30000))) {
          await request(true); return;
        }
        const unique = new Map();
        for (const row of data.networks) {
          const key = `${row.ssid}/${row.open}`, old = unique.get(key);
          if (!old || (row.supported && !old.supported) || (row.supported === old.supported && row.rssi > old.rssi)) unique.set(key, row);
        }
        setRows([...unique.values()].sort((a,b) => b.rssi-a.rssi));
        setPhase(data.phase); setReason(data.error || "radio_failed"); setDetail(data);
        busy(data.phase === "scanning");
        if (data.phase === "scanning") timer = setTimeout(() => request(), 1000);
      } catch (error) {
        if (!alive) return;
        // A dropped acknowledgement never triggers another POST. Re-read the
        // current generation because the radio may already have completed it.
        setPhase("reconnecting"); setReason(error.message === "schema" ? "schema" : "http");
        busy(false);
        if (++retry <= 3) timer = setTimeout(() => request(), 1200 * retry);
        else setPhase("unavailable");
      }
    }
    setPhase("scanning"); busy(true); request(refresh > 0);
    return () => { alive = false; clearTimeout(timer); controller.abort(); busy(false); };
  }, [device.fresh, device.data?.token, refresh]);
  const selected = rows.find((row) => row.ssid === ssid && row.open === open);
  const value = selected ? `${selected.ssid}/${selected.open}` : ssid ? "saved" : "";
  const message = phase === "failed" ? "wifi_error_radio" : phase === "reconnecting" ? "wifi_reconnecting" : phase === "unavailable" ? `wifi_error_${reason}` : phase === "scanning" ? "wifi_scanning" : rows.length ? "wifi_scan_ready" : "wifi_scan_empty";
  return <div className="wifi-picker">
    <div className="wifi-scan-heading"><label htmlFor="network-ssid">{t("network_ssid")}</label><button type="button" id="wifi-rescan" className="button compact" disabled={disabled || phase === "scanning"} onClick={() => setRefresh((value) => value + 1)}>{t("wifi_refresh")}</button></div>
    <select id="network-ssid" value={value} disabled={disabled} onChange={(event) => { const row = rows.find((item) => `${item.ssid}/${item.open}` === event.target.value); if (row) onChoose(row); }} required>
      <option value="">{t("wifi_choose")}</option>
      {ssid && !selected && <option value="saved">{ssid} · {t("wifi_saved")}</option>}
      {rows.map((row) => <option key={`${row.ssid}/${row.open}`} value={`${row.ssid}/${row.open}`} disabled={!row.supported}>{row.ssid} · {row.rssi} dBm · {t(!row.supported ? "wifi_enterprise" : row.open ? "wifi_open" : "wifi_secured")}</option>)}
    </select>
    <p className="small muted" id="wifi-scan-status" role="status">{t(message, { count: rows.length })}</p>
    {detail && <p className="small muted mono" id="wifi-scan-detail">{t("wifi_scan_detail", { generation: detail.generation, attempts: detail.attempts ?? 0, code: detail.driverCode ?? 0, reason: detail.error || detail.phase })}</p>}
  </div>;
}
