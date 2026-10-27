// Exercise the actual browser scheduler, including complete response bodies.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
module.exports = async function testRequest() {
  const source = fs.readFileSync(path.join(__dirname,"../web/request.js"),"utf8");
  const { deviceFetch } = await import("data:text/javascript;base64,"+Buffer.from(source).toString("base64"));
  const original = global.fetch, calls = [], gates = new Map();
  const tick = () => new Promise(setImmediate);
  global.fetch = async (url,{signal}) => {
    calls.push(url);
    return new Response(new ReadableStream({start(controller) {
      const cancel=()=>controller.error(new DOMException("Aborted","AbortError"));
      signal.addEventListener("abort",cancel,{once:true});
      gates.set(url,()=>{signal.removeEventListener("abort",cancel);controller.enqueue(new TextEncoder().encode("{}"));controller.close();});
    }}),{headers:{"Content-Type":"application/json"}});
  };
  try {
    const one=deviceFetch("/one"),two=deviceFetch("/two");
    await tick();assert.deepEqual(calls,["/one"],"Response headers do not release the socket slot");
    gates.get("/one")();await one;await tick();assert.deepEqual(calls,["/one","/two"]);
    gates.get("/two")();await two;
    const controller=new AbortController(),hold=deviceFetch("/hold");
    const queued=deviceFetch("/cancelled",{signal:controller.signal});
    const cancellation=assert.rejects(queued,{name:"AbortError"});controller.abort();await cancellation;
    gates.get("/hold")();await hold;await tick();assert(!calls.includes("/cancelled"));
    const telemetry=deviceFetch("/telemetry"),aborted=assert.rejects(telemetry,{name:"AbortError"});
    await tick();const stop=deviceFetch("/stop",{method:"POST",priority:true});
    await aborted;await tick();assert.equal(calls.at(-1),"/stop");gates.get("/stop")();await stop;
    const post=deviceFetch("/accepted",{method:"POST"});await tick();
    const resume=deviceFetch("/resume",{method:"POST",priority:true});await tick();
    assert.equal(calls.at(-1),"/accepted","A priority request must not abort an accepted POST");
    gates.get("/accepted")();await post;await tick();gates.get("/resume")();await resume;
    console.log("PASS: actual API scheduler serializes complete bodies, cancels queued reads, preempts telemetry for Stop and preserves accepted POSTs");
  } finally { global.fetch=original; }
};
if(require.main===module)module.exports().catch(error=>{console.error(error);process.exitCode=1;});
