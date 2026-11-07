import test from 'node:test';
import assert from 'node:assert/strict';
import {fields,decode,encode,featurePayload,supportsConfig,decodeStatus,decodeMotionStatus} from '../web/config.js';
function config(){const bytes=new Uint8Array(63);bytes[0]=2;for(const [,,offset,min] of fields)bytes[offset]=min;new DataView(bytes.buffer).setFloat32(1,1,true);bytes[20]=1;bytes[50]=137;return bytes;}
test('packed config preserves unknown fields, dse_detected and tail',()=>{const bytes=config();const values=decode(bytes);values.controller_mode=3;values.haptics_gain=1.75;const encoded=encode(bytes,values);assert.equal(encoded[12],3);assert.equal(encoded[20],1);assert.equal(encoded[50],137);assert.equal(decode(encoded).haptics_gain,1.75);assert.equal(bytes[12],0);});
test('reject unsupported layouts and unsafe numeric writes',()=>{assert.throws(()=>decode(new Uint8Array(29)));assert.throws(()=>decode(new Uint8Array([2])));const bytes=config();for(const v of [NaN,Infinity,7,-1,1.5])assert.throws(()=>encode(bytes,{...decode(bytes),controller_mode:v}));assert.equal(encode(bytes,{...decode(bytes),controller_mode:4})[12],4);assert.equal(encode(bytes,{...decode(bytes),controller_mode:5})[12],5);assert.equal(encode(bytes,{...decode(bytes),controller_mode:6})[12],6);});
test('WebHID reports include ID and may have a buffer offset',()=>{const b=new Uint8Array([99,0xf7,2,1,99]);assert.deepEqual([...featurePayload(new DataView(b.buffer,1,3),0xf7)],[2,1]);assert.throws(()=>featurePayload(new DataView(b.buffer),0xf7));});
test('exclude original controllers without config features',()=>{assert.equal(supportsConfig({collections:[{featureReports:[{reportId:5}]}]}),false);assert.equal(supportsConfig({collections:[{featureReports:[{reportId:0xf7}]}]}),true);});
test('decode versioned device diagnostics and retain legacy support',()=>{
 const legacy=new Uint8Array(13);legacy[11]=255;assert.equal(decodeStatus(legacy).battery,null);assert.equal(decodeStatus(legacy).extended,false);
 const b=new Uint8Array(53);const v=new DataView(b.buffer);b[0]=206;b[11]=80;b[13]=0xd5;b[14]=2;b[15]=4;b[16]=1;b[17]=0x30;b[18]=7;v.setUint32(19,10,true);v.setUint32(23,500,true);v.setUint32(27,100,true);b[31]=128;v.setUint16(41,16384,true);b[51]=7;b[52]=2;
 const d=decodeStatus(b);assert.equal(d.rssi,-50);assert.equal(d.activeMode,4);assert.equal(d.connected,true);assert.equal(d.commands,10);assert.equal(d.reports,500);assert.equal(d.micFrames,100);assert.equal(d.micPeak,16384);assert.equal(d.saveStatus,2);assert.equal(d.input[0],128);assert.throws(()=>decodeStatus(b.slice(0,12)));
});

test('decode independent motion, host activation and bounded test diagnostics',()=>{
 const b=new Uint8Array(63);const v=new DataView(b.buffer);b[0]=0xa8;b[1]=1;b[2]=63;b[3]=0x30;b[4]=2;
 for(let i=0;i<6;i++){v.setInt16(5+i*2,(i-3)*819,true);v.setInt16(17+i*2,(i+1)*12,true);}
 v.setUint32(33,42,true);v.setUint32(37,900,true);v.setUint32(45,3,true);v.setUint32(49,500,true);v.setUint16(61,600,true);
 const m=decodeMotionStatus(b);assert.equal(m.connected,true);assert.equal(m.imuEnabled,true);assert.equal(m.fullReport,true);assert.equal(m.testActive,true);assert.equal(m.testPattern,2);assert.equal(m.raw[0],-2457);assert.equal(m.native[5],72);assert.equal(m.rumblePackets,42);assert.equal(m.btPackets,900);assert.equal(m.btFailures,3);assert.equal(m.remaining,600);
 assert.throws(()=>decodeMotionStatus(b.slice(0,62)));b[1]=2;assert.throws(()=>decodeMotionStatus(b));
});
