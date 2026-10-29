const assert=require("node:assert/strict"),path=require("node:path");
module.exports=async({page,origin,state,calls,root})=>{
 await page.goto(origin+"/#setup");await page.waitForSelector("#meter-profile");
 assert.equal(await page.locator("#meter-profile option").count(),3);
 await page.locator("#meter-profile").selectOption("habuer");await page.locator("#config button[type=submit], #config button").last().click();
 await page.waitForFunction(()=>document.querySelector("#scan-probe")?.value==="0030");
 assert(calls.some(c=>c.url==="/api/config"&&c.data.get("profile")==="habuer"));
 Object.assign(state,{energy:12345.6,power:null,currentA:null,voltageA:null,powerFactor:.875,frequency:50,meterEnergy:12345.6,secondaryEnergy:12345.6,reactiveEnergy:65432.1,ratio:1,ratios:true,readingQuality:{required:15,transportFailed:0,invalid:0,complete:true}});
 await page.locator("#nav-overview").click();await page.waitForSelector("#value-reactiveEnergy");await page.waitForFunction(()=>document.querySelector("#power-factor")?.textContent.includes("0.875"));
 assert.equal(await page.locator("#value-currentA").count(),0);assert.equal(await page.locator("#value-voltageA").count(),0);assert.equal(await page.locator("#power").count(),0);
 assert((await page.locator("#power-factor").innerText()).includes("0.875"));assert((await page.locator("#value-frequency").innerText()).includes("50.00"));
 const accepted=await page.locator("#energy").getAttribute("data-value");assert(accepted.includes("12,345.600"));
 await page.waitForTimeout(1600); // Capture settled numerals rather than an intermediate animation frame.
 await page.screenshot({path:path.join(root,"docs/assets/ui-habuer-desktop.png"),fullPage:true});
 state.readingQuality={required:15,transportFailed:2,invalid:0,complete:false};await page.waitForSelector("#snapshot-quality");assert.equal(await page.locator("#energy").getAttribute("data-value"),accepted);
 await page.locator("#language").selectOption("id");assert((await page.locator("#snapshot-quality").innerText()).includes("Snapshot meter"));
 await page.setViewportSize({width:390,height:1000});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.screenshot({path:path.join(root,"docs/assets/ui-habuer-mobile.png"),fullPage:true});
 await page.reload();await page.locator("#nav-setup").click();await page.waitForFunction(()=>document.querySelector("#meter-profile")?.value==="habuer");
 await page.locator("#language").selectOption("en");
 await page.locator("#meter-lab > summary").click();await page.waitForSelector("#lab-start:not([disabled])");assert((await page.locator("#lab-plan").inputValue()).includes("0030:2,0034:2"));
 await page.locator("#lab-start").click();await page.waitForSelector("#lab-findings .lab-save");
 const finding=page.locator("#lab-findings .lab-device").first();assert((await finding.innerText()).includes("Habuer"));
 await finding.getByLabel("Installation name").fill("Habuer panel");await finding.getByLabel("Operator supplied model").fill("Habuer field reference");await finding.getByLabel("Production profile binding").selectOption("habuer");await finding.locator('input[type="checkbox"]').check();await finding.getByRole("button",{name:"Save finding to flash"}).click();await page.waitForSelector("#lab-saved .lab-device");
 await page.locator("#lab-saved").getByRole("button",{name:"Use for measurement"}).click();await page.waitForFunction(()=>document.querySelector("#slave")?.value==="3");
 await page.locator("#meter-profile").selectOption("fort");await page.locator("#config button").last().click();await page.waitForFunction(()=>document.querySelector("#scan-probe")?.value==="0058");
 console.log("PASS: generated-byte Habuer selection/save/reload, PF probe, optional register visibility, direct kWh/reactive energy, rejected-snapshot retention, EN/ID, mobile and Fort restoration");
};
