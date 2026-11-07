#include "xbox.h"
#include <string.h>

/* Independent CherryUSB implementation of the XUSB wire protocol.
 * Protocol reference: SundayMoments/DS5_Bridge. */
static int16_t axis(uint8_t value, bool invert)
{
    int32_t delta = (int32_t)value - 128;
    int32_t denominator = delta < 0 ? 128 : 127;
    int32_t directed = invert ? -delta : delta;
    int32_t span = directed < 0 ? 32768 : 32767;
    int32_t rounded = ((directed < 0 ? -directed : directed) * span + denominator/2) / denominator;
    return (int16_t)(directed < 0 ? -rounded : rounded);
}
static void put16(uint8_t *p, uint16_t value) { p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); }
void xbox_input(const uint8_t ds[63], uint8_t report[20])
{
    memset(report, 0, 20);
    report[1] = 20;
    uint8_t hat = ds[7] & 15;
    uint16_t buttons = 0;
    if (hat==0 || hat==1 || hat==7) buttons |= 1;
    if (hat==3 || hat==4 || hat==5) buttons |= 2;
    if (hat==5 || hat==6 || hat==7) buttons |= 4;
    if (hat==1 || hat==2 || hat==3) buttons |= 8;
    if (ds[8]&0x20) buttons |= 0x10; /* Options -> Start */
    if (ds[8]&0x10) buttons |= 0x20; /* Create -> Back */
    if (ds[8]&0x40) buttons |= 0x40;
    if (ds[8]&0x80) buttons |= 0x80;
    if (ds[8]&1) buttons |= 0x100;
    if (ds[8]&2) buttons |= 0x200;
    if (ds[9]&1) buttons |= 0x400;
    if (ds[7]&0x20) buttons |= 0x1000; /* cross -> A */
    if (ds[7]&0x40) buttons |= 0x2000; /* circle -> B */
    if (ds[7]&0x10) buttons |= 0x4000; /* square -> X */
    if (ds[7]&0x80) buttons |= 0x8000; /* triangle -> Y */
    put16(report+2, buttons);
    report[4]=ds[4]; report[5]=ds[5];
    for (unsigned i=0;i<4;i++) put16(report+6+i*2, (uint16_t)axis(ds[i], (i&1)!=0));
}
bool xbox_output(const uint8_t *packet, unsigned len, uint8_t ds_report[48])
{
    if (!packet || len<8 || len>32 || packet[0]!=0 || packet[1]!=8) return false;
    memset(ds_report, 0, 48);
    ds_report[0]=2; /* DualSense USB report ID */
    ds_report[1]=(packet[3] || packet[4]) ? 2 : 1;
    /* Improved rumble for current DualSense firmware; zero returns actuator
     * ownership to audio instead of leaving the controller in rumble mode. */
    ds_report[39]=(packet[3] || packet[4]) ? 4 : 0;
    ds_report[3]=packet[4]; /* weak/right */
    ds_report[4]=packet[3]; /* strong/left */
    return true;
}

/* Independent vendor HID configuration interface, reachable through WebHID
 * while the gamepad interface is owned by the native XInput driver. */
static const uint8_t hid[] = {
    0x06,0x00,0xff,0x09,1,0xa1,1,0x15,0,0x26,0xff,0,0x75,8,0x95,63,
    0x85,0xf6,0x09,1,0xb1,2,0x85,0xf7,0x09,2,0xb1,2,
    0x85,0xf8,0x09,3,0xb1,2,0x85,0xf9,0x09,4,0xb1,2,
    0x85,0xfb,0x09,5,0x95,127,0xb1,2,0xc0
};
static const uint8_t config[] = {
    9,2,XBOX_CONFIG_LEN,0,4,1,0,0x80,250,
    /* UAC1 playback: 4ch / 16bit / 48kHz; L/R sound, rear L/R haptics. */
    8,11,0,2,1,0,0,0,
    9,4,0,0,0,1,1,0,0,
    9,0x24,1,0,1,42,0,1,1,
    12,0x24,2,1,1,1,0,4,0x33,0,0,0,
    12,0x24,6,2,1,1,3,0,0,0,0,0,
    9,0x24,3,3,1,3,0,2,0,
    9,4,1,0,0,1,2,0,0,
    9,4,1,1,1,1,2,0,0,
    7,0x24,1,1,1,1,0,
    11,0x24,2,1,4,2,16,1,0x80,0xbb,0,
    9,5,0x01,0x09,0x88,1,1,0,0,
    7,0x25,1,0,0,0,0,
    9,4,2,0,2,0xff,0x5d,1,0,
    17,0x21,0,1,1,0x25,XBOX_EP_IN,0x14,0,0,0,0,0x13,XBOX_EP_OUT,8,0,0,
    7,5,XBOX_EP_IN,3,32,0,1,
    7,5,XBOX_EP_OUT,3,32,0,1,
    9,4,3,0,1,3,0,0,0,
    9,0x21,0x11,1,0,1,0x22,sizeof(hid)&255,sizeof(hid)>>8,
    7,5,XBOX_CONFIG_EP,3,8,0,10
};
_Static_assert(sizeof(config)==XBOX_CONFIG_LEN, "Xbox configuration length");
const uint8_t xbox_os_string[18] = {
    18,3,'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,0x20,0
};
const uint8_t xbox_compat_id[40] = {
    40,0,0,0,0,1,4,0,1,0,0,0,0,0,0,0,
    2,1,'X','U','S','B','1','0',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
const uint8_t *xbox_hid_descriptor(uint16_t *len) { *len=sizeof(hid); return hid; }
const uint8_t *xbox_config_descriptor(bool high_speed, bool other_speed)
{
    static uint8_t result[4][sizeof(config)];
    uint8_t *out=result[(high_speed?2:0)+(other_speed?1:0)];
    memcpy(out,config,sizeof(config)); out[1]=other_speed?7:2;
    /* HS intervals are exponents of 125us; FS intervals are milliseconds. */
    for(unsigned i=9;i<sizeof(config);i+=out[i]) {
        if(out[i+1]!=5) continue;
        out[i+6]=out[i+2]==1 ? (high_speed?4:1) :
            out[i+2]==XBOX_CONFIG_EP ? (high_speed?7:10) : (high_speed?4:1);
    }
    return out;
}
