const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL||(process.platform==='win32'?'msedge':undefined),headless:true});
 const page=await browser.newPage({viewport:{width:1200,height:1000}});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
   const cfg=new Uint8Array(63);cfg[0]=2;new DataView(cfg.buffer).setFloat32(1,1.2,true);cfg[5]=cfg[6]=100;cfg[7]=2;cfg[8]=30;cfg[11]=64;cfg[12]=2;cfg[13]=1;cfg[22]=cfg[23]=cfg[24]=255;cfg[26]=3;cfg[27]=8;
   window.writes=[];
   let profile=0,activeMode=2,saveStatus=0;
   const listeners={};
   const device={opened:false,vendorId:0x054c,productId:0x0ce6,productName:'DS5 Dongle test fixture',collections:[{featureReports:[{reportId:0xf7}]}],
     async open(){this.opened=true},async close(){this.opened=false},
     async receiveFeatureReport(id){
       if(window.failReport===id)throw Error('fixture read failed');
       let data;
       if(id===0xf7)data=new Uint8Array([id,...cfg]);
       if(id===0xf8)data=new Uint8Array([id,...new TextEncoder().encode(window.legacyFirmware?'TEST Pro-dev1':'TEST Pro-dev1 r3')]);
       if(id===0xf9){data=new Uint8Array(54);data[0]=id;data[1]=210;data[12]=80;data[14]=0xd5;data[15]=2;data[16]=activeMode;data[17]=1;data[18]=0x30;data[19]=1;data[32]=data[33]=data[34]=data[35]=128;data[39]=8;data[52]=7;data[53]=saveStatus;}
       if(id===0xf9&&window.legacyFirmware)data=data.slice(0,14);
       if(id===0xfb){data=new Uint8Array(127);data[0]=id;data[2]=profile;for(let i=0;i<31;i++)data[4+i*4]=i;}
       return new DataView(data.buffer);
     },
     async sendFeatureReport(id,data){
       window.writes.push({id,data:[...data]});
       if(id===0xf6&&data[0]===1){cfg.set(data.slice(1,30));saveStatus=0;}
       if(id===0xf6&&data[0]===2)saveStatus=2;
       if(id===0xf6&&data[0]===3){activeMode=cfg[12];this.opened=false;this.vendorId=activeMode>=3?0x057e:0x054c;this.productId=activeMode>=3?0x2009:0x0ce6;listeners.disconnect?.({device:this});setTimeout(()=>listeners.connect?.({device:this}),100);}
       if(id===0xfb&&data[0]===3)profile=data[1];
     }
   };
   window.fixtureDevice=device;
   Object.defineProperty(navigator,'hid',{value:{async requestDevice(){return [device]},async getDevices(){return [device]},addEventListener(name,fn){listeners[name]=fn;}}});
 });
 await page.goto(process.env.TEST_URL||'http://127.0.0.1:8765');await page.getByRole('button',{name:'连接接收器',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 assert.match(await page.locator('#battery').innerText(),/80/);
 await page.locator('#controller_mode').selectOption('3');
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='模式已应用');
 assert.equal(await page.locator('#active-mode').innerText(),'Switch Pro');
 const writes=await page.evaluate(()=>window.writes);assert.equal(writes[0].data[13],3);assert.equal(writes[1].data[0],2);
 assert.equal(writes[2].data[0],3);
 await page.locator('[data-view="mapping"]').click();
 await page.getByRole('button',{name:'读取映射',exact:true}).click();await page.waitForFunction(()=>!document.getElementById('savemap').disabled);
 assert.equal(await page.locator('#map30').inputValue(),'30');
 await page.locator('#map0').selectOption('2');await page.getByRole('button',{name:'保存映射',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent.includes('映射保存'));
 const map=await page.evaluate(()=>window.writes.at(-1));assert.equal(map.id,0xfb);assert.equal(map.data[3],2);
 await page.locator('[data-view="overview"]').click();
 await page.screenshot({path:'docs/local-config.png',fullPage:true});
 await page.setViewportSize({width:390,height:844});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
 await page.evaluate(()=>{window.failReport=0xf8;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('fixture read failed'));
 assert.equal(await page.locator('#save').isDisabled(),true);
 assert.equal(await page.locator('#savemap').isDisabled(),true);
 assert.equal(await page.locator('#profile').isDisabled(),true);
 await page.evaluate(()=>{window.failReport=undefined;});
 await page.getByRole('button',{name:'连接接收器',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 assert.equal(await page.locator('#savemap').isDisabled(),true);
 await page.locator('[data-view="audio"]').click();
 assert.equal(await page.getByRole('button',{name:'开始本地测试'}).isVisible(),true);
 await page.setViewportSize({width:1200,height:1000});
 await page.locator('[data-view="overview"]').click();
 await page.locator('#controller_mode').selectOption('4');
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='模式已应用');
 assert.equal(await page.locator('#active-mode').innerText(),'Pro + USB 麦克风');
 await page.locator('[data-view="audio"]').click();
 await page.screenshot({path:'docs/audio-config.png',fullPage:true});
 await page.evaluate(()=>{
   document.getElementById('audio-device').add(new Option('Pro Controller fixture','fixture-mic'));
   document.getElementById('audio-device').value='fixture-mic';
   Object.defineProperty(navigator,'mediaDevices',{configurable:true,value:{getUserMedia:()=>new Promise(resolve=>{window.resolveAudio=()=>resolve({getTracks:()=>[{stop(){window.audioStopped=true;}}]});})}});
 });
 await page.getByRole('button',{name:'开始本地测试',exact:true}).click();
 assert.equal(await page.locator('#start-audio').isDisabled(),true);
 await page.locator('[data-view="overview"]').click();
 await page.evaluate(()=>window.resolveAudio());
 await page.waitForFunction(()=>window.audioStopped===true);
 await page.setViewportSize({width:390,height:844});
 for(const view of ['audio','advanced','diagnostics','mapping','overview']){
   await page.locator(`[data-view="${view}"]`).click();
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,view+' overflow');
 }
 await page.evaluate(()=>{window.legacyFirmware=true;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 await page.locator('#controller_mode').selectOption('3');
 const beforeLegacy=await page.evaluate(()=>window.writes.length);
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('alpha.3'));
 assert.equal(await page.evaluate(()=>window.writes.length),beforeLegacy,'old firmware must not enable broken Pro path');
 assert.deepEqual(errors,[]);await browser.close();console.log('Browser fixture: connect, padded feature reports, save/readback, remap and mobile layout passed.');
})().catch(e=>{console.error(e);process.exit(1)});
