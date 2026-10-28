const assert=require("node:assert/strict");
const fs=require("node:fs"),path=require("node:path");
module.exports=async function testFlow(){
  const source=fs.readFileSync(path.join(__dirname,"../web/flow-playback.js"),"utf8");
  const {FrameAssembler,FLOW_MOVE_MS,FLOW_GAP_MS,FLOW_SETTLE_MS,easeFlow,flowLeg,nextFlowPhase}=await import("data:text/javascript;base64,"+Buffer.from(source).toString("base64"));
  assert(FLOW_MOVE_MS>=900&&FLOW_MOVE_MS<2000);assert(FLOW_GAP_MS>0);
  assert(FLOW_SETTLE_MS>=300);
  assert.equal(easeFlow(0),0);assert.equal(easeFlow(1),1);
  assert(easeFlow(.1)<.1&&easeFlow(.9)>.9);
  assert(easeFlow(.001)<1e-7&&1-easeFlow(.999)<1e-7,"Endpoint velocity must settle to zero");
  let previous=-1;
  for(let i=0;i<=100;++i){const value=easeFlow(i/100);assert(value>=previous&&value>=0&&value<=1);previous=value;}
  for(const direction of ["tx","rx"]){
    assert.deepEqual(flowLeg(direction,1,direction),flowLeg(`${direction}-settle`,0,direction),"Arrival must preserve position and opacity");
    const middle=flowLeg(`${direction}-settle`,.5,direction);assert.equal(middle.position,1);assert.equal(middle.opacity,.5);assert(middle.visible);
    const end=flowLeg(`${direction}-settle`,1,direction);assert.equal(end.opacity,0);assert.equal(end.position,1);
    assert.equal(flowLeg(direction,.01,direction).opacity<.01,true,"Entry must fade from zero");
    assert.equal(nextFlowPhase(direction,true),`${direction}-settle`);
  }
  assert.equal(nextFlowPhase("gap",false),"hold");assert.equal(nextFlowPhase("gap",true),"rx");
  console.log("PASS: easing is monotonic with settled endpoints, continuous arrival and gradual packet/wake fade");
  const a=new FrameAssembler();
  const event=(seq,kind,extra={})=>({seq,kind,atUs:seq*100,slave:1,baud:9600,parity:"N",address:0x58,words:2,rxPin:18,txPin:17,dirPin:16,diagnostic:false,bytes:kind<3?"01 03 00 58 00 02 45 D8":"01 03 04 00 00 C3 17 EA CD",result:kind===4?"valid":"invalid",...extra});
  for(const row of [event(1,1),event(2,2),event(3,3)])assert.equal(a.accept(row),null);
  const complete=a.accept(event(4,4));assert(complete.tx&&complete.rx&&complete.firstRX);assert.equal(complete.request.seq,1);
  a.accept(event(5,1));a.accept(event(6,2));const timeout=a.accept(event(7,6,{bytes:"",result:"timeout"}));assert(timeout.tx&&!timeout.rx);
  a.accept(event(8,1));a.accept(event(9,2));assert.equal(a.accept(event(11,4)),null,"Sequence gaps must not join frames");
  a.accept(event(12,1));assert.equal(a.accept(event(13,2,{dirPin:15})),null);assert.equal(a.accept(event(14,4)),null);
  assert.equal(a.accept(event(15,4)),null,"A response without an observed request cannot produce motion");
  a.reset();a.accept(event(0xfffffffd,1));a.accept(event(0xfffffffe,2));a.accept(event(0xffffffff,3));assert(a.accept(event(0,4)).rx);
  a.reset();a.accept(event(20,1));a.accept(event(21,2));const crc=a.accept(event(22,4,{result:"crc"}));assert.equal(crc.rx.result,"crc");
  a.reset();assert.equal(a.accept(event(23,6,{bytes:""})),null);
  a.accept(event(24,1));a.accept(event(25,2));const local=a.accept(event(26,8,{bytes:""}));assert(local.tx&&!local.rx);
  console.log("PASS: observed-frame assembler handles valid/CRC/timeout, gaps, settings changes, orphan replies, reset and sequence rollover");
};
if(require.main===module)module.exports().catch(error=>{console.error(error);process.exitCode=1;});
