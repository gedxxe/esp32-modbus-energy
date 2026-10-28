// UART observations are grouped without inventing a response or joining across gaps.
export const FLOW_MOVE_MS = 1100;
export const FLOW_GAP_MS = 140;
export const FLOW_HOLD_MS = 160;
const sameLink = (a,b) => ["slave","baud","parity","address","words","rxPin","txPin","dirPin","diagnostic"].every(key => a[key] === b[key]);
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
