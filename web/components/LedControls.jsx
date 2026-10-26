import { useState } from "react";
import { useText } from "../ui.js";

const modes = ["off", "solid", "hue", "blink", "breathe"];
const hex = (led) =>
  "#" +
  [led.red, led.green, led.blue]
    .map((v) => v.toString(16).padStart(2, "0"))
    .join("");

function LedForm({ device, led }) {
  const { t, number } = useText();
  const [mode, setMode] = useState(led.mode);
  const [brightness, setBrightness] = useState(led.brightness);
  const [color, setColor] = useState(hex(led));
  const [seconds, setSeconds] = useState(led.period / 1000);
  const blocked = !led.available || !device.fresh || device.pending;
  const animated = ["hue", "blink", "breathe"].includes(mode);
  const hasColor = ["solid", "blink", "breathe"].includes(mode);
  return (
    <section className="panel led-panel" aria-labelledby="led-title">
      <div className="led-heading">
        <div>
          <h2 id="led-title">{t("led_title")}</h2>
          <p>{t("led_intro")}</p>
        </div>
        <span
          className="led-swatch"
          aria-hidden="true"
          style={{
            backgroundColor: led.mode === "off" ? "#87948e" : hex(led),
            opacity:
              led.mode === "off" ? 0.25 : Math.max(0.15, led.brightness / 255),
          }}
        />
      </div>
      <p className="led-applied" id="led-applied">
        {t("led_applied", { mode: t("led_" + led.mode), pin: led.pin })}
      </p>
      <form
        id="led-form"
        onSubmit={(event) => {
          event.preventDefault();
          device.act("/api/led", {
            mode,
            brightness,
            period: Math.round(Number(seconds) * 1000),
            red: parseInt(color.slice(1, 3), 16),
            green: parseInt(color.slice(3, 5), 16),
            blue: parseInt(color.slice(5, 7), 16),
          });
        }}
      >
        <div className="form-grid">
          <div>
            <label htmlFor="led-mode">{t("led_mode")}</label>
            <select
              id="led-mode"
              value={mode}
              disabled={blocked}
              onChange={(event) => setMode(event.target.value)}
            >
              {modes.map((value) => (
                <option key={value} value={value}>
                  {t("led_" + value)}
                </option>
              ))}
            </select>
          </div>
          <div>
            <label htmlFor="led-color">{t("led_color")}</label>
            <input
              id="led-color"
              type="color"
              value={color}
              disabled={blocked || !hasColor}
              onChange={(event) => setColor(event.target.value)}
            />
          </div>
        </div>
        <label htmlFor="led-brightness">
          {t("led_brightness")}{" "}
          <output htmlFor="led-brightness">
            {brightness}/255 · {number((brightness / 255) * 100, 0)}%
          </output>
        </label>
        <input
          id="led-brightness"
          type="range"
          min="0"
          max="255"
          step="1"
          value={brightness}
          disabled={blocked || mode === "off"}
          onChange={(event) => setBrightness(Number(event.target.value))}
        />
        <label htmlFor="led-period">{t("led_period")}</label>
        <input
          id="led-period"
          type="number"
          min="0.5"
          max="30"
          step="0.5"
          required
          value={seconds}
          disabled={blocked || !animated}
          onChange={(event) => setSeconds(event.target.value)}
        />
        <div className="actions">
          <button id="led-save" className="button primary" disabled={blocked}>
            {t("led_save")}
          </button>
        </div>
      </form>
      <div className="hint" role="status">
        {!led.available
          ? t("led_unavailable")
          : !led.storageOK
            ? t("led_storage_error")
            : led.stored
              ? t("led_stored")
              : t("led_defaults")}
      </div>
      <p className="led-note">{t("led_note")}</p>
    </section>
  );
}

export default function LedControls({ device }) {
  const led = device.data?.led || {
    available: false,
    mode: "off",
    brightness: 128,
    period: 8000,
    red: 0,
    green: 184,
    blue: 169,
    pin: 48,
  };
  // Reset the draft only when the applied configuration changes, never on each poll.
  const signature = [
    led.available,
    led.mode,
    led.brightness,
    led.period,
    led.red,
    led.green,
    led.blue,
  ].join(":");
  return <LedForm key={signature} device={device} led={led} />;
}
