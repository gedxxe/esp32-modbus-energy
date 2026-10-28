// UART observations are grouped without inventing a response or joining across gaps.
export const FLOW_MOVE_MS = 1100;
export const FLOW_SETTLE_MS = 360;
export const FLOW_GAP_MS = 140;
export const FLOW_HOLD_MS = 160;
// Quintic easing has zero velocity and acceleration at both endpoints.
export function easeFlow(value) {
  const t=Math.min(1,Math.max(0,Number.isFinite(value)?value:0));
  return t*t*t*(t*(t*6-15)+10);
}
export function flowPhaseDuration(phase) {
  if(phase==="tx"||phase==="rx")return FLOW_MOVE_MS;
  if(phase==="tx-settle"||phase==="rx-settle")return FLOW_SETTLE_MS;
  return phase==="gap"?FLOW_GAP_MS:FLOW_HOLD_MS;
}
export function nextFlowPhase(phase,hasRX) {
  if(phase==="tx")return "tx-settle";
  if(phase==="tx-settle")return "gap";
  if(phase==="gap")return hasRX?"rx":"hold";
  if(phase==="rx")return "rx-settle";
  if(phase==="rx-settle")return "hold";
  return null;
}
export function flowLeg(phase,progress,direction) {
  if(phase===direction)return { visible:true,position:easeFlow(progress),opacity:easeFlow(progress/0.16) };
  if(phase===`${direction}-settle`)return { visible:true,position:1,opacity:1-easeFlow(progress) };
  const arrived=direction==="tx"?["gap","rx","rx-settle","hold","idle"].includes(phase):["hold","idle"].includes(phase);
  return { visible:false,position:arrived?1:0,opacity:0 };
}
const sameLink = (a,b) => ["profile","slave","baud","parity","address","words","rxPin","txPin","dirPin","diagnostic"].every(key => a[key] === b[key]);
export class FrameAssembler {
  reset() { this.draft = null; this.last = null; }
  accept(event) {
    if (this.last != null && ((event.seq-this.last)>>>0) !== 1) this.draft = null;
    this.last = event.seq;
    if (event.kind === 1) { this.draft = { request:event }; return null; }
    const draft = this.draft;
    if (!draft || !sameLink(draft.request,event)) { this.draft = null; return null; }
    if (event.kind === 2) draft.tx = event;
    if (event.kind === 3) draft.firstRX = event;
    if (![4,5,6,8].includes(event.kind)) return null;
    this.draft = null;
    if (!draft.tx) return null;
    return { ...draft, key:event.seq, outcome:event,
      rx:[4,5].includes(event.kind) && event.bytes ? event : null };
  }
}
