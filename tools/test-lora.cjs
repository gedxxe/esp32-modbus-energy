const assert=require("node:assert/strict"),path=require("node:path");
async function checkLayout(page) {
 const problems=await page.locator('#lora-panel').evaluate(panel=>{
  const errors=[],outer=panel.getBoundingClientRect();
  const controls=[...panel.querySelectorAll('input,select,button,summary')].filter(e=>e.getClientRects().length);
  const boxes=controls.map(e=>({e,b:e.getBoundingClientRect(),name:e.id||e.textContent.trim()}));
  for(const {e,b,name} of boxes){
   if(b.left<outer.left-1||b.right>outer.right+1)errors.push(`outside panel: ${name}`);
   if(e.tagName==='BUTTON'&&(b.height<43.5||e.scrollWidth>e.clientWidth+1))errors.push(`clipped/small button: ${name}`);
  }
  for(let i=0;i<boxes.length;i++)for(let j=i+1;j<boxes.length;j++){
   const a=boxes[i],b=boxes[j];
   if(Math.min(a.b.right,b.b.right)-Math.max(a.b.left,b.b.left)>1&&Math.min(a.b.bottom,b.b.bottom)-Math.max(a.b.top,b.b.top)>1)errors.push(`overlap: ${a.name} / ${b.name}`);
  }
  for(const label of panel.querySelectorAll('.lora-field')){
   if(!label.getClientRects().length)continue;
   const text=label.querySelector('span')?.getBoundingClientRect(),input=label.querySelector('input,select')?.getBoundingClientRect();
   if(text&&input&&text.bottom>input.top-5)errors.push('label touches input: '+label.htmlFor);
  }
  return errors;
 });
 assert.deepEqual(problems,[],`LoRa control layout at ${page.viewportSize().width}px`);
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Horizontal overflow');
}
module.exports=async({page,origin,state,calls,root})=>{
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(origin+"/#device");await page.waitForFunction(()=>document.querySelector("#lora-save")&&!document.querySelector("#lora-save").disabled);
 assert.equal(await page.locator("#lora-frequency").inputValue(),"915");assert(!(await page.locator("#lora-enabled").isChecked()));
 assert((await page.locator("#lora-ptmp-note").innerText()).includes("stations 2, 3 and 4"));
 assert((await page.locator("body").innerText()).includes("Checkpoints restored from staging"));
 await page.locator('#lora-advanced > summary').click();await page.locator('#lora-ack').fill('');await page.locator('#lora-advanced > summary').click();
 const beforeInvalid=calls.filter(x=>x.url==='/api/lora/config').length;
 await page.locator('#lora-save').click();await page.waitForFunction(()=>document.querySelector('#lora-advanced').open&&document.activeElement?.id==='lora-ack');
 assert((await page.locator('#lora-feedback').innerText()).includes('ACK timeout'));assert.equal(calls.filter(x=>x.url==='/api/lora/config').length,beforeInvalid);
 await page.locator('#lora-ack').fill('10');
 for(const language of ['en','id']){
  await page.locator('#language').selectOption(language);
  for(const width of [320,360,390,768,1024,1440,2560]){
   await page.setViewportSize({width,height:1000});await checkLayout(page);
   if((language==='en'&&width===1440)||(language==='id'&&width===390))await page.locator('#lora-panel').screenshot({path:path.join(root,`docs/assets/ui-lora-advanced-${width===1440?'desktop':'mobile'}.png`),style:'.main-nav{visibility:hidden}'});
  }
 }
 await page.locator('#language').selectOption('en');await page.setViewportSize({width:1440,height:1100});await page.locator('#lora-advanced > summary').click();
 await page.locator("#lora-enabled").check();assert.equal(await page.locator("#lora-blocking-key").count(),1,"Missing shared key must block enable");
 await page.locator("#lora-nodeId").fill("1");await page.locator("#lora-generate").click();assert.equal(await page.locator("#lora-blocking-key").count(),0);assert.equal(await page.locator("#lora-blocking-ids").count(),1);
 const key=await page.locator("#lora-key").inputValue();assert(/^[0-9a-f]{64}$/.test(key));
 const before=calls.filter(x=>x.url==="/api/lora/config").length;await page.locator("#lora-save").click();assert.equal(calls.filter(x=>x.url==="/api/lora/config").length,before);
 await page.locator("#lora-nodeId").fill("41");await page.locator("#lora-networkId").fill("7");await page.locator("#lora-save").click();await page.waitForSelector("#lora-keep-key:checked");
 let command=calls.filter(x=>x.url==="/api/lora/config").at(-1).data;assert.equal(command.get("key"),key);assert.equal(command.get("frequencyHz"),"915000000");assert.equal(command.get("nodeId"),"41");assert(!JSON.stringify(state).includes(key));assert.equal(await page.locator("#lora-key").count(),0);
 await page.reload();await page.waitForSelector("#lora-keep-key:checked");assert.equal(await page.locator("#lora-nodeId").inputValue(),"41");assert.equal(await page.locator("#lora-blocking-key").count(),0,"Saved shared key must not require regeneration");
 await page.locator("#lora-frequency").fill("921.4");
 assert(await page.locator("#lora-pair").isDisabled(),"Pair must not use saved RF settings while the form displays an unsaved draft");
 await page.locator("#lora-save").click();await page.waitForFunction(()=>!document.querySelector("#lora-save").disabled);
 command=calls.filter(x=>x.url==="/api/lora/config").at(-1).data;assert.equal(command.get("frequencyHz"),"921400000");assert.equal(command.get("keepKey"),"1");assert.equal(command.get("key"),"");
 await page.locator("#lora-pair").click();await page.waitForFunction(()=>document.querySelector("#lora-status")?.innerText.includes("ACK"));assert(!state.lora.paired,"Queuing HELLO is not evidence of pairing");
 Object.assign(state.lora,{paired:true,phase:2,rx:1,acks:1,lastRxAgeMs:350,rssi:-82,snr:8.5});await page.waitForFunction(()=>document.querySelector("#lora-panel")?.innerText.includes("Session paired"));
 await page.locator("#lora-panel").screenshot({path:path.join(root,"docs/assets/ui-lora-desktop.png"),style:".main-nav{visibility:hidden}"});
 await page.locator("#language").selectOption("id");assert((await page.locator("#lora-panel").innerText()).includes("Sesi sudah dipasangkan"));
 assert((await page.locator("#lora-ptmp-note").innerText()).includes("station 2, 3 dan 4"));
 await page.setViewportSize({width:390,height:1000});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.locator("#lora-panel").screenshot({path:path.join(root,"docs/assets/ui-lora-mobile.png"),style:".main-nav{visibility:hidden}"});
 state.network.enterpriseProvisioning=false;await page.waitForTimeout(2300);await page.locator("#lora-keep-key").uncheck();await page.locator("#lora-generate").click();const beforeKey=calls.filter(x=>x.url==="/api/lora/config").length;await page.locator("#lora-save").click();await page.waitForSelector("#lora-keep-key:checked");assert.equal(calls.filter(x=>x.url==="/api/lora/config").length,beforeKey+1,"LoRa keys must save through the client interface independently of Enterprise credential policy");
 Object.assign(state.lora,{available:false,paired:false,phase:6,code:-2});await page.waitForSelector("#lora-error");assert(await page.locator("#lora-pair").isDisabled());assert(!(await page.locator("#lora-save").isDisabled()),"An absent module must not lock configuration");
 state.lora.frequencyHz="invalid";await page.waitForFunction(()=>document.querySelector("#lora-save")?.disabled);
 assert(!(await page.evaluate(()=>JSON.stringify({...localStorage}))).includes(key));
 assert.deepEqual(errors,[],'LoRa UI JavaScript errors');
 console.log("PASS: shipped LoRa 915 default/off, editable RF/IDs, key generation/redaction/reuse, duplicate-ID guard, explicit unconfirmed pairing, simulated ACK status, AP/client key installation, absent/malformed radio handling, recovery status, EN/ID and mobile layout");
};
