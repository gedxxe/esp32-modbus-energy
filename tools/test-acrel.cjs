const assert=require('node:assert/strict'),path=require('node:path');
module.exports=async({page,origin,state,calls,root})=>{
 await page.goto(origin+'/#setup');await page.waitForSelector('#meter-profile');assert.equal(await page.locator('#meter-profile option').count(),6);
 let savedCount=0;
 for(const [key,probe,plan] of [['acrel-acr','0064','0064:1,0047:2'],['acrel-apm','00FC','00FC:1,0BB8:2']]){
  await page.locator('#meter-profile').selectOption(key);await page.locator('#config button').last().click();await page.waitForFunction(value=>document.querySelector('#scan-probe')?.value.toUpperCase()===value,probe);
  assert.equal(state.profileReady,false);await page.locator('#nav-overview').click();await page.waitForSelector('#commissioning-note');assert((await page.locator('#commissioning-note').innerText()).includes('Before recording'));
  await page.locator('#nav-setup').click();await page.locator('#meter-lab > summary').click();await page.waitForSelector('#lab-start:not([disabled])');assert.equal((await page.locator('#lab-plan').inputValue()).toUpperCase(),plan);
  await page.locator('#lab-start').click();await page.waitForSelector('#lab-findings .lab-save');await page.waitForFunction(slave=>document.querySelector('#lab-findings .lab-device')?.textContent.includes(`ID ${slave} `),key==='acrel-acr'?4:5);const finding=page.locator('#lab-findings .lab-device').first();
  await finding.getByLabel('Installation name').fill(key+' panel');await finding.getByLabel('Operator supplied model').fill(key);await finding.getByLabel('Production profile binding').selectOption(key);await finding.locator('input[type="checkbox"]').check();await finding.getByRole('button',{name:'Save finding to flash'}).click();await page.waitForFunction(count=>document.querySelectorAll('#lab-saved .lab-device').length===count,++savedCount).catch(async error=>{console.error('Saved-device failure',key,'accepted saves',calls.filter(c=>c.url==='/api/meter-lab'&&c.data.get('action')==='save').length);console.error(await page.locator('.lab-save').evaluateAll(forms=>forms.map(f=>({valid:f.checkValidity(),invalid:[...f.querySelectorAll(':invalid')].map(e=>e.getAttribute('aria-label')||e.tagName)}))));await page.screenshot({path:path.join(root,'output/previews/pm5100-test-failure.png'),fullPage:true});throw error;});
  const saved=page.locator('#lab-saved .lab-device').last();await saved.getByRole('button',{name:'Use for measurement'}).click();await page.waitForFunction(value=>document.querySelector('#slave')?.value===value,key==='acrel-acr'?'4':'5');
  assert.equal(state.profileReady,true);Object.assign(state,{haveEnergy:true,energy:key==='acrel-acr'?1234.5:589,meterEnergy:key==='acrel-acr'?1234.5:589,secondaryEnergy:key==='acrel-acr'?1234.5:589,power:1.1,powerFactor:.95,frequency:50,reactiveEnergy:22.5,readingQuality:{required:15,transportFailed:0,invalid:0,complete:true}});
  await page.locator('#nav-overview').click();await page.waitForFunction(()=>document.querySelector('#commissioning-note')?.textContent.includes('Recording uses'));await page.waitForSelector('#value-reactiveEnergy');
  await page.locator('#language').selectOption('id');assert((await page.locator('#commissioning-note').innerText()).includes('Pencatatan memakai'));await page.setViewportSize({width:390,height:1000});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.locator('#language').selectOption('en');await page.setViewportSize({width:1440,height:1100});
  await page.reload();await page.waitForSelector('#commissioning-note');await page.screenshot({path:path.join(root,'output/previews/'+key+'.png'),fullPage:true});await page.locator('#nav-setup').click();await page.waitForFunction(value=>document.querySelector('#meter-profile')?.value===value,key);assert.equal(await page.locator('#meter-profile').inputValue(),key);
 }
 assert(calls.some(c=>c.url==='/api/config'&&c.data.get('profile')==='acrel-apm'));
 await page.locator('#meter-profile').selectOption('schneider-pm5100');
 await page.locator('#config button').last().click();
 await page.waitForFunction(()=>document.querySelector('#scan-probe')?.value.toUpperCase()==='0A8B');
 assert.equal(state.profile,'schneider-pm5100');assert.equal(state.profileReady,true);
 await page.locator('#meter-lab > summary').click();
 await page.waitForSelector('#lab-start:not([disabled])');await page.waitForFunction(()=>document.querySelector('#lab-plan')?.value.toUpperCase()==='0A8B:2');
 Object.assign(state,{haveEnergy:true,energy:7838.5,meterEnergy:7838.5,secondaryEnergy:7838.5,power:2.5,powerFactor:.8,frequency:50,reactiveEnergy:12.5,readingQuality:{required:4,transportFailed:0,invalid:0,complete:true}});
 await page.locator('#nav-overview').click();await page.waitForSelector('#value-reactiveEnergy');
 await page.locator('#language').selectOption('id');await page.setViewportSize({width:390,height:1000});
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 await page.locator('#language').selectOption('en');await page.reload();await page.locator('#nav-setup').click();
 await page.waitForFunction(()=>document.querySelector('#meter-profile')?.value==='schneider-pm5100');
 assert(calls.some(c=>c.url==='/api/config'&&c.data.get('profile')==='schneider-pm5100'));
 await page.screenshot({path:path.join(root,'output/previews/schneider-pm5100.png'),fullPage:true});
 console.log('PASS: generated-byte PM5100 and ACR/APM selection, model-specific plans, commissioning notice, named verification/activation, live fields, EN/ID, reload and mobile');
};
