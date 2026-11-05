import {fields,decode,encode,featurePayload,supportsConfig} from './config.js';
const $=id=>document.getElementById(id);
let device,original,busy=false,proFirmware=false,remap;
const controls=['refresh','save','reboot','export','import','readmap','profile'];
const names=['□','×','○','△','L1','R1','L2','R2','Create','Options','L3','R3','PS','触摸板按下','静音','↑','↓','←','→','左区触摸','左区↑','左区↓','左区←','左区→','左区按下','右区触摸','右区↑','右区↓','右区←','右区→','右区按下'];
for(const [key,label,,min,max,step=1,type] of fields){
 const row=document.createElement('label'); row.append(document.createTextNode(label));
 const input=document.createElement(Array.isArray(type)?'select':'input');input.id=key;
 if(Array.isArray(type)) type.forEach((text,i)=>input.add(new Option(text,i)));
 else {input.type=type==='bool'?'checkbox':'number';input.min=min;input.max=max;input.step=step;input.required=type!=='bool';}
 row.append(input);$('fields').append(row);
}
function status(text){$('status').textContent=text;}
function enabled(){const ready=!!device?.opened&&!!original&&!busy;controls.forEach(id=>$(id).disabled=!ready);$('fields').disabled=!ready;$('connect').disabled=busy;$('mapping').disabled=!ready||!remap;$('savemap').disabled=$('activemap').disabled=!ready||!remap;}
async function run(fn){if(busy)return;busy=true;enabled();try{await fn();}catch(e){status(e.message||String(e));}finally{busy=false;enabled();}}
function show(bytes){const values=decode(bytes);for(const [key,,,,,,type] of fields){if(type==='bool')$(key).checked=!!values[key];else $(key).value=type==='float'?Number(values[key].toFixed(2)):values[key];}}
function values(){return Object.fromEntries(fields.map(([key,,,,,,type])=>[key,type==='bool'?Number($(key).checked):Number($(key).value)]));}
async function receive(id){return featurePayload(await device.receiveFeatureReport(id),id);}
async function send(id,bytes){const size=id===0xfb?127:63;if(bytes.length>size)throw Error('报告过长');const packet=new Uint8Array(size);packet.set(bytes);await device.sendFeatureReport(id,packet);}
async function read(){
 // A failed refresh/new connection must never leave another device's settings writable.
 original=null;remap=null;proFirmware=false;$('mapping').replaceChildren();
 const cfg=await receive(0xf7);decode(cfg);
 const version=new TextDecoder().decode(await receive(0xf8)).replace(/\0.*$/s,'');
 const s=await receive(0xf9);
 if(s.length<13)throw Error('状态报告不完整');
 const rssi=new DataView(s.buffer,s.byteOffset).getInt8(0);
 original=cfg;proFirmware=version.includes('Pro-dev1');show(cfg);
 $('device').textContent=`${device.productName} · ${version}`;
 status(`已读取配置 · 电量 ${s[11]===255?'未知':s[11]+'%'} · RSSI ${rssi} dBm${proFirmware?'':'\n当前固件没有本项目 Pro 模式标记：可配置 DS 模式，需刷入修改后的固件才能选择 Switch Pro。'}`);
}
$('connect').onclick=()=>run(async()=>{
 if(!navigator.hid)throw Error('请在桌面 Chrome / Edge 打开 http://localhost:8765；当前环境不支持 WebHID。');
 const devices=await navigator.hid.requestDevice({filters:[{vendorId:0x054c,productId:0x0ce6},{vendorId:0x054c,productId:0x0df2},{vendorId:0x057e,productId:0x2009}]});
 const selected=devices.find(supportsConfig);
 if(!selected)throw Error('未选择带接收器配置接口的设备。请勿选择直接连接的原装手柄。');
 original=null;remap=null;proFirmware=false;
 if(device?.opened&&device!==selected)await device.close();
 device=selected;if(!device.opened)await device.open();await read();
});
$('refresh').onclick=()=>run(read);
async function save(reboot){
 const next=encode(original,values());
 if(next[12]===3&&!proFirmware)throw Error('请先刷入本项目的 Pro-dev1 固件，再启用 Switch Pro 模式。');
 // Write only known fields; firmware preserves any newer trailing fields.
 await send(0xf6,new Uint8Array([1,...next.slice(0,29)]));
 const actual=await receive(0xf7);decode(actual);
 if(next.slice(0,29).some((v,i)=>actual[i]!==v))throw Error('设备回读与写入不符，配置尚未保存到闪存。');
 await send(0xf6,new Uint8Array([2]));original=actual;
 status('保存命令已发送。模式切换需重启；固件协议没有闪存保存成功回执。');
 if(reboot){await send(0xf6,new Uint8Array([3]));status('重启命令已发送。设备重新枚举后，请再次连接。');original=null;remap=null;}
}
$('settings').onsubmit=e=>{e.preventDefault();run(()=>save(false));};
$('reboot').onclick=()=>{if($('settings').reportValidity())run(()=>save(true));};
$('export').onclick=()=>run(async()=>{const bytes=encode(original,values());const blob=new Blob([JSON.stringify({format:'ds5-local-v1',bytes:[...bytes]},null,2)],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='ds5-config.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
$('import').onchange=e=>run(async()=>{const file=e.target.files[0];if(!file)return;if(file.size>8192)throw Error('配置文件过大');const json=JSON.parse(await file.text());if(json.format!=='ds5-local-v1'||!Array.isArray(json.bytes)||json.bytes.some(x=>!Number.isInteger(x)||x<0||x>255))throw Error('无效的配置文件');const imported=new Uint8Array(json.bytes);const settings=decode(imported);show(encode(original,settings));status('已载入文件到表单，点击保存后才写入接收器。');});
$('readmap').onclick=()=>run(async()=>{
 await send(0xfb,new Uint8Array([3,Number($('profile').value)]));
 const bytes=await receive(0xfb);if(bytes.length<126||bytes[1]!==Number($('profile').value))throw Error('映射报告不完整或配置编号不符');remap=bytes.slice(2,126);$('mapping').replaceChildren();
 names.forEach((name,i)=>{const row=document.createElement('label');row.append(name);const select=document.createElement('select');select.id=`map${i}`;names.forEach((n,j)=>select.add(new Option(n,j)));const type=remap[i*4];if(type!==0)select.add(new Option(`保留已有${type===1?'键盘':'鼠标'}映射`,'keep'));select.value=type===0?remap[i*4+1]:'keep';row.append(select);$('mapping').append(row);});
 status('映射已读取。修改游戏手柄目标将替换该行原有映射。');
});
$('profile').onchange=()=>{remap=null;$('mapping').replaceChildren();enabled();};
$('savemap').onclick=()=>run(async()=>{const table=remap.slice();for(let i=0;i<31;i++){const value=$(`map${i}`).value;if(value!=='keep'){table.set([0,Number(value),0,table[i*4+3]&1],i*4);}}await send(0xfb,new Uint8Array([1,Number($('profile').value),...table]));status('映射保存命令已发送。');});
$('activemap').onclick=()=>run(async()=>{await send(0xfb,new Uint8Array([5,Number($('profile').value)]));status('已切换活动映射配置。');});
navigator.hid?.addEventListener('disconnect',e=>{if(e.device===device){original=null;remap=null;enabled();status('接收器已断开。重新连接后读取配置。');}});
enabled();if(!navigator.hid)status('当前环境不支持 WebHID。请使用桌面 Chrome / Edge 打开 localhost 配置页。');
