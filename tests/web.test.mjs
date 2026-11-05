import test from 'node:test';
import assert from 'node:assert/strict';
import {fields,decode,encode,featurePayload,supportsConfig} from '../web/config.js';
function config(){const bytes=new Uint8Array(63);bytes[0]=2;for(const [,,offset,min] of fields)bytes[offset]=min;new DataView(bytes.buffer).setFloat32(1,1,true);bytes[20]=1;bytes[50]=137;return bytes;}
test('packed config preserves unknown fields, dse_detected and tail',()=>{const bytes=config();const values=decode(bytes);values.controller_mode=3;values.haptics_gain=1.75;const encoded=encode(bytes,values);assert.equal(encoded[12],3);assert.equal(encoded[20],1);assert.equal(encoded[50],137);assert.equal(decode(encoded).haptics_gain,1.75);assert.equal(bytes[12],0);});
test('reject unsupported layouts and unsafe numeric writes',()=>{assert.throws(()=>decode(new Uint8Array(29)));assert.throws(()=>decode(new Uint8Array([2])));const bytes=config();for(const v of [NaN,Infinity,4,-1,1.5])assert.throws(()=>encode(bytes,{...decode(bytes),controller_mode:v}));});
test('WebHID reports include ID and may have a buffer offset',()=>{const b=new Uint8Array([99,0xf7,2,1,99]);assert.deepEqual([...featurePayload(new DataView(b.buffer,1,3),0xf7)],[2,1]);assert.throws(()=>featurePayload(new DataView(b.buffer),0xf7));});
test('exclude original controllers without config features',()=>{assert.equal(supportsConfig({collections:[{featureReports:[{reportId:5}]}]}),false);assert.equal(supportsConfig({collections:[{featureReports:[{reportId:0xf7}]}]}),true);});
