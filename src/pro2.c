/* Switch 2 USB command and report layout adapted from Alia5/VIIPER
 * (Copyright (C) 2025-2026 Peter Repukat, GPL-3.0-or-later).
 * C transport, DS conversion and validation are part of this project.
 * SDL's Switch2 driver is the interoperability reference, not console proof. */
#include "pro2.h"
#include "xbox.h"
#include <string.h>
#include <math.h>

static void put16(uint8_t *p, int value) {
    if(value>32767) value=32767;
    if(value< -32768) value= -32768;
    p[0]=(uint8_t)value; p[1]=(uint8_t)((uint16_t)value>>8);
}
static void put32(uint8_t *p, uint32_t value) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(value>>(i*8)); }
static int get16(const uint8_t *p) { return (int16_t)(p[0]|p[1]<<8); }
static unsigned axis(uint8_t x) { return x<=128 ? x*16u : 2048u+(x-128u)*2047u/127u; }
static void stick(uint8_t *p, unsigned x, unsigned y) { p[0]=(uint8_t)x;p[1]=(uint8_t)((x>>8)|(y<<4));p[2]=(uint8_t)(y>>4); }

void pro2_init(struct pro2_state *s) { memset(s,0,sizeof(*s));s->mode=9;s->streaming=true;s->mask=0x27; }
unsigned pro2_command(struct pro2_state *s, const uint8_t *p, unsigned len, uint8_t reply[80])
{
    if(!p || len<8 || len>64 || p[1]!=0x91 || p[6] || p[7] || p[5]>56 || len!=8u+p[5]) return 0;
    /* Every mutation below requires the full command-specific payload. */
    uint8_t cmd=p[0], sub=p[3];
    if(cmd==2 && (sub!=1 || len!=16)) return 0;
    if(cmd==3 && (sub==3 || sub==10) && len!=12) return 0;
    if(cmd==3 && sub==13 && len!=16) return 0;
    if(cmd==0x0c && len!=12) return 0;
    if(cmd==9 && (sub!=7 || len!=16)) return 0;
    memset(reply,0,80);reply[0]=cmd;reply[1]=1;reply[2]=p[2];reply[3]=sub;reply[4]=0x10;reply[5]=0x78;
    if(cmd==2) {
        uint32_t address=0;for(unsigned i=0;i<4;i++) address|=(uint32_t)p[12+i]<<(8*i);
        reply[8]=64;put32(reply+12,address);
        if(address==0x13000) memcpy(reply+18,"DS5-PRO2-00000000",16);
        if(address==0x13080 || address==0x130c0) {
            stick(reply+16+0x28,2048,2048);stick(reply+16+0x2b,2047,2047);stick(reply+16+0x2e,2048,2048);
        }
        /* Other readable calibration blocks have zero biases and no user magic. */
        return 80;
    }
    if(cmd==3) {
        if(sub==10) { if(p[8]!=5 && p[8]!=9) return 0;s->mode=p[8]; }
        if(sub==3 || sub==13) { s->streaming=p[8]!=0;reply[8]=1;return 12; }
        return 8;
    }
    if(cmd==0x0c) {
        uint8_t flags=p[8]&0x27;
        if(sub==1) { /* Buttons, sticks, IMU, classic rumble fallback; no mouse. */
            reply[12]=(flags&1)?7:0;reply[13]=(flags&2)?7:0;
            reply[14]=(flags&4)?1:0;reply[17]=(flags&0x20)?3:0;return 20;
        }
        if(sub==2) { s->mask=flags;s->features&=flags; }
        else if(sub==3) s->features=0;
        else if(sub==4) s->features|=flags&s->mask;
        else if(sub==5) s->features&=(uint8_t)~flags;
        else return 0;
        return 12;
    }
    if(cmd==1 || cmd==7 || cmd==8 || cmd==9 || cmd==10 || cmd==17) return 8;
    return 0; /* No fake ACK for authentication, audio or unimplemented commands. */
}
void pro2_report(struct pro2_state *s, const struct switch_pro *in, uint32_t us, uint8_t out[64])
{
    if(s->mode==9) {
        const uint8_t *d=in->ds;
        memset(out,0,64);out[0]=9;out[1]=(uint8_t)++s->counter;
        unsigned battery=d[52]&15;if(battery>9) battery=9;
        out[2]=(uint8_t)(battery<<2)|1;
        if((d[52]>>4)==1) out[2]|=2;
        /* 0x09 is the Pro 2 standard HID layout, not Pro 1's 0x30.
         * B,A,Y,X,R,ZR,+,R3; down,right,left,up,L,ZL,-,L3; HOME,Capture,GR,GL,C. */
        out[3]=((d[7]&0x20)?1:0)|((d[7]&0x40)?2:0)|
            ((d[7]&0x10)?4:0)|((d[7]&0x80)?8:0)|
            ((d[8]&2)?0x10:0)|((d[8]&8)?0x20:0)|
            ((d[8]&0x20)?0x40:0)|((d[8]&0x80)?0x80:0);
        unsigned hat=d[7]&15;
        if(hat<=7) {
            if(hat>=3 && hat<=5) out[4]|=1;
            if(hat>=1 && hat<=3) out[4]|=2;
            if(hat>=5) out[4]|=4;
            if(hat==7 || hat<=1) out[4]|=8;
        }
        out[4]|=((d[8]&1)?0x10:0)|((d[8]&4)?0x20:0)|
            ((d[8]&0x10)?0x40:0)|((d[8]&0x40)?0x80:0);
        out[5]=(d[9]&3)|((d[9]&4)?0x10:0);
        /* Standard HID fallback uses screen coordinates (Y down). Native
         * SDL/console-style 0x05 retains Nintendo's Y-up convention below. */
        stick(out+6,axis(d[0]),axis(d[1]));
        stick(out+9,axis(d[2]),axis(d[3]));
        out[12]=(s->features&0x20)?0x38:0x30;
        return;
    }
    const uint8_t *d=in->ds;
    uint8_t pro_report[64];switch_pro_report(in,us/1000,pro_report);
    memset(out,0,64);out[0]=5;put32(out+1,++s->counter);
    if(s->features&1) {
        out[5]=pro_report[3];out[6]=pro_report[4];out[7]=pro_report[5];
        if(d[9]&4) out[6]|=0x40; /* Mute -> C; no invented GL/GR inputs. */
    }
    unsigned y=d[1]?4096-axis(d[1]):4095;
    stick(out+11,(s->features&2)?axis(d[0]):2048,(s->features&2)?y:2048);
    y=d[3]?4096-axis(d[3]):4095;
    stick(out+14,(s->features&2)?axis(d[2]):2048,(s->features&2)?y:2048);
    put16(out+32,3800);out[34]=0;out[42]=1;
    if(s->features&4) {
        put32(out+43,us);
        /* Physical sensor units are converted directly from calibrated DS
         * coordinates into the axis convention consumed by SDL Switch2. */
        static const unsigned source[3]={0,2,1};
        static const int sign[3]={1,-1,1};
        for(unsigned i=0;i<3;i++) {
            unsigned j=source[i];
            float acc=(get16(d+21+j*2)-in->bias[3+j])*in->scale[3+j];
            float gyro=(get16(d+15+j*2)-in->bias[j])*in->scale[j];
            /* in->scale: Pro units, 4096/g and 14.285 counts/degree/s. */
            put16(out+49+i*2,(int)(sign[i]*acc));
            put16(out+55+i*2,(int)(sign[i]*gyro*(32767.0f*0.01745329252f/(34.8f*(13371.0f/936.0f)))));
        }
    }
}
bool pro2_rumble(const struct pro2_state *s, const uint8_t *p, unsigned n, uint8_t ds[48])
{
    if(!(s->features&0x20) || !p || n!=64 || p[0]!=2 || (p[1]&0xf0)!=0x50 || (p[17]&0xf0)!=0x50) return false;
    unsigned amps[2][2];
    for(unsigned c=0;c<2;c++) {
        const uint8_t *f=p+2+c*16;
        amps[c][0]=((unsigned)(f[1]&0xfc)<<4)|((unsigned)(f[2]&15)<<12);
        amps[c][1]=(f[3]&0xc0)|((unsigned)f[4]<<8);
    }
    /* Preserve left/right separation. The single-frame fallback folds bands
     * per side; it does not claim to reproduce HD Rumble 2's frequencies. */
    uint8_t packet[8]={0,8};
    for(unsigned c=0;c<2;c++) {
        unsigned amp=amps[c][0]>amps[c][1]?amps[c][0]:amps[c][1];
        packet[3+c]=(uint8_t)(amp>=29000?255:amp*255/29000);
    }
    return xbox_output(packet,8,ds);
}

static const uint8_t hid[]={
    /* Common 0x05 vendor packet plus standard 0x09 button/12bit stick packet. */
    5,1,9,5,0xa1,1,0x85,5,6,0,0xff,9,1,0x15,0,0x26,255,0,0x75,8,0x95,63,0x81,2,
    0x85,9,9,1,0x95,2,0x81,2,
    /* Keep Nintendo's wire bit order, but describe each bit with its proper
     * generic button usage. Chromium RawInput indexes Button usage N at N-1.
     * This lets standard-layout websites read L/R, triggers, dpad and menu
     * correctly while keeping Nintendo's button wire order. */
    5,9,9,1,9,2,9,3,9,4,9,6,9,8,9,10,9,12,
    9,14,9,16,9,15,9,13,9,5,9,7,9,9,9,11,9,17,9,18,9,20,9,21,9,19,
    0x25,1,0x95,21,0x75,1,0x81,2,0x95,1,0x75,3,0x81,3,
    /* Consecutive usages yield axes[0..3] in Chromium RawInput. */
    5,1,9,0x30,9,0x31,9,0x32,9,0x33,0x26,255,15,0x95,4,0x75,12,0x81,2,
    6,0,0xff,9,2,0x26,255,0,0x95,52,0x75,8,0x81,2,
    0x85,2,9,1,0x95,63,0x91,2,0xc0,
    /* Local configuration is a separate top-level collection. */
    6,0,0xff,9,1,0xa1,1,0x15,0,0x26,255,0,0x75,8,0x95,63,
    0x85,0xf6,9,1,0xb1,2,0x85,0xf7,9,2,0xb1,2,0x85,0xf8,9,3,0xb1,2,
    0x85,0xf9,9,4,0xb1,2,0x85,0xfa,9,6,0xb1,2,
    0x85,0xfb,9,5,0x95,127,0xb1,2,0xc0
};
static const uint8_t config[]={
    9,2,PRO2_CONFIG_LEN,0,2,1,0,0x80,250,
    9,4,0,0,2,3,0,0,0,
    9,0x21,0x11,1,0,1,0x22,sizeof(hid)&255,sizeof(hid)>>8,
    7,5,PRO2_EP_IN,3,64,0,4,7,5,PRO2_EP_OUT,3,64,0,4,
    9,4,1,0,2,0xff,0,0,0,
    7,5,PRO2_BULK_OUT,2,64,0,0,7,5,PRO2_BULK_IN,2,64,0,0
};
_Static_assert(sizeof(config)==PRO2_CONFIG_LEN,"Pro2 configuration length");
const uint8_t pro2_compat_id[40]={40,0,0,0,0,1,4,0,1,0,0,0,0,0,0,0,1,1,'W','I','N','U','S','B',0,0};
const uint8_t *pro2_hid_descriptor(uint16_t *len) { *len=sizeof(hid);return hid; }
const uint8_t *pro2_config_descriptor(bool hs,bool other)
{
    static uint8_t result[4][sizeof(config)];uint8_t *p=result[(hs?2:0)+(other?1:0)];
    memcpy(p,config,sizeof(config));p[1]=other?7:2;
    p[33]=p[40]=hs?6:4;
    if(hs) { p[54]=p[61]=0;p[55]=p[62]=2; } /* HS bulk MPS must be 512. */
    return p;
}

const uint8_t *pro2_os_properties(void)
{
    /* MS OS 1.0 extended properties for interface 1, REG_MULTI_SZ GUIDs. */
    static uint8_t bytes[146];
    static const char name[]="DeviceInterfaceGUIDs";
    static const char guid[]="{24B80B7B-65AF-4A3B-A26D-A1C9BEA75D22}";
    unsigned name_len=sizeof(name)*2, data_len=(sizeof(guid)+1)*2;
    unsigned section=14+name_len+data_len, total=10+section;
    memset(bytes,0,sizeof(bytes));put32(bytes,total);bytes[5]=1;bytes[6]=5;bytes[8]=1;
    put32(bytes+10,section);put32(bytes+14,7);put16(bytes+18,(int)name_len);
    for(unsigned i=0;i<sizeof(name);i++) bytes[20+i*2]=(uint8_t)name[i];
    put32(bytes+20+name_len,data_len);
    for(unsigned i=0;i<sizeof(guid);i++) bytes[24+name_len+i*2]=(uint8_t)guid[i];
    return bytes;
}
