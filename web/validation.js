// Reject incomplete API payloads before they reach render paths. A failed refresh
// retains the last accepted snapshot and is reported by the caller as stale.
const object = (value) => value !== null && typeof value === "object";
const uint = (value, max = 0xffffffff) =>
  Number.isInteger(value) && value >= 0 && value <= max;
const serial = (value) =>
  object(value) &&
  uint(value.slave, 247) && value.slave > 0 &&
  uint(value.baud, 1000000) && value.baud > 0 &&
  ["N", "E", "O"].includes(value.parity);
const hex = (value) =>
  typeof value === "string" && /^(?:[0-9A-Fa-f]{2}(?: [0-9A-Fa-f]{2})*)?$/.test(value);
const result = (value) =>
  ["valid", "echo", "timeout", "short", "overflow", "crc", "exception", "invalid"].includes(value);
const frame = (value) =>
  serial(value) && uint(value.address, 65535) && uint(value.words, 125);

export function validStatus(value) {
  return (
    serial(value) && ["manual", "auto"].includes(value.direction) &&
    typeof value.token === "string" && value.token.length > 0 &&
    Array.isArray(value.slots) && value.slots.length === 9 &&
    value.slots.every((slot) => object(slot) && uint(slot.start, 1439) && typeof slot.quality_code === "string" &&
      (slot.kwh === null || (Number.isFinite(slot.kwh) && slot.kwh >= 0))) &&
    (!value.error_code || typeof value.error_code === "string") &&
    object(value.scan) &&
    ["idle", "running", "found", "cancelled", "exhausted"].includes(value.scan.phase) &&
    (value.scan.phase !== "found" || serial(value.scan.result)) &&
    (!value.led || (object(value.led) && [value.led.red, value.led.green, value.led.blue, value.led.brightness].every((v) => uint(v, 255))))
  );
}

export function validFlow(value) {
  return object(value) && uint(value.boot) && uint(value.nowUs) &&
    Array.isArray(value.events) && value.events.length <= 32 &&
    value.events.every((event) => frame(event) && uint(event.seq) &&
      uint(event.atUs) && uint(event.kind, 9) && event.kind > 0 &&
      result(event.result) && hex(event.bytes));
}

export function validTrace(value) {
  return object(value) && typeof value.active === "boolean" &&
    Array.isArray(value.frames) && value.frames.length <= 12 &&
    value.frames.every((row) => frame(row) && uint(row.atMs) &&
      result(row.result) && hex(row.tx) && hex(row.rx));
}

export function validHistory(value) {
  return Array.isArray(value) && value.length <= 31 && value.every((row) =>
    object(row) && uint(row.day) && row.day > 0 &&
    typeof row.date === "string" && /^\d{4}-\d{2}-\d{2}$/.test(row.date));
}

export function validSystem(value) {
  return object(value) && Array.isArray(value.cores) && value.cores.length === 2 &&
    value.cores.every((core, index) => object(core) && core.id === index &&
      (core.load === null || (Number.isFinite(core.load) && core.load >= 0 && core.load <= 100))) &&
    Array.isArray(value.tasks) && value.tasks.length <= 64 &&
    value.tasks.every((task) => object(task) && typeof task.name === "string" &&
      uint(task.core, 1) && uint(task.priority, 255) && uint(task.stackFreeMin) &&
      ["running", "ready", "blocked", "suspended", "deleted", "invalid"].includes(task.state));
}

const ipv4 = (value) => typeof value === "string" && (value === "" ||
  (/^\d{1,3}(?:\.\d{1,3}){3}$/.test(value) && value.split(".").every((part) => Number(part) <= 255)));
export function validNetworkView(value) {
  return object(value) && ["ap", "client", "dual"].includes(value.mode) &&
    ["ap", "client", "dual"].includes(value.activeMode) &&
    ["applying", "ap", "online", "fallback", "connecting"].includes(value.phase) &&
    [value.pending, value.connected, value.apActive, value.hasPassword, value.stored].every((v) => typeof v === "boolean") &&
    validAPIP(value.configuredAPIP) && typeof value.ssid === "string" && value.ssid.length <= 32 &&
    typeof value.apSSID === "string" && value.apSSID.length <= 32 &&
    [value.clientIP, value.apIP, value.address].every(ipv4) && Number.isFinite(value.rssi);
}
export function validDiagnostic(value) {
  return object(value) && typeof value.log === "string" && value.log.length <= 8192 &&
    typeof value.active === "boolean" && typeof value.busy === "boolean" &&
    uint(value.generation) && uint(value.discardedBytes) && uint(value.pendingCommands, 32) &&
    object(value.evidence) && Object.values(value.evidence).every((v) => typeof v === "boolean" || uint(v)) &&
    serial(value.settings) && ["manual", "auto"].includes(value.settings.direction) &&
    ["rx", "tx", "dir", "timeout", "gap", "settle", "probe", "words", "status"].every((key) => uint(value.settings[key], 65535));
}

export function validAPIP(text) {
  if(!ipv4(text)||!text)return false;const [a,b,,d]=text.split(".").map(Number);
  return d>0&&d<255&&(a===10||(a===172&&b>=16&&b<=31)||(a===192&&b===168));
}
export function validWifiScan(value) {
  return object(value)&&["idle","scanning","ready","failed"].includes(value.phase)&&uint(value.generation)&&uint(value.ageMs)&&Array.isArray(value.networks)&&value.networks.length<=16&&value.networks.every((row)=>object(row)&&typeof row.ssid==="string"&&row.ssid.length>0&&row.ssid.length<=32&&Number.isInteger(row.rssi)&&typeof row.open==="boolean"&&typeof row.supported==="boolean"&&uint(row.channel,14));
}
