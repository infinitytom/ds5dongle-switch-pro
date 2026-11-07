// Byte offsets come from the packed config_body in src/config.h (version 2).
export const fields = [
 ['haptics_gain','触觉强度',1,1,2,.05,'float'],
 ['controller_mode','USB 输出模式',12,0,6,1,['DualSense','DualSense Edge','自动识别 DS / Edge','Switch Pro','Switch Pro + USB 麦克风（实验）','Xbox 360 / XInput','Switch 2 Pro（实验，无耳麦音频）']],
 ['polling_rate_mode','DS 模式上报率',10,0,2,1,['250 Hz','500 Hz','实时']],
 ['inactive_time','空闲断开（分钟，0 为关闭）',8,0,60],
 ['speaker_volume','扬声器音量',5,0,127],['headset_volume','耳机音量',6,0,127],
 ['speaker_gain','扬声器增益',7,0,7],['audio_buffer_length','音频缓冲深度',11,16,128],
 ['disable_led','板载 LED 一分钟后熄灭',9,0,1,1,'bool'],
 ['enable_usb_sn','启用 USB 序列号',13,0,1,1,'bool'],
 ['ps_shortcut_enabled','PS 键电脑快捷键（DS 模式）',14,0,1,1,'bool'],
 ['disable_mic','禁用麦克风',15,0,1,1,'bool'],['disable_speaker','禁用扬声器',16,0,1,1,'bool'],
 ['enable_wake','USB 远程唤醒（DS 模式）',17,0,1,1,'bool'],
 ['trigger_reduce','自适应扳机功率降低',18,0,10],['lock_volume','锁定音量',19,0,1,1,'bool'],
 ['usb_stealth','蓝牙连接前隐藏 USB',21,0,1,1,'bool'],
 ['led_r','灯条红色',22,0,255],['led_g','灯条绿色',23,0,255],['led_b','灯条蓝色',24,0,255],
 ['tp_mode','触摸板模式（DS 模式）',25,0,4,1,['关闭','方向键','左鼠标 / 右方向','左方向 / 右鼠标','分区']],
 ['tp_mode_enabled_mask','触摸板可轮换模式位掩码',26,1,31],
 ['tp_mouse_sensitivity','触摸板鼠标灵敏度',27,1,32],
 ['audio_haptic','音频转触觉（DS / Xbox）',28,0,2,1,['关闭','自动（游戏优先）','强制']]
];
export function decode(bytes) {
 if (bytes.length < 29 || bytes[0] !== 2) throw Error('配置布局不受支持：需要 version 2、至少 29 字节；未写入设备。');
 const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
 return Object.fromEntries(fields.map(([key,,offset,,,,type]) => [key,type==='float'?view.getFloat32(offset,true):bytes[offset]]));
}
export function encode(original, values) {
 decode(original);
 const bytes = original.slice(); const view = new DataView(bytes.buffer);
 for (const [key,label,offset,min,max,step=1,type] of fields) {
   const value = Number(values[key]);
   if (!Number.isFinite(value) || value < min || value > max || (type !== 'float' && !Number.isInteger(value))) throw Error(`${label}：请输入 ${min}–${max} 内的有效值`);
   if(type==='float') view.setFloat32(offset,value,true); else bytes[offset]=value;
 }
 return bytes;
}
export function featurePayload(view,id) {
 const bytes=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);
 if(bytes[0]!==id) throw Error(`设备报告格式不符（预期 0x${id.toString(16)}）`);
 return bytes.slice(1);
}
export function supportsConfig(device) {
 return device.collections?.some(c=>c.featureReports?.some(r=>r.reportId===0xf7));
}

export function decodeStatus(bytes) {
 if(bytes.length<13)throw Error('状态报告不完整');
 const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
 const result={rssi:v.getInt8(0),battery:bytes[11]===255?null:bytes[11],batteryState:bytes[12],extended:false};
 if(bytes.length>=53&&bytes[13]===0xd5&&bytes[14]>=2){
   Object.assign(result,{extended:true,activeMode:bytes[15],connected:!!bytes[16],reportMode:bytes[17],flags:bytes[18],
     commands:v.getUint32(19,true),reports:v.getUint32(23,true),micFrames:v.getUint32(27,true),
     input:[...bytes.slice(31,41)],micPeak:v.getUint16(41,true),inputAge:v.getUint32(43,true),
     replies:v.getUint32(47,true),capabilities:bytes[51],saveStatus:bytes[52]});
 }
 return result;
}

export function decodeMotionStatus(bytes) {
 if(bytes.length<63||bytes[0]!==0xa8||bytes[1]!==1)throw Error('体感诊断报告不受支持，请更新到 r7 固件。');
 const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),flags=bytes[2];
 return {connected:!!(flags&1),imuEnabled:!!(flags&2),vibrationEnabled:!!(flags&4),testActive:!!(flags&8),fresh:!!(flags&16),fullReport:!!(flags&32),
   reportMode:bytes[3],testPattern:bytes[4],raw:Array.from({length:6},(_,i)=>v.getInt16(5+i*2,true)),native:Array.from({length:6},(_,i)=>v.getInt16(17+i*2,true)),
   inputAge:v.getUint32(29,true),rumblePackets:v.getUint32(33,true),btPackets:v.getUint32(37,true),rumbleAge:v.getUint32(41,true),
   btFailures:v.getUint32(45,true),samples:v.getUint32(49,true),testRequests:v.getUint32(53,true),commands:v.getUint32(57,true),remaining:v.getUint16(61,true)};
}
