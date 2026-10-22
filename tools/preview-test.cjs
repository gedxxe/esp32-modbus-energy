// Tests the real embedded HTML with explicitly simulated API responses.
const fs=require('fs'),path=require('path'),http=require('http'),assert=require('assert');
const {chromium}=require(process.argv[2]||'playwright');
const root=path.resolve(__dirname,'..');
const header=fs.readFileSync(path.join(root,'firmware/include/web_ui.h'),'utf8');
const html=header.split('R"HTML(')[1].split(')HTML";')[0];
const version=fs.readFileSync(path.join(root,'firmware/include/version.h'),'utf8').match(/FIRMWARE_VERSION\[\] = "([^"]+)"/)[1];
const starts=[360,390,420,450,480,510,990,1020,1050];
let online=true,fail=false,calls=[];
const state={version,token:'test-session',rtc:true,storage:true,ratios:true,muted:false,online:true,energy:128432.728,power:48.62,age:1,ratio:80,slave:1,baud:9600,parity:'N',crcErrors:0,timeouts:2,rejected:0,heap:180000,uptime:11300,error:'OK',clock:'08:42:16',date:'2026-10-22',dayDate:'2026-10-22',slots:starts.map((start,i)=>({start,kwh:i<5?[21.32,24.85,23.11,25.68,22.44][i]:i===5?9.12:null,quality:i<5?'lengkap-estimasi':i===5?'berjalan':'menunggu'}))};
const server=http.createServer((req,res)=>{
  if(req.url==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);return;}
  res.setHeader('Content-Type','application/json');
  if(req.url==='/api/status'){if(fail){res.statusCode=503;res.end('{}');return;}res.end(JSON.stringify({...state,online}));return;}
  if(req.url==='/api/history'){res.end(JSON.stringify([{day:20748,date:'2026-10-22'},{day:20747,date:'2026-10-21'}]));return;}
  if(req.method==='POST'){let data='';req.on('data',c=>data+=c);req.on('end',()=>{calls.push({url:req.url,data:new URLSearchParams(data)});res.statusCode=202;res.end(JSON.stringify({message:'Perintah diterima (simulasi).'}));});return;}
  res.statusCode=404;res.end('{}');
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  let browser;
  try{
    browser=await chromium.launch({headless:true,...(process.argv[3]?{channel:process.argv[3]}:{})});
    const page=await browser.newPage({viewport:{width:1440,height:1150}});let errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(`http://127.0.0.1:${server.address().port}`);await page.waitForFunction(()=>document.getElementById('intervals').children.length===6);
    assert.equal(await page.locator('#slave').inputValue(),'1');
    assert.equal(await page.locator('#version').textContent(),'Firmware '+version);
    await page.getByRole('button',{name:'Sore · 16.30–18'}).click();assert.equal(await page.locator('#intervals tr').count(),3);assert((await page.locator('#intervals').innerText()).includes('16:30'));
    await page.getByRole('button',{name:'Pagi · 06–09'}).click();
    await page.locator('#sync').click();await page.waitForFunction(()=>document.getElementById('message').textContent.includes('simulasi'));assert.equal(calls[0].url,'/api/time');assert.equal(calls[0].data.get('token'),'test-session');
    await page.locator('summary').click();await page.locator('#slave').fill('7');await page.locator('#baud').selectOption('19200');await page.locator('#parity').selectOption('E');await page.getByRole('button',{name:'Simpan koneksi'}).click();await page.waitForTimeout(100);assert.equal(calls[1].url,'/api/config');assert.equal(calls[1].data.get('slave'),'7');assert.equal(calls[1].data.get('parity'),'E');
    await page.locator('summary').click();
    const out=path.join(root,'output/previews');fs.mkdirSync(out,{recursive:true});
    const assets=path.join(root,'docs/assets');fs.mkdirSync(assets,{recursive:true});
    await page.screenshot({path:path.join(out,'dashboard-desktop-simulation.png'),fullPage:true});
    await page.screenshot({path:path.join(assets,'ui-desktop.png'),fullPage:false});
    await page.setViewportSize({width:390,height:844});await page.screenshot({path:path.join(out,'dashboard-mobile-simulation.png'),fullPage:true});
    await page.screenshot({path:path.join(assets,'ui-mobile.png'),fullPage:true});
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    online=false;await page.evaluate(()=>poll());await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('offline'));assert.equal(await page.locator('#power').textContent(),'—');
    fail=true;await page.evaluate(()=>poll());await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('terputus'));assert((await page.locator('#health').textContent()).includes('tidak diperbarui'));
    assert.deepEqual(errors,[]);await page.close();
    // Render repository-local vectors, check all text remains inside each SVG.
    const svgPage=await browser.newPage();
    const svgFiles=[];
    function collect(dir){for(const entry of fs.readdirSync(dir,{withFileTypes:true})){let f=path.join(dir,entry.name);if(entry.isDirectory())collect(f);else if(f.endsWith('.svg'))svgFiles.push(f)}}
    collect(assets);
    for(const f of svgFiles){
      let svg=fs.readFileSync(f,'utf8'),box=svg.match(/viewBox="0 0 (\d+) (\d+)"/);assert(box,'viewBox missing: '+f);
      await svgPage.setViewportSize({width:Number(box[1]),height:Number(box[2])});
      await svgPage.setContent('<html><body style="margin:0">'+svg+'</body></html>');
      await svgPage.evaluate(()=>document.fonts.ready);
      let overflow=await svgPage.evaluate(()=>{let svg=document.querySelector('svg'),v=svg.viewBox.baseVal;return [...svg.querySelectorAll('text')].filter(t=>{let b=t.getBBox();return b.x<-.5||b.y<-.5||b.x+b.width>v.width+.5||b.y+b.height>v.height+.5}).map(t=>t.textContent)});
      assert.deepEqual(overflow,[],'SVG text overflow: '+path.basename(f));
      if(!f.includes(path.sep+'badges'+path.sep))await svgPage.screenshot({path:path.join(out,path.basename(f,'.svg')+'.png'),fullPage:true});
    }
    await svgPage.close();
    console.log('PASS: desktop/mobile, 6+3 interval tabs, version, time sync, config/token, offline/stale UI, no JS errors; '+svgFiles.length+' SVGs rendered without text overflow');
  }finally{if(browser)await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1});
