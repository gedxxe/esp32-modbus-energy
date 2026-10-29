const object=v=>v!==null&&typeof v==="object";
const uint=(v,max=0xffffffff)=>Number.isInteger(v)&&v>=0&&v<=max;
const hex=v=>typeof v==="string"&&/^(?:[0-9A-F]{2}(?: [0-9A-F]{2})*)?$/.test(v)&&v.length<=47;
const probe=p=>object(p)&&uint(p.address,65535)&&uint(p.words,3)&&p.words>0&&uint(p.attempts,2)&&typeof p.confirmed==="boolean"&&typeof p.data==="boolean"&&hex(p.tx)&&Array.isArray(p.replies)&&p.replies.length===p.attempts&&p.replies.every(r=>object(r)&&uint(r.status,255)&&uint(r.attemptMs,65535)&&hex(r.tx)&&uint(r.txBytes,8)&&r.tx.length===(r.txBytes?r.txBytes*3-1:0)&&typeof r.accepted==="boolean"&&typeof r.truncated==="boolean"&&hex(r.rx));
const evidence=d=>object(d)&&uint(d.slave,247)&&d.slave>0&&uint(d.baud,1000000)&&d.baud>=1200&&["N","E","O"].includes(d.parity)&&[1,2].includes(d.stopBits)&&["manual","auto"].includes(d.direction)&&uint(d.run)&&uint(d.boot)&&uint(d.utc)&&typeof d.confirmed==="boolean"&&uint(d.mapEvidence,3)&&["requests","timeouts","crcErrors","rejected","exceptions","elapsedMs"].every(k=>uint(d[k]))&&Array.isArray(d.probes)&&d.probes.length<=8&&d.probes.every(probe);
export function validMeterLab(v){
  return object(v)&&v.schema===1&&typeof v.ready==="boolean"&&typeof v.storageOK==="boolean"&&uint(v.activeId,65535)&&typeof v.error==="string"&&object(v.discovery)&&["idle","running","complete","cancelled","capacity"].includes(v.discovery.phase)&&["run","tested","total","elapsedMs","requests","timeouts","crcErrors","rejected","exceptions"].every(k=>uint(v.discovery[k]))&&v.discovery.tested<=v.discovery.total&&Array.isArray(v.discovery.plan)&&v.discovery.plan.length>=1&&v.discovery.plan.length<=8&&v.discovery.plan.every(p=>object(p)&&uint(p.address,65535)&&uint(p.words,3)&&p.words>0&&p.address+p.words<=65536)&&new Set(v.discovery.plan.map(p=>`${p.address}:${p.words}`)).size===v.discovery.plan.length&&evidence(v.current)&&Array.isArray(v.findings)&&v.findings.length<=4&&v.findings.every((d,i)=>d.index===i&&evidence(d)&&d.confirmed)&&Array.isArray(v.saved)&&v.saved.length<=4&&new Set(v.saved.map(m=>m?.id)).size===v.saved.length&&v.saved.every(m=>object(m)&&uint(m.id,65535)&&m.id>0&&typeof m.name==="string"&&m.name.length>0&&m.name.length<=31&&typeof m.model==="string"&&m.model.length>0&&m.model.length<=39&&typeof m.notes==="string"&&m.notes.length<=63&&typeof m.profile==="string"&&typeof m.verified==="boolean"&&typeof m.activatable==="boolean"&&evidence(m.evidence)&&m.evidence.confirmed);
}
export function parseProbePlan(text){
  if(typeof text!=="string"||text.length>63)return null;
  const seen=new Set(),plan=[];
  for(const token of text.split(",")){const match=/^([0-9a-fA-F]{1,4}):([1-3])$/.exec(token);if(!match)return null;const address=parseInt(match[1],16),words=Number(match[2]);if(address+words>65536||seen.has(`${address}:${words}`))return null;seen.add(`${address}:${words}`);plan.push({address,words});}
  return plan.length>0&&plan.length<=8?plan:null;
}
export function wordCandidates(probe){
  if(!probe.data)return null;const raw=probe.replies[probe.replies.length-1]?.rx.split(" ").map(b=>parseInt(b,16));
  if(!raw||raw.length!==5+2*probe.words)return null;
  const words=Array.from({length:probe.words},(_,i)=>(raw[3+2*i]<<8)|raw[4+2*i]);
  if(words.length<2)return {words};
  const unsigned=(words[0]*65536+words[1]),swapped=words[1]*65536+words[0];
  const buffer=new ArrayBuffer(4),view=new DataView(buffer);view.setUint32(0,unsigned);const normal=view.getFloat32(0);view.setUint32(0,swapped);const swap=view.getFloat32(0);
  return {words,unsigned,swapped,signed:unsigned>=2147483648?unsigned-4294967296:unsigned,float:Number.isFinite(normal)?normal:null,floatSwapped:Number.isFinite(swap)?swap:null};
}
