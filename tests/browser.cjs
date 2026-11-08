const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL||(process.platform==='win32'?'msedge':undefined),headless:true});
 const page=await browser.newPage({viewport:{width:1200,height:1000}});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
   const cfg=new Uint8Array(63);cfg[0]=2;new DataView(cfg.buffer).setFloat32(1,1.2,true);cfg[5]=cfg[6]=100;cfg[7]=2;cfg[8]=30;cfg[11]=64;cfg[12]=2;cfg[13]=1;cfg[22]=cfg[23]=cfg[24]=255;cfg[26]=3;cfg[27]=8;
   window.writes=[];
   let profile=0,activeMode=2,saveStatus=0,testPattern=0,testUntil=0;
   const listeners={};
   const device={opened:false,vendorId:0x054c,productId:0x0ce6,productName:'DS5 Dongle test fixture',collections:[{featureReports:[{reportId:0xf7}]}],
     async open(){this.opened=true},async close(){this.opened=false},
     async receiveFeatureReport(id){
       if(window.failReport===id)throw Error('fixture read failed');
       let data;
       if(id===0xf7)data=new Uint8Array([id,...cfg]);
       if(id===0xf8)data=new Uint8Array([id,...new TextEncoder().encode(window.legacyFirmware?'TEST Pro-dev1':'TEST Pro-dev1 r11')]);
       if(id===0xf9){data=new Uint8Array(64);data[0]=id;data[1]=210;data[12]=window.fixtureBattery??80;data[14]=0xd5;data[15]=2;data[16]=activeMode;data[17]=1;data[18]=[3,4].includes(activeMode)?0x30:activeMode===6?9:activeMode===5?0:1;data[19]=1;data[32]=data[33]=data[34]=data[35]=128;data[39]=8;data[52]=window.noXbox?7:window.noPro2?31:window.noMotion?63:127;data[53]=saveStatus;data[54]=0xd9;data[55]=8;data[56]=60;data[58]=1;data[62]=7;data[63]=1;}
       if(id===0xfa){
         data=new Uint8Array(64);data[0]=id;data[1]=0xa8;data[2]=1;
         const remaining=Math.max(0,testUntil-Date.now());data[3]=55|(remaining?8:0);data[4]=0x30;data[5]=remaining?testPattern:0;
         const view=new DataView(data.buffer);[16,-32,48,0,8192,0].forEach((n,i)=>view.setInt16(6+i*2,n,true));
         [0,4096,0,16,-32,48].forEach((n,i)=>view.setInt16(18+i*2,n,true));
         view.setUint32(34,10,true);view.setUint32(38,1000,true);view.setUint32(50,500,true);view.setUint16(62,remaining,true);
       }
       if(id===0xf9&&window.legacyFirmware)data=data.slice(0,14);
       if(id===0xfb){data=new Uint8Array(127);data[0]=id;data[2]=profile;for(let i=0;i<31;i++)data[4+i*4]=i;}
       return new DataView(data.buffer);
     },
     async sendFeatureReport(id,data){
       window.writes.push({id,data:[...data]});
       if(id===0xf6&&data[0]===1){cfg.set(data.slice(1,30));saveStatus=0;}
       if(id===0xf6&&data[0]===2)saveStatus=2;
       if(id===0xf6&&data[0]===0x10){testPattern=data[1];testUntil=testPattern?Date.now()+(data[2]|data[3]<<8):0;}
       if(id===0xf6&&data[0]===3){activeMode=cfg[12];this.opened=false;this.vendorId=activeMode===5?0x1209:[3,4,6].includes(activeMode)?0x057e:0x054c;this.productId=activeMode===5?0xdb05:activeMode===6?0x2069:[3,4].includes(activeMode)?0x2009:0x0ce6;listeners.disconnect?.({device:this});setTimeout(()=>listeners.connect?.({device:this}),100);}
       if(id===0xfb&&data[0]===3)profile=data[1];
     }
   };
   window.fixtureDevice=device;
   window.disconnectFixture=()=>{device.opened=false;listeners.disconnect?.({device});};
   Object.defineProperty(navigator,'hid',{value:{async requestDevice(options){window.hidFilters=options.filters;return [device]},async getDevices(){return [device]},addEventListener(name,fn){listeners[name]=fn;}}});
 });
 await page.goto(process.env.TEST_URL||'http://127.0.0.1:8765');await page.getByRole('button',{name:'连接接收器',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 assert.equal(await page.locator('#disconnect-reason').innerText(),'蓝牙连接超时');
 assert.match(await page.locator('#disconnect-battery').innerText(),/60%/);
 const proFilter=await page.evaluate(()=>window.hidFilters.find(f=>f.productId===0x2009));
 assert.equal(proFilter.usagePage,0xff00);assert.equal(proFilter.usage,1);
 const xboxFilter=await page.evaluate(()=>window.hidFilters.find(f=>f.vendorId===0x045e));
 assert.equal(xboxFilter.productId,0x028e);assert.equal(xboxFilter.usagePage,0xff00);
 assert.match(await page.locator('#battery').innerText(),/80/);
 assert.match(await page.locator('#receiver-hint').innerText(),/手柄已连接/);
 await page.evaluate(()=>{window.fixtureBattery=10;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('receiver-hint').textContent.includes('不超过 10%'));
 await page.evaluate(()=>{window.fixtureBattery=80;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled&&document.getElementById('battery').textContent.includes('80'));

 assert.equal(await page.locator('h1').evaluate(el=>parseFloat(getComputedStyle(el).fontSize)>=30),true);
 assert.equal(await page.locator('#save').evaluate(el=>parseFloat(getComputedStyle(el).fontSize)>=14&&Number(getComputedStyle(el).fontWeight)>=700),true);
 await page.screenshot({path:'build-tests/dashboard-r6-desktop.png',fullPage:true});

 await page.locator('#controller_mode').selectOption('3');
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='模式已应用');
 assert.equal(await page.locator('#active-mode').innerText(),'Switch Pro');
 const writes=await page.evaluate(()=>window.writes);assert.equal(writes[0].data[13],3);assert.equal(writes[1].data[0],2);
 assert.equal(writes[2].data[0],3);
 await page.locator('[data-view="diagnostics"]').click();
 await page.waitForFunction(()=>document.getElementById('motion-state').textContent==='体感输出中');
 assert.match(await page.locator('#gyro-data').innerText(),/1.00.*-2.00.*3.00/);
 assert.match(await page.locator('#accel-data').innerText(),/0.00.*1.00.*0.00/);
 await page.getByRole('button',{name:'左侧 HD 试振',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='本地短时试振已启动');
 const hapticWrite=await page.evaluate(()=>window.writes.at(-1));assert.deepEqual(hapticWrite.data.slice(0,4),[0x10,1,0x58,2]);
 await page.getByRole('button',{name:'停止本地试振',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='本地试振已停止');
 assert.equal(await page.evaluate(()=>window.writes.at(-1).data[1]),0);
 await page.screenshot({path:'build-tests/motion-diagnostics-r7.png',fullPage:true});

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
 await page.setViewportSize({width:1200,height:1000});
 await page.locator('[data-mode="5"]').click();
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='模式已应用');
 await page.waitForFunction(()=>document.getElementById('active-mode').textContent==='Xbox 360 / XInput'&&!document.getElementById('save').disabled);
 assert.equal(await page.locator('#active-mode').innerText(),'Xbox 360 / XInput');
 assert.match(await page.locator('#mode-detail').innerText(),/XInput/);
 const compositeFilter=await page.evaluate(()=>window.hidFilters.find(f=>f.vendorId===0x1209));
 assert.equal(compositeFilter.productId,0xdb05);assert.equal(compositeFilter.usagePage,0xff00);
 await page.locator('[data-mode="6"]').click();
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('active-mode').textContent==='Switch 2 Pro（实验）'&&!document.getElementById('save').disabled);
 assert.match(await page.locator('#mode-note').innerText(),/无耳麦音频/);
 const pro2Filter=await page.evaluate(()=>window.hidFilters.find(f=>f.productId===0x2069));
 assert.equal(pro2Filter.usagePage,0xff00);
 await page.setViewportSize({width:390,height:844});
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'Pro2 mobile overflow');
 await page.evaluate(()=>scrollTo(0,0));
 await page.screenshot({path:'build-tests/dashboard-r6-mobile.png'});
 await page.setViewportSize({width:1200,height:1000});
 await page.screenshot({path:'build-tests/pro2-config-r5.png',fullPage:true});

 await page.screenshot({path:'build-tests/xbox-config.png',fullPage:true});
 await page.locator('[data-view="diagnostics"]').click();
 assert.equal(await page.locator('#hd-left').isDisabled(),true,'Pro2 must not pretend to offer HD Rumble 2 testing');
 await page.locator('[data-view="overview"]').click();
 await page.locator('[data-mode="2"]').click();
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('notice-title').textContent==='模式已应用');
 await page.waitForFunction(()=>document.getElementById('active-mode').textContent==='自动 / DS・Edge'&&!document.getElementById('save').disabled);
 assert.equal(await page.locator('#active-mode').innerText(),'自动 / DS・Edge');
 await page.evaluate(()=>{window.noPro2=true;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 await page.locator('[data-mode="6"]').click();
 const beforePro2=await page.evaluate(()=>window.writes.length);
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('r5'));
 assert.equal(await page.evaluate(()=>window.writes.length),beforePro2,'older firmware must reject Pro2 without writing');
 await page.evaluate(()=>{window.noXbox=true;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 await page.locator('[data-mode="5"]').click();
 const beforeXbox=await page.evaluate(()=>window.writes.length);
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('r4'));
 assert.equal(await page.evaluate(()=>window.writes.length),beforeXbox,'r3 must reject Xbox without writing');
 await page.evaluate(()=>{window.legacyFirmware=true;});
 await page.getByRole('button',{name:'重新读取',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 await page.locator('#controller_mode').selectOption('3');
 const beforeLegacy=await page.evaluate(()=>window.writes.length);
 await page.getByRole('button',{name:'保存并应用',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('alpha.3'));
 assert.equal(await page.evaluate(()=>window.writes.length),beforeLegacy,'old firmware must not enable broken Pro path');
 await page.evaluate(()=>{document.querySelector('[data-button]').classList.add('active');window.disconnectFixture();});
 assert.equal(await page.locator('[data-button].active').count(),0);
 assert.equal(await page.locator('#active-mode').innerText(),'尚未连接');
 assert.equal(await page.locator('#save').isDisabled(),true);
 assert.deepEqual(errors,[]);await browser.close();console.log('Browser fixture: connect, padded feature reports, save/readback, remap and mobile layout passed.');
})().catch(e=>{console.error(e);process.exit(1)});
