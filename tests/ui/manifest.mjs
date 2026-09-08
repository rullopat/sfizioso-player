// Browser-side regression check with a mocked JUCE native bridge.
// Serve the repository root on 127.0.0.1:18743 after building the UI.
// Set PLAYWRIGHT_MODULE to an installed Playwright module path if needed.
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE ?? 'playwright');
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import assert from 'node:assert/strict';
const browser = await chromium.launch({headless:true, ...(process.env.PLAYWRIGHT_EXECUTABLE_PATH ? {executablePath: process.env.PLAYWRIGHT_EXECUTABLE_PATH} : {})});
const page = await browser.newPage({viewport:{width:920,height:640}});
const errors=[]; page.on('pageerror',e=>errors.push(e.message));
await page.addInitScript(()=>{
 window.testCalls=[];
 window.testPresentation={instrumentName:'Evening Signals',presetName:'Soft Signal',accent:'#e7a86d',artworkUrl:'http://127.0.0.1:18743/examples/manifest-instrument/artwork/background.png',diagnostics:[],controls:[
  {number:74,label:'Brightness',value:80/127,widget:'knob',section:'tone',sectionLabel:'Tone'},
  {number:7,label:'Volume',value:100/127,widget:'slider',section:'tone',sectionLabel:'Tone'},
  {number:1,label:'Vibrato',value:0,widget:'knob',section:'expression',sectionLabel:'Expression'},
  {number:64,label:'Sustain',value:0,widget:'toggle',section:'expression',sectionLabel:'Expression'}]};
 window.__JUCE__={initialisationData:{__juce__platform:[],__juce__functions:['getInstrumentPresentation','getStatus','getAppInfo','getRecentFiles','getKeyLabels','setCc'],__juce__registeredGlobalEventIds:[],__juce__sliders:['gainDb','polyphony'],__juce__toggles:[],__juce__comboBoxes:[]},postMessage(text){
  const e=JSON.parse(text); if(e.eventId!=='__juce__invoke')return;
  const {name,params,resultId}=e.payload;window.testCalls.push({name,params});
  const result=name==='getInstrumentPresentation'?window.testPresentation:name==='getStatus'?{ok:true,fileName:'instrument.sfz',bundlePatchName:'Soft Signal',numRegions:1,numPreloadedSamples:0}:name==='getAppInfo'?{productName:'Sfizioso Player',version:'draft'}:name==='getKeyLabels'?{labels:[],used:[]}:name==='getRecentFiles'?[]:{ok:true};
  queueMicrotask(()=>window.__JUCE__.backend.emitByBackend('__juce__complete',JSON.stringify({promiseId:resultId,result})));
 }};
});
try {
 await page.goto('http://127.0.0.1:18743/src/player/ui/dist/index.html');
 await page.getByText('Evening Signals',{exact:true}).waitFor();
 assert.equal(await page.getByText('EXPERIMENTAL', {exact:true}).count(), 0);
 assert.equal(await page.getByText('COMING SOON', {exact:true}).count(), 0);
 await page.getByRole('button', {name:'MPE',exact:true}).click();
 assert.deepEqual(await page.locator('.mpe-panel .segmented').first().getByRole('button').allTextContents(), ['OFF','FULL']);
 await page.getByRole('button', {name:'OUTPUT',exact:true}).click();
 assert.deepEqual(await page.locator('.cc-section h3').allTextContents(),['Tone','Expression']);
 const slider=page.getByRole('slider',{name:'Volume',exact:true});
 assert.equal(await slider.inputValue(),'100');
 await slider.fill('32');
 await page.waitForFunction(()=>window.testCalls.some(c=>c.name==='setCc'&&c.params[0]===7&&c.params[1]===32/127));
 await page.locator('.cc-section').filter({hasText:'Expression'}).getByRole('button',{name:'ON',exact:true}).click();
 await page.waitForFunction(()=>window.testCalls.some(c=>c.name==='setCc'&&c.params[0]===64&&c.params[1]===1));
 await page.evaluate(()=>window.__JUCE__.backend.emitByBackend('ccValues',JSON.stringify({'7':80/127})));
 await page.waitForFunction(()=>document.querySelector('.cc-slider input').value==='80');
 await page.locator('.tab-content').evaluate(e => { e.scrollTop = 0; });
 await page.screenshot({path:join(tmpdir(), 'manifest-desktop.png'),fullPage:true});
 await page.setViewportSize({width:720,height:520});
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
 await page.screenshot({path:join(tmpdir(), 'manifest-narrow.png'),fullPage:true});
 await page.evaluate(()=>{
  window.testPresentation={instrumentName:'',presetName:'',accent:'',artworkUrl:'',diagnostics:['Unsupported manifest format or major version.'],controls:[{number:7,label:'Volume',value:1,widget:'knob',section:'',sectionLabel:''}]};
  window.__JUCE__.backend.emitByBackend('sfzLoaded',JSON.stringify({ok:true,fileName:'plain.sfz'}));
 });
 await page.getByText('Some instrument details could not be loaded').waitFor();
 assert.equal(await page.locator('.cc-section h3').count(),0);
 assert.equal(await page.locator('.instrument-identity').count(),0);
 assert.equal(await page.locator('.instrument-presentation').evaluate(e=>getComputedStyle(e).backgroundImage),'none');
 assert.deepEqual(errors,[]);
 console.log('PASS: grouped widgets, CC dispatch, live reflection, narrow layout, metadata/artwork reset; no JS errors.');
} finally { await browser.close(); }
