// Exercise the compressed assets embedded in the firmware, using simulated APIs.
const fs = require("node:fs");
const path = require("node:path");
const http = require("node:http");
const zlib = require("node:zlib");
const crypto = require("node:crypto");
const assert = require("node:assert/strict");
const testRequest = require("./test-request.cjs");
const testFlow = require("./test-flow.cjs");
const { chromium } = require(process.argv[2] || "playwright");
const root = path.resolve(__dirname, "..");
const version = fs
  .readFileSync(path.join(root, "firmware/include/version.h"), "utf8")
  .match(/FIRMWARE_VERSION\[\] = "([^"]+)"/)[1];
// Keep simulated labels/screenshots aligned with the installed board configuration.
const rgbPin = Number(
  fs
    .readFileSync(path.join(root, "firmware/include/board.h"), "utf8")
    .match(/RGB_LED_PIN\s*=\s*(\d+)/)[1],
);
const manifest = JSON.parse(
  fs.readFileSync(path.join(root, "web/bundle.json")),
);
const header = fs.readFileSync(
  path.join(root, "firmware/include/web_ui.h"),
  "utf8",
);
const embedded = Object.fromEntries(
  [
    ...header.matchAll(
      /const uint8_t (WEB_[A-Z_]+)\[\] PROGMEM = \{([\s\S]*?)\};/g,
    ),
  ].map(([, name, bytes]) => [
    name,
    Buffer.from(
      [...bytes.matchAll(/0x([a-f0-9]{2})/g)].map((match) =>
        parseInt(match[1], 16),
      ),
    ),
  ]),
);
for (const asset of Object.values(manifest.assets)) {
  assert.equal(embedded[asset.symbol].length, asset.gzip);
  assert.equal(
    crypto.createHash("sha256").update(embedded[asset.symbol]).digest("hex"),
    asset.sha256,
  );
  assert.equal(zlib.gunzipSync(embedded[asset.symbol]).length, asset.raw);
}
const upstream = JSON.parse(
  fs.readFileSync(path.join(root, "web/vendor/react-bits/upstream.json")),
);
for (const [file, sha] of Object.entries(upstream.files)) {
  const source = Buffer.from(
    fs
      .readFileSync(path.join(root, "web/vendor/react-bits", file), "utf8")
      .replace(/\r\n/g, "\n"),
  );
  const hash = crypto
    .createHash("sha1")
    .update(Buffer.concat([Buffer.from(`blob ${source.length}\0`), source]))
    .digest("hex");
  assert.equal(hash, sha, "React Bits upstream file changed: " + file);
}
const starts = [360, 390, 420, 450, 480, 510, 990, 1020, 1050];
let online = true,
  fail = false,
  scanMode = "running",
  rejectScan = false,
  hangScan = false,
  malformedStatus = false,
  malformedFlow = false,
  malformedTrace = false,
  malformedHistory = false,
  malformedSystem = false,
  malformedNetwork = false, malformedDiagnostic = false, malformedDisplay=false, malformedCheckpoints=false, malformedCaptureArchive=false,
  diagnosticHold = false,
  delayedControls = 0,
  rejectNextControl = false;
let traceFail = false,
  traceRequests = 0;
const trace = {
  version,
  generation: 0,
  active: false,
  count: 0,
  capacity: 12,
  txPin: 17,
  rxPin: 18,
  frames: [],
};
const calls = [],
  assetRequests = new Set();
const meterCatalog=[
 {key:"fort",name:"Fort",terminals:"58 / A · 59 / B",maxBaud:38400,energyScale:.001,ratios:44,ratioWords:3,currentA:{address:88,words:2},voltageA:{address:66,words:2},power:{address:106,words:2},frequency:{address:128,words:1},secondaryEnergy:{address:130,words:2}},
 {key:"acrel-adl400",name:"Acrel ADL400",terminals:"21 / A1 · 22 / B1",maxBaud:19200,energyScale:.01,ratios:141,ratioWords:2,currentA:{address:100,words:1},voltageA:{address:97,words:1},power:{address:362,words:2},frequency:{address:119,words:1},secondaryEnergy:{address:10,words:2}}
];
const state = {
  version,profile:"fort",meter:meterCatalog[0],meterProfiles:meterCatalog,
  token: "test-session",
  display:{available:true,idleSeconds:60,stored:false,storageOK:true,sleeping:false},
  network: {configuredAPIP:"192.168.4.1",mode:"ap",activeMode:"ap",ssid:"",hasPassword:false,stored:true,pending:false,connected:false,rssi:0,clientIP:"",apIP:"192.168.4.1",apSSID:"Midea Engineering AP",apActive:true,address:"192.168.4.1",phase:"ap"},
  control: { accepted: 0, completed: 0, rejected: 0, lastError: "", pending: 0, results: [] },
  rtc: true,
  storage: true,
  ratios: true,
  muted: false,
  led: {
    available: true,
    pin: rgbPin,
    mode: "hue",
    brightness: 128,
    period: 8000,
    red: 0,
    green: 184,
    blue: 169,
    stored: false,
    storageOK: true,
  },
  energy: 128432.728,
  power: 48.62,
  age: 1,
  ratio: 80,
  slave: 1,
  baud: 9600,
  parity: "N",
  direction: "manual",
  currentA: 4.9943, voltageA: 230.12, frequency: 50.0, secondaryEnergy: 1605.4091,
  crcErrors: 0,
  timeouts: 2,
  rejected: 0,
  heap: 180000,
  uptime: 11300,
  error: "Reading valid",
  error_code: "acquisition_ok",
  clock: "08:42:16",clockEpoch:Math.floor(Date.parse("2026-10-26T01:42:16Z")/1000),clockSampleAgeMs:0,clockRevision:0,
  date: "2026-10-26",
  dayDate: "2026-10-26",
  slots: starts.map((start, i) => ({
    start,
    kwh: i < 5 ? [21.32, 24.85, 23.11, 25.68, 22.44][i] : i === 5 ? 9.12 : null,
    quality_code: i < 5 ? "complete_estimated" : i === 5 ? "active" : "waiting",
  })),
  scan: { phase: "idle", tested: 0, total: 0 },
};
const captureMinutes=[360,390,420,450,480,1020,1050,1080,1110,1140];
function checkpointFixture(day,complete=false){
  return {day,date:new Date(day*86400000).toISOString().slice(0,10),revision:7,windowSeconds:5,durable:true,archived:true,backlog:false,
    rows:captureMinutes.map((minute,i)=>{
      const recorded=complete||i<5&&i!==2,target=day*86400+minute*60-25200;
      return {minute,status:recorded?"recorded":i===2?"missed":"waiting",kwh:recorded?128400+i*8:null,startedUTC:recorded?target+(i===1?2:0):null,capturedUTC:recorded?target+(i===1?3:1):null,
        delaySeconds:recorded?(i===1?3:1):null,raw:recorded?(128400+i*8)*1000:null,slave:recorded?1:null,ptNum:recorded?1:null,ptDen:recorded?1:null,ct:recorded?1:null,
        difference:recorded&&i!==0&&i!==5&&(complete||i===1||i===4)?8:null};
    })};
}
state.checkpoints=checkpointFixture(Math.floor((state.clockEpoch+25200)/86400));
const historicalCheckpoints=checkpointFixture(state.checkpoints.day-1,true);

// Simulated instrumentation only; exported screenshots are representative.
let wifiScanGeneration=0,wifiScanRequests=0,wifiScanFinish=0,wifiDropAck=false,wifiReadFailures=0,wifiRadioFailure="",wifiReadDelay=0;
const nearbyNetworks=[{ssid:"Plant WiFi",rssi:-80,channel:1,open:false,supported:true},{ssid:"Plant WiFi",rssi:-48,channel:6,open:false,supported:true},{ssid:"Open test network",rssi:-62,channel:11,open:true,supported:true},{ssid:"Guest WiFi",rssi:-70,channel:1,open:false,supported:true}];
let flowBoot = 1;
let flowSequence = 0,
  flowEvents = [],
  flowAuto = true,
  flowGap = false,
  lastFlowFrame = 0,
  diagnosticRequests = 0,
  systemRequests = 0;
const diagnostic = {
  version,
  generation: 0,
  active: false,
  busy: false,
  discardedBytes: 0,
  pendingCommands:0,
  settings:{slave:1,baud:9600,parity:"N",rx:18,tx:17,dir:16,direction:"manual",timeout:2000,gap:200,settle:100,probe:88,words:2,status:0xE2},
  evidence: { requests: 0 },
  log: "",
};
function appendFlow(outcome="valid") {
  const now = Math.floor((Date.now() % 1000000) * 1000);
  for (const [kind, offset, result] of [
    [1, 80000, "invalid"],
    [2, 70000, "invalid"],
    ...(outcome==="timeout"?[]:[[3, 60000, "invalid"]]),
    [outcome==="timeout"?6:4, 50000, outcome],
  ])
    flowEvents.push({
      seq: ++flowSequence,
      atUs: (now - offset) >>> 0,
      kind,
      rxPin:18,txPin:17,dirPin:16,
      baud: 9600,
      parity: "N",
      slave: 1,
      address: 130,
      words: 2,
      diagnostic: diagnostic.active,
      overflow: false,
      result,
      bytes:
        kind===6?"":kind < 3 ? "01 03 00 82 00 02 64 23" : "01 03 04 00 00 03 E8 FA 84",
    });
  flowEvents = flowEvents.slice(-32);
}
function labEvidence(index=0){
 const profile=meterCatalog[index],addresses=index?[100,10,141]:[88,130,44],widths=index?[1,2,2]:[2,2,3];
 const probes=addresses.map((address,i)=>({address,words:widths[i],attempts:2,confirmed:true,data:true,tx:"01 03 00 58 00 02 45 D8",replies:[{status:0,accepted:true,truncated:false,attemptMs:221,rx:widths[i]===1?"01 03 02 13 88 B5 2F":widths[i]===2?"01 03 04 00 00 03 E8 FA 84":"01 03 06 00 01 00 01 00 50 00 00"},{status:0,accepted:true,truncated:false,attemptMs:221,rx:widths[i]===1?"01 03 02 13 88 B5 2F":widths[i]===2?"01 03 04 00 00 03 E8 FA 84":"01 03 06 00 01 00 01 00 50 00 00"}]}));
 for(const probe of probes)for(const reply of probe.replies){reply.tx=probe.tx;reply.txBytes=8;}
 return {slave:index+1,baud:9600,parity:"N",stopBits:1,direction:"manual",boot:99,run:1,utc:1793185200,elapsedMs:2000,requests:6,timeouts:0,crcErrors:0,rejected:0,exceptions:0,mapEvidence:index?2:1,mappingHints:[profile.key],confirmed:true,probes};
}
const defaultLabPlan=[{address:0,words:1},...meterCatalog.flatMap(p=>[p.currentA,p.secondaryEnergy,{address:p.ratios,words:p.ratioWords}])];
const lab={schema:1,version,ready:true,storageOK:true,activeId:0,error:"",discovery:{plan:defaultLabPlan,phase:"idle",run:0,tested:0,total:0,elapsedMs:0,requests:0,timeouts:0,crcErrors:0,rejected:0,exceptions:0},current:{...labEvidence(),run:0,confirmed:false,probes:[]},findings:[],saved:[]};
let labRequests=0,labHold=false,labMalformed=false,labNextId=1;
const server = http.createServer((req, res) => {
  const asset = manifest.assets[req.url];
  if (asset) {
    assetRequests.add(req.url);
    res.writeHead(200, {
      "Content-Type": asset.mime,
      "Content-Encoding": "gzip",
      "Cache-Control": "no-store",
    });
    res.end(embedded[asset.symbol]);
    return;
  }
  res.setHeader("Content-Type", "application/json");
  if(req.url==="/api/network/scan"){
    if(req.method==="POST"){let body="";req.on("data",chunk=>body+=chunk);req.on("end",()=>{const data=new URLSearchParams(body);assert.equal(data.get("token"),state.token);calls.push({url:req.url,data});++wifiScanRequests;if(Date.now()>=wifiScanFinish){++wifiScanGeneration;wifiScanFinish=Date.now()+800;}if(wifiDropAck){wifiDropAck=false;res.destroy();return;}res.statusCode=202;res.end(JSON.stringify({code:"wifi_scanning"}));});}
    else {
      if(wifiReadFailures>0){--wifiReadFailures;res.statusCode=503;res.end("{}");return;}
      const respond=()=>res.end(JSON.stringify({phase:!wifiScanGeneration?"idle":Date.now()<wifiScanFinish?"scanning":wifiRadioFailure?"failed":"ready",generation:wifiScanGeneration,ageMs:0,attempts:wifiRadioFailure?3:1,error:wifiRadioFailure,driverCode:wifiRadioFailure?0x3006:0,networks:nearbyNetworks}));
      if(wifiReadDelay)setTimeout(respond,wifiReadDelay);else respond();
    }
    return;
  }
  if (req.url.startsWith("/api/uart?")) {
    if (flowAuto && Date.now() - lastFlowFrame > 1200) {
      lastFlowFrame = Date.now();
      appendFlow();
    }
    const after = Number(
      new URL(req.url, "http://localhost").searchParams.get("after"),
    );
    res.end(
      JSON.stringify({
        autoDirection: state.direction === "auto",
        boot: flowBoot,
        sequence: flowSequence,
        nowUs: Math.floor((Date.now() % 1000000) * 1000),
        capacity: 32,
        gap: flowGap,
        diagnosticActive: diagnostic.active,
        more:flowEvents.filter((e)=>!after||e.seq>after).length>8,
        events: malformedFlow ? [null] : flowEvents.filter((e) => !after || e.seq > after).slice(0,8),
      }),
    );
    return;
  }
  if (req.url === "/api/diagnostic" && req.method === "GET") {
    ++diagnosticRequests;
    if (diagnosticHold) return;
    res.end(JSON.stringify(malformedDiagnostic ? {...diagnostic,settings:null} : diagnostic));
    return;
  }
  if (req.url === "/api/system") {
    ++systemRequests;
    if (malformedSystem) { res.end(JSON.stringify({ cores: [null], tasks: [null] })); return; }
    res.end(
      JSON.stringify({
        cpuMHz: 240,
        taskCount: 18,
        heapFree: 143360,
        heapMin: 122880,
        heapLargest: 102400,
        heapTotal: 250000,
        psramFree: 8000000,
        psramTotal: 8388608,
        uiCycleMs: 5,
        acquisitionCycleMs: 101,
        commandQueue: 0,
        diagnosticQueue: 0,
        cores: [
          { id: 0, load: 17.4 },
          { id: 1, load: 6.2 },
        ],
        tasks: [
          {
            name: "loopTask / HTTP + UI",
            core: 0,
            state: "running",
            priority: 1,
            stackFreeMin: 9216,
          },
          {
            name: "fort-acquisition / UART",
            core: 1,
            state: "blocked",
            priority: 2,
            stackFreeMin: 8192,
          },
          {
            name: "IDLE0",
            core: 0,
            state: "ready",
            priority: 0,
            stackFreeMin: 1024,
          },
          {
            name: "IDLE1",
            core: 1,
            state: "ready",
            priority: 0,
            stackFreeMin: 1024,
          },
        ],
      }),
    );
    return;
  }
  if(req.url.startsWith("/api/meter-lab")&&req.method==="GET"){
    ++labRequests;if(req.url.includes("download=1"))res.setHeader("Content-Disposition","attachment; filename=meter-lab-report.json");
    res.setHeader("Content-Type","application/json");res.end(JSON.stringify(labMalformed?{...lab,saved:[null]}:lab));return;
  }
  if (req.url === "/api/status") {
    if (fail) {
      res.statusCode = 503;
      res.end("{}");
      return;
    }
    res.end(
      JSON.stringify({
        ...state,
        display:malformedDisplay?{idleSeconds:"bad"}:state.display,
        network: malformedNetwork ? {...state.network,address:{bad:true}} : state.network,
        slots: malformedStatus ? Array(9).fill(null) : state.slots,
        online: online && state.scan.phase !== "running" && !diagnostic.active,
        checkpoints:malformedCheckpoints?{...state.checkpoints,rows:[]} : state.checkpoints,
        diagnosticActive: diagnostic.active,
      }),
    );
    return;
  }
  if (req.url === "/api/trace") {
    ++traceRequests;
    if (traceFail) {
      res.statusCode = 503;
      res.end("{}");
      return;
    }
    res.end(JSON.stringify(malformedTrace ? { ...trace, frames: [null] } : trace));
    return;
  }
  if(req.url.startsWith("/api/history?kind=checkpoints")){
    res.end(JSON.stringify([{day:state.checkpoints.day,date:state.checkpoints.date},{day:historicalCheckpoints.day,date:historicalCheckpoints.date}]));return;
  }
  if(req.url.startsWith("/api/checkpoints")){
    const url=new URL(req.url,"http://localhost");
    const day=Number(url.searchParams.get("day")||state.checkpoints.day),record=day===state.checkpoints.day?state.checkpoints:historicalCheckpoints;
    if(url.searchParams.get("format")==="csv"){
      res.setHeader("Content-Type","text/csv");res.setHeader("Content-Disposition",`attachment; filename=fort-checkpoints-${record.date}.csv`);
      res.end("date,scheduled_wib,cumulative_kwh,status\n"+record.rows.map(row=>`${record.date},${String(Math.floor(row.minute/60)).padStart(2,"0")}:${String(row.minute%60).padStart(2,"0")},${row.kwh??""},${row.status}`).join("\n"));return;
    }
    res.end(JSON.stringify(malformedCaptureArchive?{...record,rows:record.rows.slice(0,2)}:record));return;
  }
  if (req.url === "/api/history") {
    if (malformedHistory) { res.end("[null]"); return; }
    res.end(
      JSON.stringify([
        { day: 20752, date: "2026-10-26" },
        { day: 20751, date: "2026-10-25" },
      ]),
    );
    return;
  }
  if (req.url.startsWith("/api/export")) {
    res.setHeader("Content-Type", "text/csv");
    res.setHeader(
      "Content-Disposition",
      "attachment; filename=fort-2026-10-26.csv",
    );
    res.end("date,shift,quality\n2026-10-26,morning,complete_estimated\n");
    return;
  }
  if (req.method === "POST") {
    let body = "";
    req.on("data", (chunk) => (body += chunk));
    req.on("end", () => {
      const data = new URLSearchParams(body);
      calls.push({ url: req.url, data });
      assert.equal(data.get("token"), state.token);
      if (req.url.startsWith("/api/scan/") && hangScan) return;
      if (req.url.startsWith("/api/scan/") && rejectScan) {
        res.statusCode = 503;
        res.end(
          JSON.stringify({
            code: "queue_busy",
            message: "Command queue is busy. Try again.",
          }),
        );
        return;
      }
      const normalControl = req.url !== "/api/diagnostic";
      const commandId = normalControl ? ++state.control.accepted : null;
      const reject = normalControl && rejectNextControl;
      if (normalControl) {
        ++state.control.pending;
        rejectNextControl = false;
      }
      const apply = () => {
        if (reject) {
          state.control.rejected = commandId;
          state.control.lastError = "scan_busy";
        } else {
      if(req.url==="/api/meter-lab/stop"){lab.discovery.phase="cancelled";state.discoveryRunning=false;}
      if(req.url==="/api/meter-lab"){
        const action=data.get("action");
        if(action==="start"){
          const run=lab.discovery.run+1;lab.discovery={...lab.discovery,run,phase:"running",tested:0,total:7,elapsedMs:0,plan:data.get("plan").split(",").map(s=>({address:parseInt(s.split(":")[0],16),words:Number(s.split(":")[1])}))};lab.findings=[];state.discoveryRunning=true;
          if(!labHold)setTimeout(()=>{lab.discovery.phase="complete";lab.discovery.tested=7;lab.findings=[0,1].map(index=>({...labEvidence(index),run,index}));lab.current=lab.findings[1];state.discoveryRunning=false;},200);
        }
        if(action==="save"){const evidence=lab.findings[Number(data.get("index"))];lab.saved.push({id:labNextId++,name:data.get("name"),model:data.get("model"),notes:data.get("notes"),profile:data.get("profile"),verified:data.get("verified")==="1",activatable:data.get("profile")!=="unknown"&&data.get("verified")==="1",evidence});}
        if(action==="remove")lab.saved=lab.saved.filter(row=>row.id!==Number(data.get("id")));
        if(action==="select"){const saved=lab.saved.find(row=>row.id===Number(data.get("id")));lab.activeId=saved.id;state.savedMeterId=saved.id;state.meterName=saved.name;state.profile=saved.profile;state.meter=meterCatalog.find(p=>p.key===saved.profile);state.slave=saved.evidence.slave;state.baud=saved.evidence.baud;state.parity=saved.evidence.parity;state.direction=saved.evidence.direction;}
      }
      if (req.url === "/api/diagnostic") {
        const command = data.get("command");
        if (command === "resume") {
          diagnostic.active = false;
          diagnostic.log += "[RESUMED] Production settings restored.\n";
        } else if (command === "x") {
          diagnostic.busy = false;
          diagnostic.log += "Stopped. Queued commands cancelled.\n";
        } else {
          const passive=["status","help","report","compare","plan show"].includes(command);
          if (!passive && !diagnostic.active) ++diagnostic.generation;
          if (!passive) diagnostic.active = true;
          const [key,arg] = command.split(" ");
          if (["timeout","gap","settle","baud","id"].includes(key)) diagnostic.settings[key==="id"?"slave":key]=Number(arg);
          if (key==="direction") diagnostic.settings.direction=arg;
          if (key==="parity") diagnostic.settings.parity=arg;
          diagnostic.log += `[COMMAND_INPUT] ${command}\n`;
          if (command === "diagnose") {
            diagnostic.evidence={requests:8,rxBytes:0,data:0,exceptions:0,timeouts:8};
            diagnostic.log +=
              "[REPORT_BEGIN]\n[EVIDENCE] requests=8 valid_data=0\n[NO_RX_UNRESOLVED] No accepted reply.\n[REPORT_END]\n";
          } else
            diagnostic.log +=
              "[SETTINGS] ID=1 baud=9600 8N1 RX=18 TX=17. RAM only\n";
        }
      }
      if (req.url === "/api/trace/start") {
        ++trace.generation;
        trace.active = true;
        trace.count = 4;
        const tx = "01 03 00 82 00 02 64 23";
        trace.frames = ["echo", "crc", "timeout", "valid"].map(
          (result, index) => ({
            atMs: 10000 + index * 100,
            baud: 9600,
            parity: "N",
            slave: 1,
            address: 130,
            words: 2,
            attempt: 1,
            scan: true,
            overflow: false,
            result,
            tx,
            rx:
              result === "echo"
                ? tx
                : result === "timeout"
                  ? ""
                  : result === "valid"
                    ? "01 03 04 00 00 C3 17 EA CD"
                    : "01 03 04 00 00 C3 17 00 00",
            rxBytes: result === "echo" ? 8 : result === "timeout" ? 0 : 9,
          }),
        );
      }
      if (req.url === "/api/trace/stop") trace.active = false;
      if(req.url==="/api/display"){state.display.idleSeconds=Number(data.get("idleSeconds"));state.display.stored=true;}
      if (req.url === "/api/network") {
        const mode=data.get("mode"),previous=state.network;
        state.network={...previous,configuredAPIP:data.get("apIP")||previous.configuredAPIP,mode,activeMode:mode,stored:true,pending:false,connected:mode!=="ap",phase:mode==="ap"?"ap":"online",rssi:mode==="ap"?0:-53,clientIP:mode==="ap"?"":"192.168.1.84",apActive:mode!=="client",apIP:mode==="client"?"":"192.168.4.1",address:mode==="ap"?"192.168.4.1":"192.168.1.84"};
        if(mode!=="ap"){state.network.ssid=data.get("ssid");state.network.hasPassword=data.get("open")!=="1";}
      }
      if (req.url === "/api/config"){
        Object.assign(state, {
          profile:data.get("profile")||state.profile,
          slave: Number(data.get("slave")),
          baud: Number(data.get("baud")),
          parity: data.get("parity"),
          direction: data.get("direction"),
        });
        state.meter=meterCatalog.find(p=>p.key===state.profile);state.scan.address=state.meter.currentA.address;state.scan.words=state.meter.currentA.words;
      }
      if (req.url === "/api/scan/start")
        state.scan = {
          phase: scanMode,
          tested:
            scanMode === "running"
              ? Math.min(
                  2,
                  (Number(data.get("last")) - Number(data.get("first")) + 1) *
                    (data.get("mode") === "current" ? 1 : 18),
                )
              : (Number(data.get("last")) - Number(data.get("first")) + 1) *
                (data.get("mode") === "current" ? 1 : 18),
          total:
            (Number(data.get("last")) - Number(data.get("first")) + 1) *
            (data.get("mode") === "current" ? 1 : 18),
          mode: data.get("mode") || "matrix",
          address: Number(data.get("address") || 88),
          words: Number(data.get("words") || 2),
          first: Number(data.get("first")),
          last: Number(data.get("last")),
          elapsed: 3,
          slave: 7,
          baud: 19200,
          parity: "E",
          timeouts: 2,
          crcErrors: 1,
          rejected: 0,
          ...(scanMode === "found"
            ? { result: { slave: 7, baud: 19200, parity: "E", direction: state.direction } }
            : {}),
        };
      if (req.url === "/api/scan/cancel") state.scan.phase = "cancelled";
      if (req.url === "/api/scan/apply")
        Object.assign(state, state.scan.result);
      if (req.url === "/api/led")
        Object.assign(state.led, {
          mode: data.get("mode"),
          brightness: Number(data.get("brightness")),
          period: Number(data.get("period")),
          red: Number(data.get("red")),
          green: Number(data.get("green")),
          blue: Number(data.get("blue")),
          stored: true,
          storageOK: true,
        });
      if (req.url === "/api/mute") state.muted = !state.muted;
        }
        if (normalControl) {
          state.control.completed = commandId;
          state.control.results.push({ id: commandId, error: reject ? "scan_busy" : "" });
          state.control.results = state.control.results.slice(-8);
          --state.control.pending;
        }
      };
      if (normalControl && delayedControls) setTimeout(apply, delayedControls);
      else apply();
      res.statusCode = 202;
      res.end(JSON.stringify({ code: "accepted", message: "Command queued.", ...(normalControl ? { accepted: true, commandId } : {}) }));
    });
    return;
  }
  res.statusCode = 404;
  res.end("{}");
});

async function checkVectors(browser, out) {
  const assets = path.join(root, "docs/assets");
  const files = [];
  const collect = (dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const file = path.join(dir, entry.name);
      if (entry.isDirectory()) collect(file);
      else if (file.endsWith(".svg")) files.push(file);
    }
  };
  collect(assets);
  const page = await browser.newPage();
  for (const file of files) {
    const svg = fs.readFileSync(file, "utf8"),
      box = svg.match(/viewBox="0 0 (\d+) (\d+)"/);
    assert(box, "SVG viewBox missing: " + file);
    await page.setViewportSize({
      width: Number(box[1]),
      height: Number(box[2]),
    });
    await page.setContent('<body style="margin:0">' + svg + "</body>");
    await page.evaluate(() => document.fonts.ready);
    const overflow = await page.evaluate(() => {
      const svg = document.querySelector("svg"),
        v = svg.viewBox.baseVal;
      return [...svg.querySelectorAll("text")]
        .filter((text) => {
          const b = text.getBBox();
          return (
            b.x < -0.5 ||
            b.y < -0.5 ||
            b.x + b.width > v.width + 0.5 ||
            b.y + b.height > v.height + 0.5
          );
        })
        .map((text) => text.textContent);
    });
    assert.deepEqual(overflow, [], "SVG text overflow: " + path.basename(file));
    if (!file.includes(path.sep + "badges" + path.sep))
      await page.screenshot({
        path: path.join(out, path.basename(file, ".svg") + ".png"),
        fullPage: true,
      });
  }
  await page.close();
  return files.length;
}

(async () => {
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const out = path.join(root, "output/previews"),
    assets = path.join(root, "docs/assets");
  fs.mkdirSync(out, { recursive: true });
  let browser;
  try {
    await testRequest();await testFlow();
    browser = await chromium.launch({
      headless: true,
      ...(process.argv[3] ? { channel: process.argv[3] } : {}),
    });
    if(process.argv.includes("--meter-lab-only")){
      const page=await browser.newPage({viewport:{width:1440,height:1100}});
      await require("./test-meter-lab.cjs")({page,origin,lab,calls,setMalformed:value=>labMalformed=value,setHold:value=>labHold=value,requests:()=>labRequests,root});
      return;
    }
    if(process.argv.includes("--design-only")){
      const context=await browser.newContext({viewport:{width:1440,height:1100}});
      const page=await context.newPage();await page.goto(origin);await page.waitForFunction(()=>document.querySelectorAll("#checkpoint-rows tr").length===5);await page.waitForSelector(".flow-train.tx");await page.waitForTimeout(350);
      await page.locator("#uart-flow").screenshot({path:path.join(assets,"ui-flow-desktop.png"),style:".main-nav{visibility:hidden}"});
      await page.locator(".main-nav").screenshot({path:path.join(assets,"ui-navigation.png")});
      await page.locator("#clock").screenshot({path:path.join(assets,"ui-clock.png")});
      await page.locator(".energy-surface").screenshot({path:path.join(assets,"ui-energy.png")});
      await page.locator("#checkpoints").screenshot({path:path.join(assets,"ui-checkpoints-desktop.png")});
      await page.setViewportSize({width:390,height:844});await page.waitForTimeout(500);
      await page.locator("#uart-flow").screenshot({path:path.join(assets,"ui-flow-mobile.png"),style:".main-nav{visibility:hidden}"});
      await page.locator("#checkpoints").screenshot({path:path.join(assets,"ui-checkpoints-mobile.png")});
      if(process.argv.includes("--record-flow")||process.argv.includes("--record-energy")){
        const energyRecording=process.argv.includes("--record-energy");
        const selector=energyRecording?".energy-surface":".flow-playback";
        const mediaName=energyRecording?"ui-energy-demo":"ui-flow-demo";
        await page.setViewportSize({width:1160,height:760});
        const hide=await page.addStyleTag({content:".main-nav{visibility:hidden}"});
        await page.locator(selector).scrollIntoViewIfNeeded();
        const box=await page.locator(selector).boundingBox();
        flowAuto=false;
        await page.waitForFunction(()=>document.querySelector(".flow-playback")?.dataset.phase==="idle");
        await page.waitForTimeout(600);
        const cdp=await context.newCDPSession(page),frames=[];
        const folder=path.join(out,energyRecording?"energy-recording":"flow-recording");fs.mkdirSync(folder,{recursive:true});
        const receive=({data,sessionId,metadata})=>{
          const name=`frame-${String(frames.length).padStart(5,"0")}.jpg`;
          fs.writeFileSync(path.join(folder,name),Buffer.from(data,"base64"));
          frames.push({name,time:metadata.timestamp});
          cdp.send("Page.screencastFrameAck",{sessionId}).catch(()=>{});
        };
        cdp.on("Page.screencastFrame",receive);
        await cdp.send("Page.startScreencast",{format:"jpeg",quality:92,everyNthFrame:2});
        await page.waitForTimeout(400);
        if(energyRecording){
          for(const value of [128432.735,128432.756]){
            state.energy=value;await page.locator("#refresh").evaluate(button=>button.click());
            await page.waitForFunction(value=>document.getElementById("energy")?.dataset.value===value.toLocaleString("en-GB",{minimumFractionDigits:3,maximumFractionDigits:3}),value);
            await page.waitForTimeout(1400);
          }
        }else{
          appendFlow();
          await page.waitForSelector(".flow-train.tx");
          await page.waitForFunction(()=>document.querySelector(".flow-playback")?.dataset.phase==="idle");
          await page.waitForTimeout(450);
        }
        await cdp.send("Page.stopScreencast");cdp.off("Page.screencastFrame",receive);await cdp.detach();
        assert(frames.length>25,"The demo must contain actual changing browser frames");
        const list=frames.map((frame,index)=>`file '${frame.name}'\nduration ${Math.max(.01,(frames[index+1]?.time??frame.time+.45)-frame.time).toFixed(6)}`).join("\n");
        fs.writeFileSync(path.join(folder,"frames.txt"),list+`\nfile '${frames.at(-1).name}'\n`);
        const crop=[box.width,box.height,box.x,box.y].map(value=>Math.max(0,Math.floor(value/2)*2)).join(":");
        const result=require("node:child_process").spawnSync("ffmpeg",["-v","error","-y","-f","concat","-safe","1","-i",path.join(folder,"frames.txt"),"-vf",`crop=${crop}`,"-r","30","-c:v","libx264","-pix_fmt","yuv420p","-an","-movflags","+faststart",path.join(assets,mediaName+".mp4")],{encoding:"utf8"});
        assert.equal(result.status,0,result.stderr);await hide.evaluate(node=>node.remove());
        fs.copyFileSync(path.join(assets,mediaName+".mp4"),path.join(root,`output/${energyRecording?"energy-counter":"flow-replay"}-v${version}.mp4`));
        console.log(`PASS: ${frames.length} real browser frames recorded as docs/assets/${mediaName}.mp4`);
      }
      if(!process.argv.includes("--record-flow")&&!process.argv.includes("--record-energy")){
        await page.setViewportSize({width:1440,height:1100});await page.goto(`${origin}/#setup`);
        await page.waitForSelector("#meter-profile");await page.locator("#meter-profile").selectOption("acrel-adl400");
        await page.locator("#config button").click();await page.waitForFunction(()=>document.querySelector("#scan-probe")?.value==="0064");
        await page.locator("#config").screenshot({path:path.join(assets,"ui-meter-profile.png"),style:".main-nav{visibility:hidden}"});
      }
      console.log("PASS: shipped UI design previews captured");return;
    }
    if (process.argv.includes("--vectors-only")) {
      console.log(
        "PASS: " + (await checkVectors(browser, out)) + " SVGs rendered",
      );
      return;
    }
    const errors = [],
      external = [];
    const page = await browser.newPage({
      viewport: { width: 1440, height: 1050 },
    });
    page.on("pageerror", (error) => errors.push(error.message));
    await page.route("**/*", (route) => {
      if (!route.request().url().startsWith(origin)) {
        external.push(route.request().url());
        route.abort();
      } else route.continue();
    });
    const refresh = async () => {
      await page.locator("#refresh").click();
    };
    await page.goto(origin);
    await page.waitForFunction(
      () => document.querySelectorAll("#intervals tr").length === 6,
    );
    assert.equal(await page.locator("html").getAttribute("lang"), "en");
    await page.waitForFunction(()=>document.querySelectorAll("#checkpoint-rows tr").length===5);
    assert((await page.locator("#checkpoint-rows").innerText()).includes("06:00"));
    assert((await page.locator("#checkpoint-rows").innerText()).includes("128,400.000"));
    assert((await page.locator('[data-minute="420"]').innerText()).includes("Missed"));
    await page.locator("#checkpoint-differences").check();
    assert((await page.locator("#checkpoint-rows").innerText()).includes("8.000"));
    await page.locator('[data-capture-shift="1"]').click();
    assert((await page.locator("#checkpoint-rows").innerText()).includes("19:00"));
    assert((await page.locator("#checkpoint-rows").innerText()).includes("Waiting"));
    await page.locator('[data-capture-shift="0"]').click();
    malformedCheckpoints=true;await refresh();
    await page.waitForFunction(()=>document.getElementById("checkpoints")?.textContent.includes("Connection or checkpoint data is unavailable"));
    assert((await page.locator("#checkpoint-rows").innerText()).includes("128,400.000"),"Malformed checkpoint data retains the last accepted record");
    malformedCheckpoints=false;await refresh();
    await page.locator("#checkpoint-history > summary").click();
    await page.waitForFunction(()=>document.querySelectorAll("#checkpoint-day option").length===2);
    await page.locator("#checkpoint-day").selectOption(String(historicalCheckpoints.day));
    await page.waitForFunction(date=>document.getElementById("checkpoint-date")?.textContent.includes(date),historicalCheckpoints.date);
    await page.locator('[data-capture-shift="1"]').click();
    assert((await page.locator("#checkpoint-rows").innerText()).includes("18:30"));
    const cpDownload=page.waitForEvent("download");await page.locator("#checkpoint-export").click();
    assert((await cpDownload).suggestedFilename().includes("checkpoints"));
    await page.locator("#checkpoint-day").selectOption("");
    await page.waitForFunction(date=>document.getElementById("checkpoint-date")?.textContent.includes(date),state.checkpoints.date);
    malformedCaptureArchive=true;await page.locator("#checkpoint-day").selectOption(String(historicalCheckpoints.day));
    await page.waitForFunction(()=>document.getElementById("checkpoints")?.textContent.includes("could not be loaded"));
    assert.equal(await page.locator("#checkpoint-rows").count(),0,"Do not show today's values under a failed historical date");
    malformedCaptureArchive=false;await page.locator("#checkpoint-day").selectOption("");
    await page.locator('[data-capture-shift="0"]').click();await page.locator("#checkpoint-differences").uncheck();
    await page.locator("#checkpoint-history > summary").click();

    assert.equal(await page.locator("#energy").getAttribute("data-value"), "128,432.728");
    assert.equal(await page.locator("#energy .counter").count(),1,"The primary energy value uses the supplied Counter");
    for(const [value,label] of [[999.999,"999.999"],[1000.001,"1,000.001"],[42.125,"42.125"],[0,"0.000"]]){
      state.energy=value;await refresh();
      await page.waitForFunction(label=>document.getElementById("energy")?.dataset.value===label,label);
      assert.equal(await page.locator("#energy .counter__sr").textContent(),label,"The accessible value must expose the accepted reading, including carry and decreases");
    }
    state.energy=null;await refresh();
    await page.waitForFunction(()=>document.getElementById("energy")?.dataset.value==="—");
    assert.equal(await page.locator("#energy .counter").count(),0,"Missing energy must not animate as zero");
    state.energy=128432.728;await refresh();
    await page.waitForFunction(()=>document.getElementById("energy")?.dataset.value==="128,432.728");

    assert.equal(await page.locator("#value-currentA").textContent(), "4.994 A");
    assert((await page.locator("#register-title").innerText()).includes("register"));
    malformedStatus = true;
    await refresh();
    await page.waitForFunction(() => document.getElementById("health")?.textContent.includes("Connection lost"));
    assert.equal(await page.locator("#energy").getAttribute("data-value"), "128,432.728", "Malformed status retains the accepted snapshot");
    malformedStatus = false;
    await refresh();
    await page.waitForFunction(() => !document.getElementById("health")?.textContent.includes("Connection lost"));
    assert.equal(
      await page.locator("#version").textContent(),
      "Firmware " + version,
    );
    const clockStart=await page.locator("#clock .device-clock").getAttribute("data-time");
    const beforeClockWrites=calls.filter(row=>row.url==="/api/time").length;
    await page.waitForTimeout(2200);
    assert.notEqual(await page.locator("#clock .device-clock").getAttribute("data-time"),clockStart,"The clock must advance between status samples");
    await page.waitForTimeout(13200);
    assert.notEqual(await page.locator("#clock .device-clock").getAttribute("data-time"),"—","Fresh identical status samples must keep the running clock alive");
    assert.equal(calls.filter(row=>row.url==="/api/time").length,beforeClockWrites,"Clock animation must never synchronize the RTC automatically");
    await page.locator("#legacy-intervals").evaluate(node=>node.open=true);
    await page.locator('[data-shift="1"]').click();
    assert.equal(await page.locator("#intervals tr").count(), 3);
    assert((await page.locator("#intervals").innerText()).includes("16:30"));
    assert((await page.locator(".shift-total").innerText()).includes("—")); // No complete samples, not zero energy.
    await page.locator('[data-shift="0"]').click();
    malformedHistory = true;
    await page.locator("#nav-device").click();
    await page.getByText("Archive list could not be loaded.", { exact: true }).waitFor();
    assert(await page.locator("#mute").isVisible(), "Malformed history must not crash the device page");
    malformedHistory = false;
    await page.locator("#nav-overview").click();
    await page.locator("#nav-device").click();
    await page.waitForFunction(() => document.querySelectorAll("#history option").length === 2);
    await page.locator("#network-mode").selectOption("dual");
    await page.waitForFunction(()=>document.querySelectorAll("#network-ssid option").length>=4);
    await page.locator("#network-ssid").selectOption("Plant WiFi/false");
    await page.locator("#network-password").fill("plant-secret-123");
    await page.locator("#network-save").click();
    await page.waitForFunction(() => document.getElementById("network-status")?.textContent.includes("Client connected"));
    assert(calls.some(c=>c.url==="/api/network" && c.data.get("mode")==="dual" && c.data.get("password")==="plant-secret-123"));
    assert(!JSON.stringify(state.network).includes("plant-secret-123"));
    await page.reload();await page.locator("#nav-device").click();
    await page.waitForFunction(() => document.getElementById("network-mode")?.value==="dual");
    assert.equal(await page.locator("#network-password").inputValue(),"");
    await page.locator("#network-mode").selectOption("client");await page.locator("#network-save").click();
    await page.waitForFunction(() => !document.getElementById("network-save")?.disabled);
    assert(calls.some(c=>c.url==="/api/network" && c.data.get("mode")==="client" && c.data.get("keep")==="1"));
    state.network.connected=false;state.network.phase="fallback";state.network.clientIP="";state.network.apActive=true;state.network.apIP="192.168.4.1";state.network.address="192.168.4.1";
    await refresh();await page.waitForFunction(() => document.getElementById("network-status")?.textContent.includes("recovery AP"));
    await page.locator("#network-ap-ip").fill("8.8.8.8");
    const beforeInvalidNetwork=calls.length;await page.locator("#network-save").click();assert.equal(calls.length,beforeInvalidNetwork);
    await page.locator("#network-ap-ip").fill("192.168.8.1");
    await page.waitForFunction(()=>document.querySelectorAll("#network-ssid option").length>=4);
    await page.locator("#network-ssid").selectOption("Open test network/true");
    await page.locator("#network-mode").selectOption("dual");await page.locator("#network-save").click();
    await page.waitForFunction(() => !document.getElementById("network-save")?.disabled);
    assert(calls.some(c=>c.url==="/api/network" && c.data.get("open")==="1"));
    await page.locator("#network-ap-ip").fill("192.168.9.1");
    malformedNetwork=true;await refresh();await page.waitForFunction(() => document.getElementById("network-save")?.disabled);
    assert(await page.locator("#mute").isVisible());malformedNetwork=false;await refresh();await page.waitForFunction(() => !document.getElementById("network-save")?.disabled);
    assert.equal(await page.locator("#network-ap-ip").inputValue(),"192.168.9.1", "A malformed refresh must not erase the operator draft");
    const rescanWiFi=async()=>{
      // Wait for this queued POST, not the previous scan's retained result text.
      const posted=page.waitForRequest(request=>request.method()==="POST"&&request.url()===origin+"/api/network/scan");
      await page.locator("#wifi-rescan").click();
      await posted;
      await page.waitForFunction(()=>!document.getElementById("wifi-rescan")?.disabled);
    };
    const scanRequestsBeforeRefresh=wifiScanRequests;
    await rescanWiFi();
    assert(wifiScanRequests>scanRequestsBeforeRefresh,"Network scanning must be available without typing an SSID");
    wifiDropAck=true;const beforeLostAck=wifiScanGeneration;
    await rescanWiFi();
    await page.waitForFunction(()=>document.getElementById("wifi-scan-status")?.textContent.includes("networks found"));
    assert.equal(wifiScanGeneration,beforeLostAck+1,"A lost POST acknowledgement must recover the same scan instead of starting another");
    wifiRadioFailure="radio_start";
    await rescanWiFi();
    await page.waitForFunction(()=>document.getElementById("wifi-scan-status")?.textContent.includes("radio scan failed"));
    assert((await page.locator("#wifi-scan-detail").innerText()).includes("radio_start"));
    assert(await page.locator('#network-ssid option[value="Plant WiFi/false"]').count(),"A failed scan retains the previous SSIDs");
    wifiRadioFailure="";wifiReadFailures=1;
    await rescanWiFi();
    await page.waitForFunction(()=>document.getElementById("wifi-scan-status")?.textContent.includes("networks found"));
    wifiReadDelay=3100;
    await rescanWiFi();
    await page.waitForFunction(()=>document.getElementById("wifi-scan-status")?.textContent.includes("networks found"));
    wifiReadDelay=0;
    await page.locator("#network-ssid").blur();
    await page.setViewportSize({width:1440,height:1200});
    await page.locator("#network-panel").screenshot({path:path.join(assets,"ui-network-desktop.png")});
    await page.setViewportSize({width:390,height:1800});
    await page.locator("#network-panel").screenshot({path:path.join(assets,"ui-network-mobile.png")});
    await page.setViewportSize({width:1440,height:1000});
    await page.locator("#network-mode").selectOption("ap");await page.locator("#network-save").click();
    await page.waitForFunction(() => !document.getElementById("network-save")?.disabled);
    assert.equal(await page.locator("#display-idle").inputValue(),"60");
    await page.locator("#display-idle").fill("120");await page.locator("#display-save").click();
    await page.waitForFunction(()=>!document.getElementById("display-save")?.disabled);
    assert(calls.some(row=>row.url==="/api/display"&&row.data.get("idleSeconds")==="120"));
    await page.reload();await page.locator("#nav-device").click();
    await page.waitForFunction(()=>document.getElementById("display-idle")?.value==="120");
    await page.locator("#display-enabled").uncheck();await page.locator("#display-save").click();
    await page.waitForFunction(()=>!document.getElementById("display-save")?.disabled);
    assert(calls.some(row=>row.url==="/api/display"&&row.data.get("idleSeconds")==="0"));
    malformedDisplay=true;await refresh();await page.waitForFunction(()=>document.getElementById("display-save")?.disabled);
    assert(await page.locator("#mute").isEnabled());malformedDisplay=false;await refresh();await page.waitForFunction(()=>!document.getElementById("display-save")?.disabled);
    await page.locator("#display-enabled").check();await page.locator("#display-idle").fill("60");await page.locator("#display-save").click();
    await page.waitForFunction(()=>!document.getElementById("display-save")?.disabled);
    await page.setViewportSize({width:1440,height:1000});await page.locator("#display-panel").screenshot({path:path.join(assets,"ui-display-desktop.png")});
    await page.setViewportSize({width:390,height:1400});await page.locator("#display-panel").screenshot({path:path.join(assets,"ui-display-mobile.png")});await page.setViewportSize({width:1440,height:1000});
    delayedControls = 1200;
    await page.locator("#mute").click();
    await page.waitForTimeout(300);
    assert(await page.locator("#mute").isDisabled(), "202 admission must not unlock controls before UART-owner completion");
    await page.waitForFunction(() => document.getElementById("mute")?.textContent.includes("Enable alarm"));
    await page.waitForFunction(() => !document.getElementById("mute")?.disabled);
    delayedControls = 0;
    rejectNextControl = true;
    await page.locator("#mute").click();
    await page.waitForFunction(() => document.getElementById("message")?.textContent.includes("Cancel the scan"));
    assert((await page.locator("#mute").innerText()).includes("Enable alarm"), "Rejected commands must not appear applied");
    assert.equal(await page.locator("#led-mode").inputValue(), "hue");
    assert.equal(await page.locator("#led-brightness").inputValue(), "128");
    assert(await page.locator("#led-color").isDisabled());
    await page.locator("#led-mode").selectOption("blink");
    await page.locator("#led-color").fill("#1234ab");
    await page.locator("#led-period").fill("1.5");
    await page.locator("#led-brightness").fill("200");
    await refresh();
    await page.waitForTimeout(100);
    assert.equal(
      await page.locator("#led-mode").inputValue(),
      "blink",
      "Polling must preserve the unsaved LED draft",
    );
    await page.locator("#led-save").click();
    await page.waitForFunction(() =>
      document.getElementById("led-applied")?.textContent.includes("Blink"),
    );
    const ledCall = calls.find((call) => call.url === "/api/led");
    assert(ledCall);
    assert.equal(ledCall.data.get("red"), "18");
    assert.equal(ledCall.data.get("green"), "52");
    assert.equal(ledCall.data.get("blue"), "171");
    assert.equal(ledCall.data.get("brightness"), "200");
    assert.equal(ledCall.data.get("period"), "1500");
    await page.reload();
    await page.locator("#nav-device").click();
    await page.waitForFunction(
      () => document.getElementById("led-mode")?.value === "blink",
    );
    for (const mode of ["solid", "breathe", "off", "hue"]) {
      await page.locator("#led-mode").selectOption(mode);
      assert.equal(
        await page.locator("#led-period").isDisabled(),
        ["solid", "off"].includes(mode),
      );
      await page.locator("#led-save").click();
      await page.waitForFunction(
        (expected) =>
          document
            .getElementById("led-applied")
            ?.textContent.includes(expected),
        {
          solid: "Solid color",
          breathe: "Breathing",
          off: "Off",
          hue: "Hue rotation",
        }[mode],
      );
      await page.waitForFunction(
        () => !document.getElementById("led-save").disabled,
      );
    }
    state.led.available = false;
    await refresh();
    await page.waitForFunction(
      () => document.getElementById("led-save").disabled,
    );
    state.led.available = true;
    state.led.storageOK = false;
    await refresh();
    await page.waitForFunction(() =>
      document
        .querySelector(".led-panel .hint")
        ?.textContent.includes("Storage failed"),
    );
    state.led.storageOK = true;
    await refresh();
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(root, "docs/assets/ui-led-mobile.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.screenshot({
      path: path.join(root, "docs/assets/ui-led-desktop.png"),
      fullPage: true,
    });
    const timeResponse=page.waitForResponse(response=>response.url().endsWith("/api/time")&&response.request().method()==="POST");
    await page.locator("#sync").click();
    await timeResponse;
    await page.waitForFunction(() =>
      document.getElementById("message")?.textContent.includes("queued"),
    );
    assert(
      calls.some(
        (call) =>
          call.url === "/api/time" &&
          Number(call.data.get("epoch")) > 1704067200,
      ),
    );
    await page.locator("#history").selectOption("20751");
    const download = page.waitForEvent("download");
    await page.locator("#download").click();
    assert((await download).suggestedFilename().endsWith(".csv"));
    await page.locator("#nav-setup").click();
    assert.equal(
      traceRequests,
      0,
      "Closed raw panel must not poll its endpoint",
    );
    await page.locator("#raw-trace summary").click();
    await page.waitForFunction(
      () =>
        document.getElementById("trace-start") &&
        !document.getElementById("trace-start").disabled,
    );
    await page.locator("#trace-start").click();
    await page.waitForFunction(
      () => document.querySelectorAll("#trace-frames article").length === 4,
    );
    assert(
      (await page.locator("#trace-frames").innerText()).includes(
        "Exact TX echo",
      ),
    );
    assert(
      (await page.locator("#trace-frames").innerText()).includes(
        "CRC mismatch",
      ),
    );
    assert(
      (await page.locator("#trace-frames").innerText()).includes(
        "Valid response",
      ),
    );
    assert(await page.locator("#trace-start").isDisabled());
    await page.locator("#trace-stop").click();
    await page.waitForFunction(
      () => document.getElementById("trace-stop").disabled,
    );
    assert(calls.some((call) => call.url === "/api/trace/start"));
    const rawDownloadPromise = page.waitForEvent("download");
    await page.locator("#trace-download").click();
    const rawDownload = await rawDownloadPromise;
    assert.equal(rawDownload.suggestedFilename(), "fort-raw-1.txt");
    const rawText = fs.readFileSync(await rawDownload.path(), "utf8");
    assert(
      rawText.includes("result=echo") &&
        rawText.includes("GPIO17 / RX GPIO18") &&
        rawText.includes("RX (8 captured bytes)"),
    );
    malformedTrace = true;
    await page.waitForFunction(() => document.getElementById("trace-status")?.textContent.includes("Cannot"));
    assert(await page.locator("#trace-start").isDisabled(), "Malformed raw frames must disable capture controls without a render error");
    malformedTrace = false;
    await page.waitForFunction(() => !document.getElementById("trace-status")?.textContent.includes("Cannot"));
    traceFail = true;
    await page.waitForFunction(() =>
      document
        .getElementById("trace-status")
        ?.textContent.includes("Cannot refresh"),
    );
    assert(await page.locator("#trace-start").isDisabled());
    traceFail = false;
    await page.waitForFunction(
      () =>
        document.getElementById("trace-start") &&
        !document.getElementById("trace-start").disabled,
    );
    for (const width of [320, 390, 768, 1440]) {
      await page.setViewportSize({ width, height: 1000 });
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "raw capture overflow at " + width,
      );
    }
    await page.screenshot({
      path: path.join(root, "docs/assets/ui-trace-desktop.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(root, "docs/assets/ui-trace-mobile.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.locator("#raw-trace summary").click();
    // Exercise the profile control against the shipped gzip, including reload.
    await page.locator("#meter-profile").selectOption("acrel-adl400");
    assert.equal(await page.locator("#baud option[value='38400']").count(),0);
    await page.locator("#config button").click();
    await page.waitForFunction(()=>document.querySelector("#scan-probe")?.value==="0064");
    assert.equal(await page.locator("#scan-words").inputValue(),"1");
    await page.reload();await page.waitForFunction(()=>document.querySelector("#meter-profile")?.value==="acrel-adl400");
    assert((await page.locator(".wiring-help").textContent()).includes("21 / A1"));
    assert(calls.some(call=>call.url==="/api/config"&&call.data.get("profile")==="acrel-adl400"));
    await page.locator("#config").screenshot({path:path.join(root,"docs/assets/ui-meter-profile.png"),style:".main-nav{visibility:hidden}"});
    await page.locator("#meter-profile").selectOption("fort");await page.locator("#config button").click();
    await page.waitForFunction(()=>document.querySelector("#scan-probe")?.value==="0058");
    await page.locator("#slave").fill("3");
    await page.locator("#baud").selectOption("4800");
    await page.locator("#parity").selectOption("O");
    await page.locator("#config button").click();
    await page.waitForFunction(
      () => !document.getElementById("scanstart").disabled,
    );
    assert(
      calls.some(
        (call) =>
          call.url === "/api/config" &&
          call.data.get("slave") === "3" &&
          call.data.get("baud") === "4800" &&
          call.data.get("parity") === "O",
      ),
    );
    await page.locator("#direction").selectOption("auto");
    await page.locator("#config button").click();
    await page.waitForFunction(() => !document.querySelector("#config button").disabled);
    await page.reload();await page.waitForFunction(() => document.getElementById("direction")?.value === "auto");
    assert.equal(await page.locator("#direction").inputValue(), "auto");
    await page.locator("#direction").selectOption("manual");await page.locator("#config button").click();
    await page.waitForFunction(() => !document.querySelector("#config button").disabled);
    const scanCalls = () =>
      calls.filter((call) => call.url.startsWith("/api/scan/"));
    await page.locator("#scanfirst").fill("20");
    await page.locator("#scanlast").fill("10");
    await page.locator("#scanstart").click();
    assert(
      (await page.locator("#message").innerText()).includes("Use slave IDs"),
    );
    assert.equal(scanCalls().length, 0);
    await page.locator("#scanfirst").fill("7");
    await page.locator("#scanlast").fill("7");
    await page.locator("#scanstart").click();
    await page.waitForFunction(
      () => !document.getElementById("scancancel").disabled,
    );
    assert.equal(scanCalls()[0].data.get("first"), "7");
    assert.equal(scanCalls()[0].data.get("last"), "7");
    assert.equal(scanCalls()[0].data.get("mode"), "current");
    assert(await page.locator("#slave").isDisabled());
    assert(await page.locator("#scanstart").isDisabled());
    assert(await page.locator("#scanapply").isDisabled());
    assert.equal(
      await page.locator("#scanprogress").getAttribute("value"),
      "1",
    );
    await page.locator("#language").selectOption("id");
    assert(
      (await page.locator("#raw-trace summary").innerText()).includes("mentah"),
    );
    assert(
      (await page.locator("#scanstatus").innerText()).includes(
        "Pencatatan dijeda",
      ),
    );
    await page.reload();
    await page.waitForFunction(
      () =>
        document.getElementById("scancancel") &&
        !document.getElementById("scancancel").disabled,
    );
    assert.equal(await page.locator("html").getAttribute("lang"), "id");
    assert.equal(await page.locator("#scanfirst").inputValue(), "7");
    assert.equal(await page.locator("#scanlast").inputValue(), "7");
    await page.locator("#language").selectOption("en");
    await page.locator("#scancancel").click();
    await page.waitForFunction(() =>
      document.getElementById("scanstatus")?.textContent.includes("cancelled"),
    );
    assert.equal(await page.locator("#slave").inputValue(), "3");
    await page.locator("#scan-mode").selectOption("matrix");
    scanMode = "found";
    await page.locator("#scanstart").click();
    await page.waitForFunction(
      () => !document.getElementById("scanapply").disabled,
    );
    assert.equal(await page.locator("#slave").inputValue(), "3");
    assert((await page.locator("#scanstatus").innerText()).includes("ID 7"));
    await page.locator("#scanapply").click();
    await page.waitForFunction(
      () => document.getElementById("slave")?.value === "7",
    );
    assert.equal(await page.locator("#baud").inputValue(), "19200");
    assert.equal(await page.locator("#parity").inputValue(), "E");
    assert.equal(await page.locator("#direction").inputValue(), "manual");
    assert(await page.locator("#scanapply").isDisabled());
    scanMode = "exhausted";
    await page.locator("#scanstart").click();
    await page.waitForFunction(() =>
      document
        .getElementById("scanstatus")
        ?.textContent.includes("No validated reply"),
    );
    rejectScan = true;
    await page.locator("#scanstart").click();
    await page.waitForFunction(() =>
      document.getElementById("message")?.textContent.includes("queue is busy"),
    );
    rejectScan = false;
    await page.locator("#language").selectOption("id");
    assert((await page.locator("#message").innerText()).includes("Antrean"));
    await page.locator("#language").selectOption("en");
    hangScan = true;
    await page.locator("#scanstart").click();
    await page.waitForFunction(
      () =>
        document
          .getElementById("message")
          ?.textContent.includes("Execution is not confirmed yet"),
      null,
      { timeout: 12000 },
    );
    hangScan = false;
    assert(!(await page.locator("#scanstart").isDisabled()));
    state.scan = {
      ...state.scan,
      phase: "running",
      first: 1,
      last: 247,
      tested: 54,
      total: 4446,
      elapsed: 46,
      slave: 55,
      baud: 9600,
      parity: "N",
    };
    await refresh();
    await page.waitForFunction(
      () =>
        document.getElementById("scanprogress")?.getAttribute("value") === "54",
    );
    if (await page.locator("#dismiss").count())
      await page.locator("#dismiss").click();
    await page.waitForTimeout(400);
    await page.screenshot({
      path: path.join(assets, "ui-scan-desktop.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(assets, "ui-scan-mobile.png"),
      fullPage: true,
    });
    for (const width of [320, 390, 768]) {
      await page.setViewportSize({ width, height: 844 });
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "setup overflow at " + width,
      );
    }
    state.scan.phase = "cancelled";
    await refresh();
    await page.waitForFunction(() =>
      document.getElementById("scanstatus")?.textContent.includes("cancelled"),
    );
    await page.locator("#nav-overview").click();
    await page.waitForTimeout(400);
    await page.setViewportSize({ width: 1440, height: 1050 });
    await page.screenshot({
      path: path.join(assets, "ui-desktop.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({
      path: path.join(assets, "ui-mobile.png"),
      fullPage: true,
    });
    await page.locator("#language").selectOption("id");
    assert.equal(await page.locator("#energy").getAttribute("data-value"), "128.432,728");
    await page.locator("#legacy-intervals").evaluate(node=>node.open=true);
    assert((await page.locator("#intervals").innerText()).includes("Lengkap"));
    await page.screenshot({
      path: path.join(assets, "ui-mobile-id.png"),
      fullPage: true,
    });
    await page.setViewportSize({ width: 1440, height: 1050 });
    await page.screenshot({
      path: path.join(assets, "ui-desktop-id.png"),
      fullPage: true,
    });
    for (const width of [320, 390, 768, 1440, 2560]) {
      await page.setViewportSize({ width, height: 1000 });
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "overview overflow at " + width,
      );
    }
    await page.locator("#language").selectOption("en");
    await page.evaluate(() => {
      window.flowAdded = 0;window.flowRX = 0;
      const svg = document.querySelector(".flow-playback");
      window.flowObserver = new MutationObserver((records) => {
        for (const record of records)
          for (const node of record.addedNodes)
            if (node.nodeType === 1 && node.hasAttribute("data-event"))
              {++window.flowAdded;if(node.classList.contains("rx"))++window.flowRX;}
      });
      window.flowObserver.observe(svg, { childList: true,subtree:true });
    });
    flowAuto = true;
    lastFlowFrame = 0;
    await page.waitForFunction(() => window.flowAdded > 0);
    flowAuto = false;
    await page.waitForFunction(()=>document.querySelector(".flow-playback")?.dataset.phase==="idle");
    const observed = await page.evaluate(() => window.flowAdded);
    await page.waitForTimeout(600);
    assert.equal(
      await page.evaluate(() => window.flowAdded),
      observed,
      "No idle/decorative packet animation without a new device event",
    );
    await page.evaluate(() => {
      window.flowFadeSamples={tx:[],rx:[]};window.flowTravelSamples=[];window.flowWavePeak={tx:0,rx:0};
      let frame=0;
      const sample=()=>{
        const phase=document.querySelector(".flow-playback")?.dataset.phase;
        if(phase==="tx"){
          const train=document.querySelector(".flow-train.tx"),track=document.querySelector(".flow-rail.tx .flow-track");
          if(train&&track){
            const a=train.getBoundingClientRect(),b=track.getBoundingClientRect();
            window.flowTravelSamples.push({at:performance.now(),position:(a.x-b.x)/(b.width-a.width)});
          }
        }
        for(const direction of ["tx","rx"]){
          if(phase===direction&&window.flowWavePeak[direction]===0){
            const train=document.querySelector(`.flow-train.${direction}`),track=document.querySelector(`.flow-rail.${direction} .flow-track`);
            if(train&&track){
              const a=train.getBoundingClientRect(),b=track.getBoundingClientRect();
              const position=(a.x-b.x)/(b.width-a.width);
              if(position>.4&&position<.6)window.flowWavePeak[direction]=Math.max(...[...track.querySelectorAll(".wake-slider__bar")].map(node=>node.getBoundingClientRect().height));
            }
          }
          if(phase!==`${direction}-settle`)continue;
          const train=document.querySelector(`.flow-train.${direction}`);
          const wake=document.querySelector(`.flow-rail.${direction} .flow-wake`);
          if(train&&wake)window.flowFadeSamples[direction].push({
            opacity:Number(getComputedStyle(train).opacity),
            wake:Number(getComputedStyle(wake).opacity),
            x:train.getBoundingClientRect().x
          });
        }
        frame=requestAnimationFrame(sample);
      };
      frame=requestAnimationFrame(sample);
      window.stopFlowFadeProbe=()=>cancelAnimationFrame(frame);
    });
    appendFlow();
    await page.waitForSelector(".flow-train.tx");
    const pointCount=await page.locator(".flow-rail.tx .wake-slider__bar").count();
    assert(pointCount>=64&&pointCount<=256,"The actual WakeSlider uses more, smaller points");
    const dotBounds=await page.locator(".flow-rail.tx .wake-slider__bar").evaluateAll(nodes=>nodes.map(node=>{const box=node.getBoundingClientRect();return{width:box.width,height:box.height};}));
    assert(dotBounds.every(point=>Math.abs(point.width-2)<.1&&point.height<=36.1),"Points retain their narrow width while the moving wake can grow vertically");
    assert.equal(await page.locator(".flow-train.rx").count(),0,"TX and RX replay must alternate");
    await page.waitForSelector(".flow-train.rx");
    await page.waitForFunction(()=>document.querySelector(".flow-playback")?.dataset.phase==="idle");
    const {fadeSamples,travelSamples,wavePeak}=await page.evaluate(()=>{window.stopFlowFadeProbe();return{fadeSamples:window.flowFadeSamples,travelSamples:window.flowTravelSamples,wavePeak:window.flowWavePeak};});
    assert(travelSamples.length>=8&&travelSamples[0].position<.1&&travelSamples.at(-1).position>.9,"The request must visibly travel across the track");
    assert(travelSamples.at(-1).at-travelSamples[0].at>=900,"Rendered travel must remain readable rather than flashing at the endpoint");
    assert(travelSamples.some(row=>row.position>.3&&row.position<.7),"The replay must paint intermediate positions");
    for(const direction of ["tx","rx"]){
      assert(wavePeak[direction]>=24&&wavePeak[direction]<=36.1,`${direction}: the moving sound wave must visibly rise within its 36 px envelope`);
      const samples=fadeSamples[direction];
      assert(samples.length>=4,`${direction}: arrival must remain mounted over multiple rendering frames`);
      assert(samples[0].opacity>.9&&samples.at(-1).opacity<.1,`${direction}: fade must span from opaque to nearly transparent`);
      assert(samples.some(row=>row.opacity>.2&&row.opacity<.8),`${direction}: opacity must pass through intermediate values`);
      for(let i=1;i<samples.length;++i)assert(samples[i].opacity<=samples[i-1].opacity,`${direction}: fade cannot brighten again`);
      assert(Math.max(...samples.map(row=>row.x))-Math.min(...samples.map(row=>row.x))<1,"Fade must stay at the arrived endpoint rather than resetting to the start");
      assert(samples.every(row=>Math.abs(row.wake-row.opacity)<.001),"The wake must decay with the packet rather than flatten immediately");
    }
    assert(await page.locator(".flow-equipment-grid svg").count()===3);
    assert.equal(await page.locator(".flow-tap,.flow-lane").count(),0,"No intersecting SVG transport lines remain");
    assert(await page.locator("#clock .counter").count()===3,"Clock units use the supplied Counter component");
    assert.equal(await page.locator(".main-nav button").count(),3);
    assert(await page.locator(".main-nav .star-border__light").count()===3,"Navigation uses supplied StarBorder components");
    assert(await page.locator('.nav-tab[aria-current="page"]').evaluate(node=>getComputedStyle(node,"::after").content==="none"),"The old blue underline must be absent");
    assert(await page.locator(".flow-symbol path").count()>=4,"Device symbols must be drawn as vectors");
    assert((await page.locator("#flow-direction").innerText()).includes("/RE = 0"));
    const rxBeforeTimeout=await page.evaluate(()=>window.flowRX);
    appendFlow("timeout");
    await page.waitForFunction(()=>document.getElementById("flow-result")?.textContent.includes("Timeout"));
    assert((await page.locator(".flow-frames").innerText()).includes("No bytes received"));
    await page.waitForSelector(".flow-train.tx");
    await page.waitForFunction(()=>document.querySelector(".flow-playback")?.dataset.phase==="idle");
    assert.equal(await page.evaluate(()=>window.flowRX),rxBeforeTimeout,"Timeout cannot fabricate an RX frame");
    appendFlow("crc");
    await page.waitForFunction(()=>document.getElementById("flow-result")?.textContent.includes("CRC mismatch"));
    malformedFlow = true;
    await page.waitForFunction(() => document.getElementById("flow-phase")?.textContent.includes("unavailable"));
    assert(await page.locator("#nav-setup").isVisible(), "Malformed flow rows must not crash the dashboard");
    malformedFlow = false;
    await page.waitForFunction(() => !document.getElementById("flow-phase")?.textContent.includes("unavailable"));
    const beforeReboot=await page.evaluate(()=>window.flowAdded);
    flowBoot++;
    flowSequence = 0;
    flowEvents = [];
    appendFlow();
    await page.waitForTimeout(900);
    assert.equal(
      await page.evaluate(() => window.flowAdded),
      beforeReboot,
      "Reboot history must not animate as newly observed packets",
    );
    flowAuto = true;
    await page.locator("#nav-setup").click();
    const beforeDiagnostic = diagnosticRequests;
    await page.waitForTimeout(1100);
    assert.equal(
      diagnosticRequests,
      beforeDiagnostic,
      "Closed diagnostic panel must not fetch logs",
    );
    diagnosticHold = true;
    await page.locator("#diagnostic-console > summary").click();
    await page.waitForSelector("#diagnostic-command");
    assert(await page.locator("#diagnostic-send").isDisabled(), "Do not run diagnostics before loading current UART-owner state");
    diagnosticHold = false;
    await page.waitForFunction(() => !document.getElementById("diagnostic-send").disabled, null, { timeout: 12000 });
    await page.locator("#diagnostic-command").evaluate((node) => {
      const clipboardData = new DataTransfer();
      clipboardData.setData("text", "x\nstatus");
      node.dispatchEvent(
        new ClipboardEvent("paste", {
          clipboardData,
          bubbles: true,
          cancelable: true,
        }),
      );
    });
    assert.equal(
      await page.locator("#diagnostic-command").inputValue(),
      "diagnose",
    );
    await page.locator("#diagnostic-command").fill("status");await page.locator("#diagnostic-send").click();
    await page.waitForFunction(() => document.getElementById("diagnostic-output")?.textContent.includes("[COMMAND_INPUT] status"));
    assert.equal(diagnostic.active,false,"Read-only session inspection must not pause recording");
    await page.locator("#diagnostic-tools > summary").click();
    await page.locator("#diagnostic-setting").selectOption("gap");await page.locator("#diagnostic-setting-value").fill("350");await page.locator("#diagnostic-setting-apply").click();
    await page.waitForFunction(() => document.getElementById("diagnostic-settings")?.textContent.includes("gap 350 ms"));
    await page.locator("#diagnostic-address").fill("008A");await page.locator("#diagnostic-read").click();
    await page.waitForFunction(() => document.getElementById("diagnostic-output")?.textContent.includes("reg 008A 2"));
    assert(calls.some(c=>c.url==="/api/diagnostic" && c.data.get("command")==="reg 008A 2"));
    await page.locator("#diagnostic-command").fill("primary");await page.locator("#diagnostic-send").click();
    await page.waitForFunction(() => document.getElementById("diagnostic-output")?.textContent.includes("[COMMAND_INPUT] primary"));
    malformedDiagnostic=true;await page.waitForFunction(() => document.getElementById("diagnostic-send")?.disabled);
    assert(await page.locator("#diagnostic-stop").isEnabled());malformedDiagnostic=false;
    await page.waitForFunction(() => !document.getElementById("diagnostic-send")?.disabled);
    await page.locator("#diagnostic-tools > summary").click();
    await page.locator("#diagnostic-command").fill("diagnose");
    await page.locator("#diagnostic-send").click();
    await page.waitForFunction(() =>
      document
        .getElementById("diagnostic-output")
        ?.textContent.includes("REPORT_END"),
    );
    await refresh();
    assert(await page.locator("#scanstart").isDisabled());
    assert(await page.locator("#slave").isDisabled());
    assert((await page.locator("#health").innerText()).includes("Maintenance"));
    await page.locator("#nav-device").click();
    assert(await page.locator("#sync").isDisabled(), "RTC sync must stay blocked during maintenance");
    assert(await page.locator("#network-save").isDisabled(), "Network changes must stay blocked during maintenance");
    delayedControls = 2000;
    await page.locator("#mute").click();
    await page.waitForTimeout(100);
    assert(await page.locator("#mute").isDisabled());
    await page.locator("#nav-setup").click();
    await page.locator("#diagnostic-console > summary").click();
    await page.waitForFunction(() => !document.getElementById("diagnostic-stop")?.disabled);
    const beforePriority = calls.length;
    await page.locator("#diagnostic-stop").click();
    await page.waitForFunction(() => !document.getElementById("diagnostic-stop")?.disabled);
    assert(calls.slice(beforePriority).some((c) => c.url === "/api/diagnostic" && c.data.get("command") === "x"), "Priority stop must be sent while ordinary control is awaiting completion");
    await page.waitForTimeout(2200);
    delayedControls = 0;
    await page.locator("#nav-device").click();

    await page.locator("#nav-setup").click();
    await page.locator("#diagnostic-console > summary").click();
    await page.waitForFunction(() => !document.getElementById("diagnostic-resume")?.disabled);
    const commandCall = calls.find(
      (c) =>
        c.url === "/api/diagnostic" && c.data.get("command") === "diagnose",
    );
    assert(commandCall);
    const downloadPromise = page.waitForEvent("download");
    await page.locator("#diagnostic-download").click();
    const logDownload = await downloadPromise;
    assert(logDownload.suggestedFilename().startsWith("fort-diagnostic-"));
    await page.locator("#language").selectOption("id");
    assert(
      (await page.locator("#diagnostic-resume").innerText()).includes(
        "Lanjutkan",
      ),
    );
    await page.setViewportSize({ width: 390, height: 2400 });
    await page
      .locator("#diagnostic-console")
      .screenshot({ path: path.join(assets, "ui-diagnostic-mobile.png") });
    await page.locator("#diagnostic-resume").click();
    await page.waitForFunction(() =>
      document
        .getElementById("diagnostic-status")
        ?.textContent.includes("produksi"),
    );
    await refresh();
    assert(!(await page.locator("#scanstart").isDisabled()));
    await page.locator("#diagnostic-console > summary").click();
    await page.waitForTimeout(100);
    const closedDiagnostic = diagnosticRequests;
    await page.waitForTimeout(1100);
    assert.equal(diagnosticRequests, closedDiagnostic);
    await page.locator("#nav-device").click();
    const beforeSystem = systemRequests;
    await page.waitForTimeout(1600);
    assert.equal(
      systemRequests,
      beforeSystem,
      "Closed process monitor must not poll",
    );
    await page.locator("#process-monitor summary").click();
    await page.waitForSelector("#process-monitor tbody tr");
    assert.equal(await page.locator("#process-monitor tbody tr").count(), 4);
    malformedSystem = true;
    await page.locator("#process-monitor [role=status].warning").waitFor();
    assert.equal(await page.locator("#process-monitor tbody tr").count(), 4, "Malformed process metrics must retain the previous table");
    malformedSystem = false;
    await page.locator("#process-monitor [role=status].warning").waitFor({ state: "hidden" });

    assert(
      (await page.locator("#process-monitor").innerText()).includes("17.4%"),
    );
    await page.setViewportSize({ width: 1440, height: 1050 });
    await page
      .locator("#process-monitor")
      .screenshot({ path: path.join(assets, "ui-process-desktop.png") });
    await page
      .locator("#uart-flow")
      .screenshot({ path: path.join(assets, "ui-flow-desktop.png"),style:".main-nav{visibility:hidden}" });
    for (const width of [320, 390, 768, 1440, 2560]) {
      await page.setViewportSize({ width, height: 1000 });
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "instrumentation overflow at " + width,
      );
    }
    await page.locator("#nav-overview").click();
    await page.locator("#language").selectOption("id");
    online = false;
    await refresh();
    await page.waitForFunction(() =>
      document.getElementById("connection")?.textContent.includes("offline"),
    );
    assert.equal(await page.locator("#power").textContent(), "—");
    fail = true;
    await refresh();
    await page.waitForFunction(() =>
      document.getElementById("connection")?.textContent.includes("terputus"),
    );
    assert(
      (await page.locator("#health").innerText()).includes(
        "bukan pembacaan langsung",
      ),
    );
    await page.locator("#nav-setup").click();
    assert(await page.locator("#scanstart").isDisabled());
    assert(await page.locator("#scanapply").isDisabled());
    await page.locator("#raw-trace summary").click();
    assert(await page.locator("#trace-start").isDisabled());
    await page.locator("#nav-device").click();
    assert(await page.locator("#led-save").isDisabled());
    assert((await page.locator("#led-title").innerText()).includes("bawaan"));
    fail = false;
    online = true;
    const reduced = await browser.newPage({
      viewport: { width: 390, height: 844 },
      reducedMotion: "reduce",
    });
    reduced.on("pageerror", (error) => errors.push(error.message));
    await reduced.addInitScript(() => {
      Object.defineProperty(window, "localStorage", {
        get() {
          throw new Error("Storage blocked");
        },
      });
    });
    await reduced.goto(origin);
    await reduced.waitForSelector("#energy");
    assert.equal(await reduced.locator("html").getAttribute("lang"), "en");
    assert.equal(
      await reduced
        .locator(".energy-surface")
        .evaluate((node) => getComputedStyle(node).visibility),
      "visible",
    );
    await reduced.close();
    assert.deepEqual(external, []);
    assert.deepEqual(errors, []);
    assert(
      assetRequests.has("/assets/app.js") &&
        assetRequests.has("/assets/app.css"),
    );
    await page.close();
    const vectors = await checkVectors(browser, out);
    console.log(
      `PASS: real gzip assets + upstream React Bits hashes; EN/ID persistence and errors; event-driven UART motion/no idle animation; diagnostic commands/export/pause/resume; process metrics/closed polling; raw TX/RX capture/echo/export/closed polling/errors; LED modes/POST/draft/reload/storage/disabled; scan/config/time/CSV; screensaver save/reload/disable/schema handling; automatic SSID scan/picker, custom AP IPv4 and smooth clock without RTC writes; AP/client/dual settings, credentials privacy, fallback and malformed network state; structured diagnostics and session settings; stale/offline; 320–2560px; reduced motion; no external requests/JS errors; ${vectors} SVGs`,
    );
  } catch (error) {
    const failed = browser?.contexts()[0]?.pages()[0];
    if (failed) {
      await failed.screenshot({
        path: path.join(out, "ui-failure.png"),
        fullPage: true,
      });
      fs.writeFileSync(
        path.join(out, "ui-failure.txt"),
        await failed.locator("body").innerText(),
      );
    }
    throw error;
  } finally {
    if (browser) await browser.close();
    await new Promise((resolve) => server.close(resolve));
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
