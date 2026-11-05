import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
const {chromium}=process.env.PLAYWRIGHT_MODULE?await import(process.env.PLAYWRIGHT_MODULE):await import('playwright');
const browser=await chromium.launch({headless:true,...(process.env.CHROMIUM_PATH?{executablePath:process.env.CHROMIUM_PATH}:{}),args:['--no-sandbox']});
const page=await browser.newPage({viewport:{width:390,height:844},deviceScaleFactor:1});
const failures=[];const checks=[];page.on('pageerror',e=>failures.push(e.message));
page.on('console',m=>{if(m.type()==='error')failures.push(m.text());});
const url=process.env.APP_URL||'http://127.0.0.1:8080/';
page.on('request',request=>{if(!request.url().startsWith(url)&&!request.url().startsWith('data:'))failures.push('Unexpected external request: '+request.url());});
const check=async(name,fn)=>{await fn();checks.push(name);console.log('PASS',name);};
const ready=()=>page.waitForFunction(()=>document.querySelector('#main')?.getAttribute('aria-busy')==='false'&&document.querySelector('.temperature'));
const scenario=async id=>{await page.locator('#scenario').selectOption(id);await page.waitForFunction(()=>document.querySelector('#feedback').textContent.includes('scenario loaded'));await page.waitForFunction(()=>document.querySelector('#scenario').disabled===false);};
const navigate=async name=>{await page.locator('.bottom-nav').getByRole('button',{name,exact:true}).click();};
try{
 await page.goto(url);await ready();
 await check('demo Home loads and local assets have no console errors',async()=>{assert.match(await page.locator('#main').innerText(),/36\.8/);assert.match(await page.locator('#main').innerText(),/11:00–16:00/);});
 await mkdir(new URL('../docs/screenshots/',import.meta.url),{recursive:true});
 await page.screenshot({path:new URL('../docs/screenshots/home.jpg',import.meta.url).pathname,fullPage:true,type:"jpeg",quality:75});
 await check('confirmed temperature and jets requests update central state',async()=>{
  await page.getByRole('button',{name:'Increase target temperature'}).click();await page.waitForFunction(()=>document.querySelector('#feedback').textContent.includes('Request accepted'));assert.match(await page.locator('#main').innerText(),/38\.5/);
  await page.locator('[data-action=jets][data-value=HIGH]').click();await page.waitForFunction(()=>document.querySelector('[data-action=jets][data-value=HIGH]')?.getAttribute('aria-pressed')==='true');
  assert.match(await page.locator('.status-strip').innerText(),/HIGH/);await navigate('Diagnostics');assert.match(await page.locator('#main').innerText(),/SSR command\s+OFF/);await navigate('Home');
 });
 await check('rejected requests leave target unchanged and show error',async()=>{
  const before=await page.locator('.temperature-control strong').textContent();await page.getByRole('button',{name:'Reject next request'}).click();await page.getByRole('button',{name:'Increase target temperature'}).click();await page.waitForFunction(()=>document.querySelector('#feedback').textContent.includes('Simulated API rejection'));assert.equal(await page.locator('.temperature-control strong').textContent(),before);
 });
 await check('all 12 selectable scenarios render coherent screens',async()=>{for(const id of ['idle','heating','reached','high','sensor-a','sensor-b','mismatch','flow','no-response','overtemp','wifi']){await scenario(id);if(['sensor-a','sensor-b','mismatch','flow','no-response','overtemp','wifi'].includes(id))assert.equal(await page.getByRole('button',{name:'Increase target temperature'}).isDisabled(),true);}});
 await check('offline banner and controls disabled',async()=>{assert.match(await page.locator('#banners').innerText(),/Controller offline/);assert.equal(await page.locator('[data-action=jets][data-value=HIGH]').isDisabled(),true);});
 await scenario('flow');await navigate('Diagnostics');
 await page.screenshot({path:new URL('../docs/screenshots/diagnostics.jpg',import.meta.url).pathname,fullPage:true,type:"jpeg",quality:75});
 await check('fault Diagnostics shows OEM code, trigger, snapshot, causes and checks',async()=>{const text=await page.locator('#main').innerText();for(const v of ['dr','DELTA_T_HIGH','Captured at trigger','Probable causes','Suggested checks','4.0°C'])assert.ok(text.includes(v));});
 await check('History events open full immutable fault details',async()=>{await navigate('History');await page.locator('[data-action=event]').filter({hasText:'Low flow suspected'}).first().click();await page.locator('#detail-dialog').waitFor({state:'visible'});assert.match(await page.locator('#dialog-content').innerText(),/DELTA_T_HIGH/);await page.getByRole('button',{name:'Close',exact:true}).click();});
 await scenario('idle');await navigate('Settings');
 await check('schedule draft survives polling and saves overnight weekdays',async()=>{await page.locator('[name=start]').fill('22:00');await page.locator('[name=end]').fill('06:00');await page.locator('[name=day][value="7"]').uncheck();await page.waitForTimeout(1800);assert.equal(await page.locator('[name=start]').inputValue(),'22:00');await page.getByRole('button',{name:'Save schedule',exact:true}).click();await page.waitForFunction(()=>document.querySelector('#feedback').textContent.includes('Request accepted'));await navigate('Home');assert.match(await page.locator('.schedule-summary').innerText(),/22:00–06:00/);});
 await check('Service entry confirmation can be cancelled and confirmed',async()=>{await navigate('Settings');await page.locator('[data-action=confirm-service]').click();await page.getByRole('button',{name:'Cancel',exact:true}).click();assert.equal(await page.locator('#banners').innerText(),'');await page.locator('[data-action=confirm-service]').click();await page.locator('#dialog-confirm').click();await page.waitForFunction(()=>document.querySelector('#banners').textContent.includes('Service Mode'));await navigate('Home');assert.equal(await page.getByRole('button',{name:'Increase target temperature'}).isDisabled(),true);});
 await check('Service persists after browser reload and explicitly exits',async()=>{await page.reload();await ready();assert.match(await page.locator('#banners').innerText(),/Service Mode/);await navigate('Settings');await page.locator('[data-action=confirm-service]').click();await page.locator('#dialog-confirm').click();await page.waitForFunction(()=>!document.querySelector('#banners').textContent.includes('Service Mode'));});
 await check('watchdog scenario requires reset then service exit',async()=>{await scenario('watchdog');await navigate('Diagnostics');assert.match(await page.locator('#main').innerText(),/WATCHDOG_RECOVERY/);await page.locator('[data-action=confirm-reset]').click();await page.locator('#dialog-confirm').click();await page.waitForFunction(()=>!document.querySelector('.fault-card'));assert.match(await page.locator('#banners').innerText(),/Service Mode/);await navigate('Settings');await page.locator('[data-action=confirm-service]').click();await page.locator('#dialog-confirm').click();await page.waitForFunction(()=>!document.querySelector('#banners').textContent.includes('Service Mode'));});
 await check('paused updates become stale and cannot send commands',async()=>{await navigate('Home');await page.getByRole('button',{name:'Pause updates',exact:true}).click();await page.waitForFunction(()=>document.querySelector('#banners').textContent.includes('Data is stale'),null,{timeout:10000});assert.equal(await page.getByRole('button',{name:'Increase target temperature'}).isDisabled(),true);await page.getByRole('button',{name:'Resume updates',exact:true}).click();await page.waitForFunction(()=>!document.querySelector('#banners').textContent.includes('Data is stale'));});
 await check('four screens fit 360, 390, 430 and 1280px without horizontal overflow',async()=>{for(const width of [360,390,430,1280]){await page.setViewportSize({width,height:900});for(const name of ['Home','History','Diagnostics','Settings']){await navigate(name);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`${name} overflow at ${width}`);}}});
 assert.deepEqual(failures,[]);checks.push('no console errors or external dependencies');
 await mkdir(new URL('../artifacts/',import.meta.url),{recursive:true});await writeFile(new URL('../artifacts/browser-results.json',import.meta.url),JSON.stringify({passed:checks,failures},null,2));
 console.log(`${checks.length} browser checks passed.`);
}finally{await browser.close();}
