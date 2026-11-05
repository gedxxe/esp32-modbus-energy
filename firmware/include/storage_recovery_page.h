// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Flash-resident recovery page; no OLED, LittleFS or generated web bundle.
#include <Arduino.h>
static const char STORAGE_RECOVERY_HTML[] PROGMEM = R"RECOVERY(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex,nofollow"><title>LittleFS recovery | Midea Engineering</title>
<style>
:root{color-scheme:dark;font:16px system-ui,sans-serif;background:#101f24;color:#ebf4f4}
*{box-sizing:border-box}main{max-width:740px;margin:24px auto;padding:0 18px}
h1{font-size:1.7rem;margin:0 0 8px}h2{font-size:1.1rem;margin:0 0 12px}
p,li{line-height:1.55}.muted{color:#b4c9cb}section{border:1px solid #31545b;border-radius:14px;padding:18px;margin:18px 0;background:#152c32}
dl{display:grid;grid-template-columns:minmax(145px,1fr) 1fr;gap:8px}
dt{color:#b4c9cb}dd{margin:0;overflow-wrap:anywhere;font-family:ui-monospace,monospace}
button,a.link{font:inherit;padding:11px 15px;border-radius:9px;border:1px solid #6fabad;background:#174a50;color:#fff;cursor:pointer}
button:disabled{opacity:.45;cursor:not-allowed}a{color:#a6e1e5}
.warning{border-left:4px solid #e1ad67;padding:9px 12px;background:#3c3026}
#message{min-height:3em;white-space:pre-wrap;overflow-wrap:anywhere}
</style></head><body><main>
<p class="muted">MIDEA ENGINEERING / ESP32-S3</p>
<h1>LittleFS recovery — no OLED needed</h1>
<p>Wi-Fi connected does not mean cloud connected. The IoT uploader cannot start while LittleFS is unavailable. This page lives in the firmware, not in LittleFS.</p>
<section><h2>Storage diagnostics</h2><dl id="details"><dt>Status</dt><dd>Reading device…</dd></dl>
<p id="message" role="status" aria-live="polite"></p>
<button id="retry" disabled>Retry mount — do not format</button>
<button id="refresh">Refresh</button></section>
<section><h2>Safe retry</h2>
<p>The read-only mount probe runs on the acquisition owner. It does not format, erase, write an archive or change Modbus, RTC or LoRa settings. If it succeeds, <strong>reboot normally</strong> to reconcile NVS/archives and enable the IoT uploader.</p>
<p class="warning">A failed mount can mean unformatted storage, corruption or a partition mismatch. It is not proof that archives can be discarded.</p></section>
<section><h2>Physical erase — last resort only</h2>
<p>First export and back up every recoverable record. Then, only if you explicitly accept losing the remaining LittleFS data: power off, hold the physical OK button on <strong>GPIO6</strong> low, power on and keep holding for <strong>5 seconds</strong>. Release early to cancel. USB Serial Monitor at <strong>115200 baud</strong> reports progress without an OLED.</p>
<p class="warning">Formatting deletes LittleFS archives and filesystem credentials, but not NVS. This firmware intentionally has no network format endpoint.</p>
<p><a class="link" href="/dashboard">Normal dashboard</a> · <a href="/api/status">Raw API status</a></p>
</section></main><script>
let current=null,busy=false;
const details=document.getElementById('details'),message=document.getElementById('message'),retry=document.getElementById('retry');
function fill(rows){details.replaceChildren();for(const [k,v] of rows){const a=document.createElement('dt'),b=document.createElement('dd');a.textContent=k;b.textContent=v===undefined||v===null?'unavailable':String(v);details.append(a,b)}}
async function refresh(){
 try{
  const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);
  current=await r.json();
  const fs=current.storageRecovery||{},iot=current.iot||{},wifi=current.network||{};
  fill([['Filesystem mounted',fs.mounted],['Partition found',fs.partitionFound],['Partition bytes',fs.partitionBytes],
    ['Boot action',fs.bootAction],['Retry attempts',fs.retryAttempts],['Retry result',fs.retryResult],
    ['OLED present',fs.oledAvailable],['Wi-Fi mode',wifi.activeMode],['Wi-Fi connected',wifi.connected],
    ['IoT provisioned',iot.configured],['IoT worker enabled',iot.enabled],['IoT error',iot.error]]);
  retry.disabled=busy||fs.mounted!==false||!fs.partitionFound||fs.retrySucceeded===true;
  if(fs.retrySucceeded)message.textContent='Safe mount probe succeeded. Reboot normally. Do not format.';
  else if(fs.mounted)message.textContent='LittleFS is mounted; recovery not needed.';
  else if(!fs.partitionFound)message.textContent='Storage partition missing. Check the flashed partition table; do not format.';
  else message.textContent='Mount unavailable. Safe retry will not erase files.';
 }catch(e){retry.disabled=true;message.textContent='Unable to read local status: '+e.message}
}
document.getElementById('refresh').addEventListener('click',refresh);
retry.addEventListener('click',async()=>{
 if(busy||!current?.token)return;
 busy=true;retry.disabled=true;message.textContent='Queuing read-only mount probe…';
 try{
  const r=await fetch('/api/storage/retry',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({token:current.token})});
  const payload=await r.json();if(!r.ok)throw Error(payload.message||payload.code||'Request rejected');
  let confirmed=false;for(let i=0;i<24;i++){
   await new Promise(resolve=>setTimeout(resolve,500));
   const status=await (await fetch('/api/status',{cache:'no-store'})).json();current=status;
   if(status.control?.completed>=payload.commandId){
    const result=status.control.results?.find(v=>v.id===payload.commandId);
    if(result){confirmed=true;await refresh();if(result.error)message.textContent='Probe result: '+result.error;break;}
   }
  }
  if(!confirmed)message.textContent='Completion not yet confirmed. Refresh the page; never format automatically.';
 }catch(e){message.textContent='Retry failed: '+e.message}
 finally{busy=false;retry.disabled=true;}
});
refresh();
</script></body></html>)RECOVERY";
