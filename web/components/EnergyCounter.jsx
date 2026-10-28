import { memo } from "react";
import Counter from "../vendor/react-bits/Counter.jsx";
import { isNumber, localeFor, useText } from "../ui.js";

const RollingCounter=memo(Counter);

export default function EnergyCounter({ value }) {
  const { lang,number }=useText();
  return <span id="energy" className="energy-counter" data-value={number(value)}>
    {isNumber(value) ? <RollingCounter value={value} locale={localeFor(lang)} decimals={3}
      fontSize="inherit" fontWeight="inherit" duration={0.65} bounce={0.05}
      stagger={0.025} motionBlur={false} intro={false} fade={0.12} /> : "—"}
  </span>;
}
