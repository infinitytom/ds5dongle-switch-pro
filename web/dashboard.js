import {fields,decode,encode,featurePayload,supportsConfig,decodeStatus,decodeMotionStatus} from './config.js';

const $=id=>document.getElementById(id);
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const labels=['DualSense','DualSense Edge','自动 / DS・Edge','Switch Pro','Pro + USB 麦克风','Xbox 360 / XInput','Switch 2 Pro（实验）'];
const names=['□','×','○','△','L1','R1','L2','R2','Create','Options','L3','R3','PS','触摸板按下','静音','↑','↓','←','→','左区触摸','左区↑','左区↓','左区←','左区→','左区按下','右区触摸','右区↑','右区↓','右区←','右区→','右区按下'];
const audioFields=new Set(['speaker_volume','headset_volume','speaker_gain','disable_mic','disable_speaker','lock_volume','audio_haptic']);
const quickFields=new Set(['haptics_gain','polling_rate_mode','inactive_time','disable_led']);
let device=null,original=null,firmware='',diagnostic=null,remap=null,busy=false,polling=false,failures=0;
let pendingMode=null,reconnectUntil=0,reconnectTimer=null,readGeneration=0;
let audioStream=null,audioContext=null,audioAnimation=0,audioSession=0;
let motionDiagnostic=null;
const events=[];
const controls=['refresh','save','reboot','export','import','readmap','profile'];

function log(message){
  events.unshift({time:new Date().toLocaleTimeString('zh-CN',{hour12:false}),message});
  events.splice(30);$('event-log').replaceChildren();
  for(const entry of events){const li=document.createElement('li'),time=document.createElement('time');time.textContent=entry.time;li.append(time,document.createTextNode(entry.message));$('event-log').append(li);}
}
function announce(title,message,kind='info'){
  $('notice-title').textContent=title;$('status').textContent=message;$('notice').className=`notice ${kind}`;
}
function setView(view){
  document.querySelectorAll('[data-panel]').forEach(p=>p.classList.toggle('active',p.dataset.panel===view));
  document.querySelectorAll('[data-view]').forEach(b=>{b.classList.toggle('active',b.dataset.view===view);b.setAttribute('aria-current',b.dataset.view===view?'page':'false');});
  const title={overview:'设备概览',mapping:'按键映射',audio:'音频与麦克风',advanced:'高级设置',diagnostics:'连接诊断',support:'支持开发'}[view];
  $('view-name').textContent=title;$('page-title').textContent=view==='overview'?'接收器控制台':title;
  if(view!=='audio')stopAudio();
}
document.querySelectorAll('[data-view]').forEach(b=>b.addEventListener('click',()=>setView(b.dataset.view)));

for(const [key,label,,min,max,step=1,type] of fields){
  const row=document.createElement('label');row.append(document.createTextNode(label));
  const input=document.createElement(Array.isArray(type)?'select':'input');input.id=key;
  if(Array.isArray(type))type.forEach((text,i)=>input.add(new Option(text,String(i))));
  else{input.type=type==='bool'?'checkbox':'number';input.min=min;input.max=max;input.step=step;input.required=type!=='bool';}
  input.addEventListener('change',updateSummary);input.addEventListener('input',updateSummary);row.append(input);
  const target=key==='controller_mode'?'fields':quickFields.has(key)?'quick-fields':audioFields.has(key)?'audio-fields':'advanced-fields';
  $(target).append(row);
}
$('controller_mode').value='2';
document.querySelectorAll('[data-mode]').forEach(b=>b.addEventListener('click',()=>{$('controller_mode').value=b.dataset.mode;updateSummary();}));

function formValues(){return Object.fromEntries(fields.map(([key,,,,,,type])=>[key,type==='bool'?Number($(key).checked):Number($(key).value)]));}
function showConfig(bytes){const values=decode(bytes);for(const [key,,,,,,type] of fields){if(type==='bool')$(key).checked=!!values[key];else $(key).value=type==='float'?Number(values[key].toFixed(2)):values[key];}updateSummary();}
function activeMode(){
  if(diagnostic?.extended)return diagnostic.activeMode;
  if(!original)return null;
  const saved=original[12];
  return modeIdentity(saved,device)?saved:modeIdentity(6,device)?6:modeIdentity(5,device)?5:device?.vendorId===0x057e?3:0;
}
function modeIdentity(mode,d){return mode===6?d?.vendorId===0x057e&&d?.productId===0x2069:mode===5?(d?.vendorId===0x1209&&d?.productId===0xdb05)||(d?.vendorId===0x045e&&d?.productId===0x028e):[3,4].includes(mode)?d?.vendorId===0x057e&&d?.productId===0x2009:d?.vendorId===0x054c;}
function mustReconnect(next){
  if(next[12]!==activeMode())return true;
  if(!original)return false;
  return [13,14,17,21].some(i=>next[i]!==original[i])||([2,3].includes(next[25])!==[2,3].includes(original[25]));
}
function updateSummary(){
  const mode=Number($('controller_mode').value);
  document.querySelectorAll('[data-mode]').forEach(b=>b.classList.toggle('selected',Number(b.dataset.mode)===mode||(Number(b.dataset.mode)===2&&mode<=2)));
  $('mode-note').textContent=mode===6?'独立 Switch 2 Pro 实验协议：输入、体感及 SDL 基础震动转换；静音键映射 C。无耳麦音频，完整 HD Rumble 2 和 Switch 2 主机均未验证。':mode===5?'电脑 XInput + 四通道 USB 音频：前两通道声音，后两通道触觉；普通音频可在音频页转为触觉。音频播放优先于双马达震动，无麦克风。':mode===4?'增加独立 USB 麦克风接口。先在电脑选择此输入设备测试，Switch 2 的复合设备兼容性待实测。':mode===3?'一代 Switch Pro 协议。主机开启有线通信；浏览器普通手柄输入使用标准 0x3F，模拟器可切为完整报告。':'保留 DualSense 原生音频、麦克风、自适应扳机和键鼠功能。';
  if(mode===5&&diagnostic?.extended&&!(diagnostic.capabilities&16))$('mode-note').textContent='当前固件仅提供基础 XInput；四通道音频与音频触觉需要刷入 r5 或更新固件。';
  if(mode===6){
    const revision=Number(firmware.match(/Pro-dev1 r(\d+)/)?.[1]||0);
    $('mode-note').textContent=diagnostic&&!(diagnostic.capabilities&32)?'此接收器固件没有 Pro2 功能，不能保存该模式。请核对连接诊断中的固件版本，网页更新不会升级固件。':
      revision&&revision<8?'当前固件的 Pro2 主要面向电脑，缺少 NS2 主机身份与配对握手。主机测试需 r8 或更新固件；本接收器尚未通过 NS2 验收。':
      revision&&revision<10?'当前 Pro2 固件缺少本轮主机 0x09 体感、Edge GL/GR、玩家灯和校准保存扩展，请刷入 r10 后复测；无耳麦音频与完整 HD Rumble 2。':
      'Pro2 实验：r10 新增主机 0x09 体感转换、Edge GL/GR、玩家灯与用户校准保存；电脑 0x05 保留。无耳麦音频与完整 HD Rumble 2，新功能待本接收器主机实测。';
  }
  if(!original){$('apply-summary').textContent=pendingMode!==null?'正在重新连接设备':'等待连接设备';$('apply-detail').textContent=pendingMode!==null?'需要时点击右上角授权新模式接收器':'配置由接收器保存到闪存';$('save-indicator').className='save-indicator';return;}
  const before=decode(original),values=formValues();
  const dirty=fields.filter(([key])=>Math.abs(Number(before[key])-Number(values[key]))>.00001).length;
  let reconnect=false;try{reconnect=mustReconnect(encode(original,values));}catch{/* Form validation reports errors on save. */}
  $('apply-summary').textContent=dirty?`${dirty} 项设置待保存`:reconnect?'模式已保存，等待应用':'设置已与接收器同步';
  $('apply-detail').textContent=reconnect?`应用后自动保存并重启接收器，等待以 ${labels[mode]} 重新枚举`:'保存后回读配置；新版固件会确认闪存写入结果';
  $('save-indicator').className=`save-indicator ${dirty||reconnect?'dirty':'saved'}`;
}
function enableControls(){
  const ready=!!device?.opened&&!!original&&!busy;
  controls.forEach(id=>$(id).disabled=!ready);
  document.querySelectorAll('[data-config]').forEach(f=>f.disabled=!ready);
  document.querySelectorAll('[data-mode]').forEach(b=>b.disabled=!ready);
  $('connect').disabled=busy;
  $('connect').textContent=pendingMode!==null?'授权新模式接收器':device?.opened&&original?'切换接收器':'连接接收器';
  $('mapping').disabled=!ready||!remap;$('savemap').disabled=$('activemap').disabled=!ready||!remap;
  const connected=!!original&&!!device?.opened;
  $('connection-pill').className=`connection-pill ${connected?'connected':''}`;
  $('connection-pill').replaceChildren();const dot=document.createElement('i');$('connection-pill').append(dot,document.createTextNode(connected?'USB 已连接':pendingMode!==null?'等待重新枚举':'未连接'));
  document.querySelector('.app-shell').classList.toggle('reconnecting',pendingMode!==null);
  enableHapticControls();
  updateSummary();
}
async function run(fn){
  if(busy)return;busy=true;enableControls();
  try{while(polling)await delay(10);await fn();}
  catch(error){announce('操作未完成',error.message||String(error),'error');log(error.message||String(error));}
  finally{busy=false;enableControls();}
}
async function receive(id){return featurePayload(await device.receiveFeatureReport(id),id);}
async function send(id,bytes){const size=id===0xfb?127:63;if(bytes.length>size)throw Error('报告过长');const packet=new Uint8Array(size);packet.set(bytes);await device.sendFeatureReport(id,packet);}
function clearReadState(){original=null;remap=null;diagnostic=null;motionDiagnostic=null;resetLiveStatus();resetMotion();$('mapping').replaceChildren();$('map-empty').hidden=false;}

function enableHapticControls(){
  const ready=!!device?.opened&&!!original&&!busy&&!!(diagnostic?.capabilities&64);
  const playable=ready&&[3,4].includes(activeMode())&&diagnostic?.connected&&motionDiagnostic?.connected&&motionDiagnostic.fresh;
  for(const id of ['hd-left','hd-right','hd-both'])$(id).disabled=!playable;
  $('hd-stop').disabled=!ready;
}
function resetMotion(){
  $('motion-state').textContent='等待诊断';$('imu-host-state').textContent='—';
  $('gyro-data').textContent=$('accel-data').textContent=$('native-motion').textContent='—';
  $('motion-samples').textContent=$('host-rumble-count').textContent=$('haptic-bt-count').textContent=$('haptic-bt-failures').textContent='—';
  $('haptic-test-state').textContent='测试不会保存设置，600 毫秒后自动停止。';
  enableHapticControls();
}
function renderMotion(m){
  motionDiagnostic=m;
  const vector=(xs,scale)=>xs.map(x=>(x/scale).toFixed(2)).join(' · ');
  $('gyro-data').textContent=vector(m.raw.slice(0,3),16)+' °/s';
  $('accel-data').textContent=vector(m.raw.slice(3,6),8192)+' g';
  $('native-motion').textContent=m.native.join(' · ');
  $('motion-samples').textContent=String(m.samples);
  $('imu-host-state').textContent=m.imuEnabled?(m.fullReport?'已启用 · 完整体感报告':'已启用，但当前报告不含体感'):'应用尚未开启 IMU';
  $('motion-state').textContent=!m.connected?'手柄未连接':!m.fresh?'蓝牙输入已过期':m.imuEnabled&&m.fullReport?'体感输出中':'源数据可读 · 等待应用开启';
  $('host-rumble-count').textContent=String(m.rumblePackets);
  $('haptic-bt-count').textContent=String(m.btPackets);$('haptic-bt-failures').textContent=String(m.btFailures);
  $('haptic-test-state').textContent=m.testActive?`本地试振进行中 · 剩余 ${m.remaining} ms`:
    activeMode()===6?'Pro 2 暂无完整 HD Rumble 2；本地 HD 试振请切换纯 Pro 或 Pro+麦克风。':'测试不会保存设置，600 毫秒后自动停止。';
  enableHapticControls();
}
async function readMotion(){
  if(!(diagnostic?.capabilities&64)){
    motionDiagnostic=null;resetMotion();$('motion-state').textContent=[3,4,6].includes(activeMode())?'此固件无扩展诊断 · 请更新 r7':'请选择 Switch 模式查看扩展诊断';return;
  }
  renderMotion(decodeMotionStatus(await receive(0xfa)));
}
for(const [id,pattern] of [['hd-left',1],['hd-right',2],['hd-both',3],['hd-stop',0]]){
  $(id).onclick=()=>run(async()=>{
    if(!(diagnostic?.capabilities&64))throw Error('本地体感与试振需要 r7 固件。');
    if(pattern&&(![3,4].includes(activeMode())||!diagnostic.connected))throw Error('请连接手柄，并切换纯 Pro 或 Pro+麦克风。');
    await send(0xf6,new Uint8Array([0x10,pattern,0x58,2]));
    await delay(45);await readMotion();
    if(pattern&&!motionDiagnostic?.testActive)throw Error('设备未确认本地试振，请检查蓝牙连接并重试。');
    announce(pattern?'本地短时试振已启动':'本地试振已停止',pattern?'请感受对应侧的震动。此测试独立于模拟器，不等于游戏已发出 HD 震动；600 毫秒后自动结束。':'没有保存或修改输出模式。','success');
  });
}

function resetLiveStatus(){
  updateInput([128,128,128,128,0,0,0,8,0,0]);
  $('live-badge').textContent='等待连接';$('live-badge').className='mini-badge';
  $('active-mode').textContent='尚未连接';$('mode-detail').textContent='连接后读取运行模式';
  $('bt-state').textContent='未连接';$('input-age').textContent='等待接收器连接';
  $('battery').textContent='—';$('rssi').textContent='—';$('receiver-hint').textContent='连接后查看接收器闪灯说明。';
  for(const id of ['disconnect-reason','disconnect-battery','disconnect-count'])$(id).textContent='—';
}
function updateInput(raw){
  if(!raw||raw.length<10){$('live-badge').textContent='旧固件无实时诊断';return;}
  const hat=raw[7]&15;
  const pressed=[!!(raw[7]&16),!!(raw[7]&32),!!(raw[7]&64),!!(raw[7]&128),...Array.from({length:8},(_,i)=>!!(raw[8]&(1<<i))),!!(raw[9]&1),!!(raw[9]&2),!!(raw[9]&4),[0,1,7].includes(hat),[3,4,5].includes(hat),[5,6,7].includes(hat),[1,2,3].includes(hat)];
  document.querySelectorAll('[data-button]').forEach(b=>b.classList.toggle('active',pressed[Number(b.dataset.button)]));
  $('stick-left').setAttribute('cx',String(184+(raw[0]-128)*.105));$('stick-left').setAttribute('cy',String(183+(raw[1]-128)*.105));
  $('stick-right').setAttribute('cx',String(376+(raw[2]-128)*.105));$('stick-right').setAttribute('cy',String(183+(raw[3]-128)*.105));
  const axis=x=>((x-128)/128).toFixed(2);
  $('left-axis').textContent=`X ${axis(raw[0])} · Y ${axis(raw[1])}`;$('right-axis').textContent=`X ${axis(raw[2])} · Y ${axis(raw[3])}`;
  $('trigger-values').textContent=`${Math.round(raw[4]/255*100)}% / ${Math.round(raw[5]/255*100)}%`;
}
function renderStatus(s){
  diagnostic=s;const mode=activeMode();
  $('active-mode').textContent=mode===null?'尚未连接':labels[mode]||'未知模式';
  $('mode-detail').textContent=s.extended?(mode===6?'Pro 2 实验 · 输入 / 体感':mode===5?'XInput · 模拟扳机 / 音频触觉':s.reportMode===0x3f?'0x3F · 浏览器标准输入':s.reportMode===0x30?'0x30 · 完整 Pro 输入':'原生 DualSense 输入'):'旧固件 · 状态信息有限';
  const connected=s.extended?s.connected:s.battery!==null;
  $('bt-state').textContent=connected?'已连接':'等待配对';
  $('input-age').textContent=s.extended&&connected?`最近输入 ${s.inputAge} ms 前`:'手柄按住 PS + Create 进入配对';
  $('battery').replaceChildren(document.createTextNode(s.battery===null?'—':String(s.battery)));const percent=document.createElement('em');percent.textContent='%';$('battery').append(percent);
  $('battery-detail').textContent=s.battery===null?'等待手柄电量':s.batteryState===1?'正在充电':s.batteryState===2?'充电完成':'电池供电';
  $('rssi').replaceChildren(document.createTextNode(connected?String(s.rssi):'—'));const dbm=document.createElement('em');dbm.textContent='dBm';$('rssi').append(dbm);
  $('signal-detail').textContent=!connected?'等待蓝牙连接':s.rssi>-60?'信号良好':s.rssi>-80?'信号一般':'信号较弱';
  $('live-badge').textContent=s.extended?(connected?'输入监测中':'等待蓝牙输入'):'升级固件启用实时监测';$('live-badge').className=`mini-badge ${s.extended&&connected?'live':''}`;
  if(s.extended)updateInput(connected?s.input:[128,128,128,128,0,0,0,8,0,0]);
  $('report-value').textContent=s.extended?`0x${s.reportMode.toString(16).toUpperCase()}`:'旧固件未提供';
  $('commands-value').textContent=s.extended?String(s.commands):'—';$('reports-value').textContent=s.extended?String(s.reports):'—';$('replies-value').textContent=s.extended?String(s.replies):'—';
  $('save-value').textContent=s.extended?['未确认 / 待保存','正在写入闪存','已写入闪存','闪存保存失败'][s.saveStatus]||'未知':'旧固件无回执';
  $('receiver-hint').textContent=!connected?'手柄尚未连接。LCTech 的红色电源灯常亮，蓝色状态灯闪烁会看起来红紫交替；这也可能是等待连接，不能单凭灯色判断掉线。':
    s.batteryState===0&&s.battery!==null&&s.battery<=10?'手柄已连接，但电量不超过 10%：接收器会闪灯警告，请先充电。':
    s.extended&&s.inputAge>1000?'蓝牙状态为已连接，但输入已超过 1 秒未更新，请保留断线历史并导出诊断。':
    '手柄已连接。红紫交替也可能来自板载状态灯；是否掉线请结合输入更新时间和断开次数判断。';
  const history=s.linkHistory;
  $('disconnect-reason').textContent=!history?'需 r9 固件':!history.disconnects?'本次上电未记录断线':
    ({8:'蓝牙连接超时',19:'对端终止连接',22:'本机终止 / 强制清理'}[history.reason]||
     (history.reason===null?'原因未知':`HCI 0x${history.reason.toString(16).padStart(2,'0')}`));
  $('disconnect-battery').textContent=history?.disconnects?(history.battery===null?'未取得断线前电量':`${history.battery}% · ${history.batteryState===0?'电池供电':history.batteryState===1?'正在充电':'状态 '+history.batteryState}`):'—';
  $('disconnect-count').textContent=history?`${history.disconnects} 次（接收器本次上电）`:'需 r9 固件';
  $('mic-state').textContent=s.extended&&s.flags&2?'主机已打开音频输入':mode===4?'已启用 USB 麦克风接口':'未打开麦克风流';
  $('mic-frames').textContent=s.extended?String(s.micFrames):'—';$('mic-device-level').textContent=s.extended?`${Math.round((s.flags&4?s.micPeak:0)/32768*100)}%`:'—';
}
async function read(){
  const generation=++readGeneration;clearReadState();
  const cfg=await receive(0xf7);decode(cfg);
  const version=new TextDecoder().decode(await receive(0xf8)).replace(/\0.*$/s,'');
  const state=decodeStatus(await receive(0xf9));
  if(generation!==readGeneration)throw Error('读取期间设备已经断开，请重新连接。');
  original=cfg;firmware=version;renderStatus(state);showConfig(cfg);failures=0;
  $('device').textContent=`${device.productName||'DS5 接收器'} · ${version}`;
  $('firmware-value').textContent=version;
  const hex=x=>Number(x||0).toString(16).padStart(4,'0').toUpperCase();
  $('usb-value').textContent=`${hex(device.vendorId)} : ${hex(device.productId)}`;
  $('hid-value').textContent=supportsConfig(device)?'F6 / F7 / F8 / F9 可访问':'未找到配置报告';
  if(!state.extended||!(state.capabilities&1))announce('接收器已连接 · 建议升级固件','当前固件缺少 Pro 收发端点修复和实时诊断。请更新到 alpha.3 固件后使用 Switch Pro。','warning');
  else announce('接收器已连接',`当前模式：${labels[state.activeMode]}。设置保存后会自动判断是否需要重新枚举。`,'success');
  log(`已连接 ${version} / ${labels[activeMode()]}`);
}
async function connectTo(selected){
  clearReadState();
  if(device?.opened&&device!==selected){try{await device.close();}catch{/* Previous enumeration is gone. */}}
  device=selected;if(!device.opened)await device.open();await read();
  const expected=pendingMode;pendingMode=null;clearTimeout(reconnectTimer);
  const matched=expected===null||activeMode()===expected;
  if(!matched)announce('设备重新连接，但模式与目标不同','请查看连接诊断并导出记录；本次没有继续写入配置。','warning');
  return matched;
}
$('connect').onclick=()=>run(async()=>{
  if(!navigator.hid)throw Error('当前浏览器不支持 WebHID。请使用桌面 Chrome / Edge 打开本地配置页。');
  // Windows exposes Pro gameplay and configuration as separate HID TLCs.
  // Only offer the vendor configuration collection for Nintendo identity;
  // selecting the gameplay collection cannot read the F7/F8 feature reports.
  const devices=await navigator.hid.requestDevice({filters:[{vendorId:0x054c,productId:0x0ce6},{vendorId:0x054c,productId:0x0df2},{vendorId:0x057e,productId:0x2009,usagePage:0xff00,usage:1},{vendorId:0x045e,productId:0x028e,usagePage:0xff00,usage:1},{vendorId:0x1209,productId:0xdb05,usagePage:0xff00,usage:1},{vendorId:0x057e,productId:0x2069,usagePage:0xff00,usage:1}]});
  const selected=devices.find(supportsConfig);
  if(!selected)throw Error('未选择配置接口。Pro 模式请选择带 F7 配置报告的接收器接口，原装直连手柄不提供此接口。');
  await connectTo(selected);
});
$('refresh').onclick=()=>run(read);

let autoConnecting=false;
async function autoConnect(){
  if(autoConnecting||busy||polling||pendingMode!==null||(device?.opened&&original)||!navigator.hid?.getDevices)return;
  autoConnecting=true;
  try{
    const granted=await navigator.hid.getDevices();
    for(const candidate of granted.filter(supportsConfig)){
      if(busy||polling||pendingMode!==null||(device?.opened&&original))break;
      busy=true;enableControls();
      try{await connectTo(candidate);break;}
      catch{try{await candidate.close();}catch{}clearReadState();}
      finally{busy=false;enableControls();}
    }
  }catch{/* Retry when permission or availability changes. */}
  finally{autoConnecting=false;}
}
setInterval(autoConnect,2000);

async function reconnectScan(){
  if(pendingMode===null)return;
  if(Date.now()>reconnectUntil){announce('等待新模式授权','若浏览器尚未授权此 USB 身份，请点击右上角「授权新模式接收器」。','warning');enableControls();return;}
  if(!busy&&!polling&&navigator.hid?.getDevices){
    const granted=await navigator.hid.getDevices().catch(()=>[]);
    const selected=granted.find(d=>supportsConfig(d)&&modeIdentity(pendingMode,d));
    if(selected){await run(async()=>{if(await connectTo(selected))announce('模式已应用',`已重新连接：${labels[activeMode()]}。${activeMode()===6?'Pro 2 为实验协议，主机兼容与耳麦音频尚未通过。':'无需手动拔插接收器。'}`,'success');});if(pendingMode===null)return;}
  }
  reconnectTimer=setTimeout(reconnectScan,500);
}
async function save(forceReconnect=false){
  const next=encode(original,formValues());
  if([3,4].includes(next[12])&&(!diagnostic?.extended||!(diagnostic.capabilities&1)))throw Error('当前固件缺少 Pro USB 修复，请先更新到 alpha.3 固件。');
  if(next[12]===6&&!(diagnostic?.capabilities&32))throw Error('此固件未提供 Pro 2 实验协议，请先刷入 r5 或更新固件。');
  if(next[12]===5&&!(diagnostic?.capabilities&8))throw Error('当前固件未提供 Xbox / XInput，请先刷入 r4 或更新固件。');
  if(next[12]===4&&!(diagnostic.capabilities&4))throw Error('此固件未提供 Pro + USB 麦克风接口。');
  const reconnect=forceReconnect||mustReconnect(next);
  await send(0xf6,new Uint8Array([1,...next.slice(0,29)]));
  const actual=await receive(0xf7);decode(actual);
  if(next.slice(0,29).some((v,i)=>actual[i]!==v))throw Error('设备回读与写入不符，本次未发送闪存保存命令。');
  await send(0xf6,new Uint8Array([2]));
  if(diagnostic?.extended){
    let saved=false;
    for(let i=0;i<30;i++){await delay(60);const s=decodeStatus(await receive(0xf9));renderStatus(s);if(s.saveStatus===3)throw Error('接收器写入闪存失败，未重启。请导出诊断。');if(s.saveStatus===2){saved=true;break;}}
    if(!saved)throw Error('等待闪存保存确认超时，未发送重启命令。');
  }
  original=actual;
  if(!reconnect){announce('设置已保存',diagnostic?.extended?'闪存保存已确认。':'保存命令已发送；旧固件不提供写入回执。','success');log('设置保存完成');return;}
  pendingMode=next[12];reconnectUntil=Date.now()+20000;
  log(`设置已保存，切换至 ${labels[pendingMode]}，等待重新枚举`);
  announce('设置已保存 · 正在应用模式','接收器会重启并重新枚举 USB。已授权的身份将自动连接；新身份需要在浏览器中授权。');
  try{await send(0xf6,new Uint8Array([3]));}
  catch(error){if(device?.opened){pendingMode=null;throw error;}}
  clearReadState();reconnectTimer=setTimeout(reconnectScan,700);
}
$('settings').onsubmit=e=>{e.preventDefault();run(()=>save(false));};
$('reboot').onclick=()=>{if($('settings').reportValidity())run(()=>save(true));};
function download(name,data,type='application/json'){
  const blob=new Blob([typeof data==='string'?data:JSON.stringify(data,null,2)],{type});const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
}
$('export').onclick=()=>run(async()=>{const bytes=encode(original,formValues());download('ds5-config.json',{format:'ds5-local-v1',bytes:[...bytes]});});
$('import').onchange=e=>run(async()=>{
  const file=e.target.files[0];if(!file)return;if(file.size>8192)throw Error('配置文件过大');
  const json=JSON.parse(await file.text());
  if(json.format!=='ds5-local-v1'||!Array.isArray(json.bytes)||json.bytes.some(x=>!Number.isInteger(x)||x<0||x>255))throw Error('无效的配置文件');
  showConfig(encode(original,decode(new Uint8Array(json.bytes))));announce('配置已载入表单','点击「保存并应用」后才会写入接收器。');
});
$('diagnostic-export').onclick=()=>download('ds5-diagnostics.json',{app:'alpha.12',time:new Date().toISOString(),firmware,usb:device?{vendorId:device.vendorId,productId:device.productId,productName:device.productName}:null,config:original?decode(original):null,diagnostic,motionDiagnostic,events});

$('readmap').onclick=()=>run(async()=>{
  await send(0xfb,new Uint8Array([3,Number($('profile').value)]));
  const bytes=await receive(0xfb);if(bytes.length<126||bytes[1]!==Number($('profile').value))throw Error('映射报告不完整或配置编号不符');
  remap=bytes.slice(2,126);$('mapping').replaceChildren();$('map-empty').hidden=true;
  names.forEach((name,i)=>{const row=document.createElement('label');row.append(name);const select=document.createElement('select');select.id=`map${i}`;names.forEach((n,j)=>select.add(new Option(n,String(j))));const type=remap[i*4];if(type!==0)select.add(new Option(`保留已有${type===1?'键盘':'鼠标'}映射`,'keep'));select.value=type===0?String(remap[i*4+1]):'keep';row.append(select);$('mapping').append(row);});
  announce('映射已读取','修改游戏手柄目标会替换该行原有映射。键盘/鼠标输出仅在 DS 模式有效。');
});
$('profile').onchange=()=>{remap=null;$('mapping').replaceChildren();$('map-empty').hidden=false;enableControls();};
$('savemap').onclick=()=>run(async()=>{const table=remap.slice();for(let i=0;i<31;i++){const value=$(`map${i}`).value;if(value!=='keep')table.set([0,Number(value),0,table[i*4+3]&1],i*4);}await send(0xfb,new Uint8Array([1,Number($('profile').value),...table]));announce('映射保存命令已发送','映射写入不需要切换 USB 身份。','success');log('映射保存命令已发送');});
$('activemap').onclick=()=>run(async()=>{await send(0xfb,new Uint8Array([5,Number($('profile').value)]));announce('活动映射已切换',`当前使用配置 ${Number($('profile').value)+1}。`,'success');});

navigator.hid?.addEventListener('disconnect',e=>{
  if(e.device!==device)return;++readGeneration;stopAudio();clearReadState();enableControls();
  if(pendingMode===null){announce('USB 接收器已断开','重新接入后将自动连接；新模式身份首次使用需点击授权。','warning');log('USB 设备断开');}
});
navigator.hid?.addEventListener('connect',()=>{if(pendingMode!==null){clearTimeout(reconnectTimer);reconnectTimer=setTimeout(reconnectScan,150);}else{autoConnect();}});
setInterval(async()=>{
  if(busy||polling||!original||!device?.opened||pendingMode!==null)return;
  polling=true;
  try{
    const s=decodeStatus(await receive(0xf9));renderStatus(s);failures=0;
    if(document.querySelector('[data-panel="diagnostics"]').classList.contains('active')){
      try{await readMotion();}catch(error){motionDiagnostic=null;resetMotion();$('motion-state').textContent=error.message;enableHapticControls();}
    }
  }
  catch{if(++failures>=3){clearReadState();enableControls();announce('状态读取中断','设备暂时不可访问，请重新连接并读取。','warning');}}
  finally{polling=false;}
},180);

async function audioDevices(){
  const devices=(await navigator.mediaDevices.enumerateDevices()).filter(d=>d.kind==='audioinput'&&!['default','communications'].includes(d.deviceId));
  $('audio-device').replaceChildren(new Option('请选择音频输入设备',''));
  devices.forEach((d,i)=>$('audio-device').add(new Option(d.label||`音频输入 ${i+1}`,d.deviceId)));
  const controller=devices.find(d=>/DualSense|Pro Controller|DS5|Sony/i.test(d.label));
  if(controller)$('audio-device').value=controller.deviceId;
  $('audio-help').textContent=controller?'已找到名称匹配的手柄输入，请点击开始测试。仅在本机处理音频。':'未自动找到手柄名称。请手动确认输入设备；Pro 模式需要先启用「Pro + USB 麦克风」。';
}
$('grant-audio').onclick=async()=>{
  try{if(!navigator.mediaDevices?.getUserMedia)throw Error('此浏览器没有可用的音频接口');const permission=await navigator.mediaDevices.getUserMedia({audio:true,video:false});permission.getTracks().forEach(t=>t.stop());await audioDevices();}
  catch(e){$('audio-help').textContent=`无法获取音频设备：${e.message}`;}
};
function stopAudio(){
  ++audioSession;
  cancelAnimationFrame(audioAnimation);audioAnimation=0;
  audioStream?.getTracks().forEach(t=>t.stop());audioStream=null;
  if(audioContext){audioContext.close().catch(()=>{});audioContext=null;}
  $('stop-audio').disabled=true;$('start-audio').disabled=false;
  $('mic-level').style.width='0%';$('mic-level-value').textContent='0%';$('mic-test-label').textContent='本地测试已停止';
}
$('start-audio').onclick=async()=>{
  let session;
  try{
    const selected=$('audio-device').value;if(!selected)throw Error('请先授权并选择手柄的音频输入，避免误测电脑麦克风。');
    stopAudio();session=audioSession;$('start-audio').disabled=true;$('stop-audio').disabled=false;
    const stream=await navigator.mediaDevices.getUserMedia({audio:{deviceId:{exact:selected}},video:false});
    if(session!==audioSession){stream.getTracks().forEach(t=>t.stop());return;}
    audioStream=stream;
    audioContext=new AudioContext();const source=audioContext.createMediaStreamSource(audioStream),analyser=audioContext.createAnalyser();analyser.fftSize=256;source.connect(analyser);
    const buffer=new Uint8Array(analyser.fftSize);$('mic-test-label').textContent=$('audio-device').selectedOptions[0].textContent;$('stop-audio').disabled=false;$('start-audio').disabled=true;
    const tick=()=>{analyser.getByteTimeDomainData(buffer);let sum=0;for(const v of buffer)sum+=((v-128)/128)**2;const level=Math.min(100,Math.round(Math.sqrt(sum/buffer.length)*220));$('mic-level').style.width=`${level}%`;$('mic-level-value').textContent=`${level}%`;audioAnimation=requestAnimationFrame(tick);};tick();
  }catch(e){if(session===undefined||session===audioSession){stopAudio();$('audio-help').textContent=`测试未开始：${e.message}`;}}
};
$('stop-audio').onclick=stopAudio;
window.addEventListener('pagehide',stopAudio);
document.addEventListener('visibilitychange',()=>{if(document.hidden&&audioStream)stopAudio();});
enableControls();
if(!navigator.hid)announce('浏览器不支持 WebHID','请使用桌面 Chrome / Edge 打开本机配置地址。','warning');
else autoConnect();
