const assert=require('node:assert/strict');
module.exports=async({page,origin,state,calls})=>{
 await page.goto(origin+'/#setup');await page.waitForSelector('#meter-profile');
 const names=await page.locator('#meter-profile option').allTextContents();assert.equal(names.filter(n=>n.includes('ADL400')).length,1);assert.equal(names.length,5);
 await page.locator('#meter-profile').selectOption('acrel-adl400');await page.locator('#slave').fill('220');await page.locator('#config button').last().click();
 await page.waitForFunction(()=>document.querySelector('#slave')?.value==='220');
 Object.assign(state,{energy:7838.4,secondaryEnergy:97.98,meterEnergy:97.98,ratio:80,ratios:true,currentA:100,voltageA:235.54956,frequency:49.96,powerFactor:.95,power:1.234});
 await page.locator('#nav-overview').click();await page.waitForFunction(()=>document.querySelector('#energy')?.getAttribute('data-value')?.includes('7,838.400'));
 assert((await page.locator('#value-secondaryEnergy').innerText()).includes('97.980'));assert((await page.locator('#value-voltageA').innerText()).includes('235.550'));assert((await page.locator('#value-voltageA').locator('..').innerText()).includes('5300'));
 assert((await page.locator('#value-frequency').innerText()).includes('49.96'));
 await page.locator('#nav-setup').click();await page.locator('#meter-lab > summary').click();await page.waitForSelector('#lab-plan');assert.equal((await page.locator('#lab-plan').inputValue()).toUpperCase(),'0064:1,0000:2,008D:2');
 await page.locator('#meter-lab > summary').click();
 const details=page.locator('details').filter({has:page.locator('#diagnostic-command')});
 // The console is mounted only after expanding its parent panel.
 if(!await page.locator('#diagnostic-command').count())await page.locator('#diagnostic-console > summary').click();
 await page.getByRole('button',{name:'ADL400 energy comparison',exact:true}).click();assert.equal(await page.locator('#diagnostic-command').inputValue(),'adl400');await page.locator('#diagnostic-send').click();await page.waitForFunction(()=>document.querySelector('#diagnostic-output')?.textContent.includes('7838.400000'));
 assert(calls.some(c=>c.url==='/api/config'&&c.data.get('slave')==='220'));assert(calls.some(c=>c.url==='/api/diagnostic'&&c.data.get('command')==='adl400'));
 await page.locator('#language').selectOption('id');assert(await page.getByRole('button',{name:'Pembandingan energi ADL400',exact:true}).count());
 await page.setViewportSize({width:390,height:1000});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 await page.reload();await page.waitForSelector('#meter-profile');assert.equal(await page.locator('#slave').inputValue(),'220');
 console.log('PASS: one ADL400 option, configurable ID220, scaled engineering values, total-energy plan, comparison command, EN/ID, mobile and reload');
};
