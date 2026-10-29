import { addressHex, directEnergy } from "../meter-profile.js";
import { useText } from "../ui.js";

export default function MeterMeasurements({profile,status,live}) {
  const {t,number}=useText();
  const fields=[["current_a","currentA","A"],["voltage_a","voltageA","V"],["power_factor","powerFactor","cos φ"],["frequency","frequency","Hz"],[directEnergy(profile)?"meter_active_energy":"secondary_energy","secondaryEnergy","kWh"],["reactive_energy","reactiveEnergy","kvarh"]];
  return <section className="panel register-summary" aria-labelledby="register-title">
    <div><h2 id="register-title">{t("register_title")}</h2><p>{t(directEnergy(profile)?"register_direct_intro":"register_intro")}</p></div>
    <dl className="register-values">{fields.filter(([,key])=>profile[key]).map(([label,key,unit])=><div key={key}>
      <dt>{t(label)} <code>0x{addressHex(profile[key].address)}</code></dt>
      <dd id={`value-${key}`}>{live?number(key==="secondaryEnergy"?(status?.meterEnergy??status?.secondaryEnergy):status?.[key],key==="frequency"?2:3):"—"} <small>{unit}</small>
      {["secondaryEnergy","reactiveEnergy","powerFactor"].includes(key)&&status?.secondaryAge!=null&&<div className="small muted">{t("sample",{seconds:status.secondaryAge})}</div>}</dd>
    </div>)}</dl>
    {profile.coherentSnapshot&&status?.readingQuality?.required>0&&!status.readingQuality.complete&&<p id="snapshot-quality" className="hint" role="status">{t("snapshot_rejected")}<code> RX {status.readingQuality.transportFailed} / value {status.readingQuality.invalid}</code></p>}
    <p className="small muted">{t(profile.coherentSnapshot?"register_coherent_note":"register_note")}</p>
  </section>;
}
