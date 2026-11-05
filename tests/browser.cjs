const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL||(process.platform==='win32'?'msedge':undefined),headless:true});
 const page=await browser.newPage({viewport:{width:1200,height:1000}});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
   const cfg=new Uint8Array(63);cfg[0]=2;new DataView(cfg.buffer).setFloat32(1,1.2,true);cfg[5]=cfg[6]=100;cfg[7]=2;cfg[8]=30;cfg[11]=64;cfg[12]=2;cfg[13]=1;cfg[22]=cfg[23]=cfg[24]=255;cfg[26]=3;cfg[27]=8;
   window.writes=[];
   let profile=0;
   const device={opened:false,productName:'DS5 Dongle test fixture',collections:[{featureReports:[{reportId:0xf7}]}],
     async open(){this.opened=true},async close(){this.opened=false},
     async receiveFeatureReport(id){
       if(window.failReport===id)throw Error('fixture read failed');
       let data;
       if(id===0xf7)data=new Uint8Array([id,...cfg]);
       if(id===0xf8)data=new Uint8Array([id,...new TextEncoder().encode('TEST Pro-dev1')]);
       if(id===0xf9){data=new Uint8Array(14);data[0]=id;data[1]=210;data[12]=80;}
       if(id===0xfb){data=new Uint8Array(127);data[0]=id;data[2]=profile;for(let i=0;i<31;i++)data[4+i*4]=i;}
       return new DataView(data.buffer);
     },
     async sendFeatureReport(id,data){window.writes.push({id,data:[...data]});if(id===0xf6&&data[0]===1)cfg.set(data.slice(1,30));if(id===0xfb&&data[0]===3)profile=data[1];}
   };
   window.fixtureDevice=device;
   Object.defineProperty(navigator,'hid',{value:{async requestDevice(){return [device]},addEventListener(){}}});
 });
 await page.goto(process.env.TEST_URL||'http://127.0.0.1:8765');await page.getByRole('button',{name:'连接接收器',exact:true}).click();
 await page.waitForFunction(()=>!document.getElementById('save').disabled);
 assert.match(await page.locator('#status').innerText(),/80%/);
 await page.locator('#controller_mode').selectOption('3');
 await page.getByRole('button',{name:'保存到接收器',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('保存命令已发送'));
 const writes=await page.evaluate(()=>window.writes);assert.equal(writes[0].data[13],3);assert.equal(writes[1].data[0],2);
 await page.getByRole('button',{name:'读取映射',exact:true}).click();await page.waitForFunction(()=>!document.getElementById('savemap').disabled);
 assert.equal(await page.locator('#map30').inputValue(),'30');
 await page.locator('#map0').selectOption('2');await page.getByRole('button',{name:'保存映射',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('status').textContent.includes('映射保存'));
 const map=await page.evaluate(()=>window.writes.at(-1));assert.equal(map.id,0xfb);assert.equal(map.data[3],2);
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
 assert.deepEqual(errors,[]);await browser.close();console.log('Browser fixture: connect, padded feature reports, save/readback, remap and mobile layout passed.');
})().catch(e=>{console.error(e);process.exit(1)});
