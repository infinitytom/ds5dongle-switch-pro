#include "pro2.h"
#include "ns2_pairing_crypto.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int get16(const uint8_t *p) { return (int16_t)(p[0]|p[1]<<8); }
static unsigned get32(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24; }
static unsigned command(struct pro2_state *s,uint8_t cmd,uint8_t sub,uint8_t payload,uint8_t out[PRO2_REPLY_MAX])
{
    uint8_t p[16]={0};p[0]=cmd;p[1]=0x91;p[3]=sub;p[5]=4;p[8]=payload;
    return pro2_command(s,p,12,out);
}
static void native_extension_tests(void)
{
    struct pro2_state s;struct switch_pro in;uint8_t mac[6]={2},reply[PRO2_REPLY_MAX],report[64];
    pro2_init(&s);switch_pro_init(&in,mac);
    assert(command(&s,0x0c,4,0x27,reply)==12);
    memset(in.ds,0,63);memset(in.ds,128,4);in.ds[7]=8;in.ds[24]=0x20; /* DS Y=+1g. */
    for(unsigned n=1;n<=50;n++) {
        uint32_t stamp=n*12000;for(unsigned b=0;b<4;b++)in.ds[27+b]=(uint8_t)(stamp>>(b*8));
        pro2_input(&s,&in,false,n*4000);
    }
    pro2_report(&s,&in,200000,report);assert(report[0]==9&&report[15]==30);
    assert((int)get32(report+16+24)==282472448); /* independent 1g PDU scale. */
    uint8_t held[30];memcpy(held,report+16,30);pro2_report(&s,&in,204000,report);
    assert(!memcmp(held,report+16,30)); /* Polling alone cannot invent motion. */
    in.ds[17]=0x40;in.ds[18]=6; /* DS yaw +100 deg/s. */
    for(unsigned n=51;n<=300;n++) {
        uint32_t stamp=n*12000;for(unsigned b=0;b<4;b++)in.ds[27+b]=(uint8_t)(stamp>>(b*8));
        pro2_input(&s,&in,true,n*4000);
    }
    assert(fabsf(s.motion.quaternion[2])>.5f); /* physical DS Y -> Pro2 Z. */
    in.ds[9]=0xc0;pro2_report(&s,&in,1200000,report);assert((report[5]&12)==12);
    assert(command(&s,3,10,5,reply)==8);pro2_report(&s,&in,1200000,report);
    assert((report[8]&3)==3&&!(report[7]&3)); /* Paddles never become dpad. */
    pro2_input(&s,&in,false,1204000);pro2_report(&s,&in,1204000,report);assert(!(report[8]&3));
    pro2_disconnect(&s);assert(!s.motion_valid&&!s.motion.initialized&&!s.edge);
    assert(command(&s,3,10,9,reply)==8);pro2_report(&s,&in,0,report);assert(!report[15]);
    /* Native 42-byte and SDL 64-byte rumble share identical payload decoding. */
    uint8_t rumble[64]={2,0x50};rumble[17]=0x50;rumble[2]=0x87;rumble[3]=1;rumble[4]=0x20;rumble[5]=0x11;
    uint8_t a[48],b[48];assert(pro2_rumble(&s,rumble,42,a)&&pro2_rumble(&s,rumble,64,b)&&!memcmp(a,b,48));
    assert(!a[3]&&!a[4]); /* Nonzero frequency with zero amplitudes must not buzz. */
    for(unsigned n=0;n<23;n++)assert(!pro2_rumble(&s,rumble,n,a));
    /* Player assignment is emitted as a valid USB-output bridge report. */
    uint8_t led[16]={9,0x91,0,7,0,8,0,0,3};assert(pro2_command(&s,led,16,reply)==8);
    assert(pro2_take_led(&s,a)&&a[0]==2&&a[2]==0x10&&a[44]==10);
    assert(!a[1]&&!a[3]&&!a[4]&&!pro2_take_led(&s,a));
    /* FS split calibration write, readback, factory protection, RX reset. */
    uint8_t write[80]={2,0x91,0,5,0,72,0,0};write[12]=0;write[13]=0xc0;write[14]=0x1f;
    for(unsigned i=16;i<80;i++)write[i]=(uint8_t)i;
    struct pro2_rx rx={0};assert(!pro2_rx_feed(&rx,write,64));
    assert(pro2_rx_feed(&rx,write+64,16)==80&&pro2_command(&s,rx.data,80,reply)==16);
    assert(s.calibration_dirty&&!memcmp(s.user_calibration,write+16,64));
    uint8_t read[16]={2,0x91,0,4,0,8,0,0,64,0,0,0,0,0xc0,0x1f};
    assert(pro2_command(&s,read,16,reply)==80&&!memcmp(reply+16,write+16,64));
    write[12]=0x70;struct pro2_state before=s;assert(!pro2_command(&s,write,80,reply)&&!memcmp(&s,&before,sizeof(s)));
    write[12]=0;write[13]=0x30;write[14]=1;assert(!pro2_command(&s,write,80,reply));
    memset(&rx,0,sizeof(rx));assert(!pro2_rx_feed(&rx,write,64));
    memset(&rx,0,sizeof(rx));assert(!pro2_rx_feed(&rx,write+64,16));
    read[8]=9;read[12]=0xa8;read[13]=0x30;read[14]=1;
    assert(pro2_command(&s,read,16,reply)==25&&reply[16]==0&&reply[17]==8&&reply[18]==128);
}
int main(void)
{
    native_extension_tests();
    struct pro2_state s;struct switch_pro in;uint8_t reply[PRO2_REPLY_MAX],report[64];
    const uint8_t mac[6]={2,3,4,5,6,7};switch_pro_init(&in,mac);pro2_init(&s);
    assert(s.mode==9 && s.streaming);
    /* Console EP0 requests precede bulk commands. The captured OUT request
     * carries wValue=0x0276; treating every nonzero value as invalid stalls it. */
    {
        uint32_t n;uint8_t control[64],info[16];pro2_set_mac(&s,mac);
        assert(pro2_control(&s,0xc0,3,0,0,64,control,&n)&&n==64);
        assert(control[0]==1&&control[0x12]==0x7e&&control[0x14]==0x69);
        assert(pro2_control(&s,0xc0,3,0,0,7,control,&n)&&n==7);
        assert(pro2_control(&s,0xc0,2,0,0,64,control,&n)&&n==16);
        memcpy(info,control,16);assert(!memcmp(info+10,mac,6));
        assert(pro2_control(&s,0x40,4,0x0276,0,0,control,&n)&&!n);
        assert(!pro2_control(&s,0x40,4,0x0276,0,1,control,&n));
        assert(!pro2_control(&s,0x80,3,0,0,64,control,&n));
        assert(!pro2_control(&s,0xc0,0x20,0,4,64,control,&n));
        assert(command(&s,0x10,1,0,reply)==20&&!memcmp(reply+8,info,3));
        /* Native key exchange then challenge: A1=B1 gives an all-zero LTK.
         * AES-128(key=0, plaintext=0) has an independent known answer. */
        uint8_t p[25]={0x15,0x91,0,4,0,17,0,0,1};
        const uint8_t b1[16]={0x5c,0xf6,0xee,0x79,0x2c,0xdf,0x05,0xe1,0xba,0x2b,0x63,0x25,0xc4,0x1a,0x5f,0x10};
        memcpy(p+9,b1,16);assert(pro2_command(&s,p,25,reply)==25&&!memcmp(reply+9,b1,16));
        p[3]=2;memset(p+9,0,16);
        const uint8_t aes_zero[16]={0x66,0xe9,0x4b,0xd4,0xef,0x8a,0x2c,0x3b,0x88,0x4c,0xfa,0x59,0xca,0x34,0x2b,0x2e};
        assert(pro2_command(&s,p,25,reply)==25&&!memcmp(reply+9,aes_zero,16));
        assert(command(&s,0x15,1,0,reply)==17&&!memcmp(reply+11,info+10,6));
        assert(command(&s,0x15,3,0,reply)==9&&reply[8]==1);
        for(unsigned short_len=8;short_len<25;short_len++) {
            p[5]=(uint8_t)(short_len-8);struct pro2_state before=s;
            assert(!pro2_command(&s,p,short_len,reply)&&!memcmp(&s,&before,sizeof(s)));
        }
        /* Console uses variable-size flash reads as well as SDL's 64-byte reads. */
        uint8_t read[16]={2,0x91,0,4,0,8,0,0,80,0,0,0,0,0x30,1,0};
        assert(pro2_command(&s,read,16,reply)==PRO2_REPLY_MAX&&reply[8]==80);
        assert(reply[4]==0&&reply[5]==0xf8&&reply[16]==1&&reply[34]==0x7e);
        read[8]=81;assert(!pro2_command(&s,read,16,reply));
        uint8_t configure[16]={0x0c,0x91,0,6,0,8,0,0,0,0,0,0,4};
        assert(pro2_command(&s,configure,16,reply)==48&&reply[12]==4);
        assert(command(&s,7,1,0,reply)==9);
        assert(command(&s,0x16,1,0,reply)==32);
        assert(command(&s,0x11,1,0,reply)==12&&reply[8]==3);
        assert(command(&s,0x11,3,0,reply)==37);
        assert(command(&s,0x0c,4,0x27,reply)==12);
        assert(command(&s,0x0c,3,4,reply)==12&&s.mask==0x23&&s.features==0x23);
        assert(command(&s,0x0c,4,4,reply)==12&&s.features==0x23);
        pro2_init(&s);
    }
    /* Independent HID Usage 1..21 expectations, one physical key at a time.
     * Testing only 0x05 missed the old default 0x09 / Pro-1 layout mismatch. */
    const struct {unsigned byte, mask, usage;} buttons[]={
        {7,0x20,1},{7,0x40,2},{7,0x10,3},{7,0x80,4},
        {8,2,5},{8,8,6},{8,0x20,7},{8,0x80,8},
        {8,1,13},{8,4,14},{8,0x10,15},{8,0x40,16},
        {9,1,17},{9,2,18},{9,4,21}
    };
    for(unsigned k=0;k<sizeof(buttons)/sizeof(buttons[0]);k++) {
        uint8_t ds[63]={128,128,128,128};ds[7]=8;
        ds[buttons[k].byte]|=(uint8_t)buttons[k].mask;
        switch_pro_input(&in,ds);pro2_report(&s,&in,4000,report);
        unsigned actual=report[3]|(unsigned)report[4]<<8|(unsigned)report[5]<<16;
        assert(report[0]==9 && actual==(1u<<(buttons[k].usage-1)));
        assert(report[6]==0 && report[7]==8 && report[8]==128);
        for(unsigned i=13;i<64;i++) assert(report[i]==0);
    }
    const unsigned dpad9[]={8,10,2,3,1,5,4,12,0,0,0,0,0,0,0,0};
    for(unsigned h=0;h<16;h++) {
        uint8_t ds[63]={128,128,128,128};ds[7]=(uint8_t)h;
        switch_pro_input(&in,ds);pro2_report(&s,&in,4000,report);
        assert(report[3]==0 && report[4]==dpad9[h] && report[5]==0);
    }
    switch_pro_init(&in,mac);
    for(unsigned a=0;a<4;a++) for(unsigned v=0;v<256;v++) {
        uint8_t ds[63]={128,128,128,128};ds[7]=8;ds[a]=(uint8_t)v;
        switch_pro_input(&in,ds);pro2_report(&s,&in,4000,report);
        const uint8_t *p=report+6+(a/2)*3;
        unsigned actual=(a&1)?(p[1]>>4)|(unsigned)p[2]<<4:p[0]|(unsigned)(p[1]&15)<<8;
        unsigned expected=v<=128?v*16:2048+(v-128)*2047/127;
        assert(actual==expected); /* Browser 0x09: up=0, center=2048, down=4095. */
    }
    switch_pro_init(&in,mac);
    /* Parse the actual HID descriptor, then apply Chromium's Usage N ->
     * buttons[N-1] rule used by the reported external tester. */
    uint16_t descriptor_len;const uint8_t *descriptor=pro2_hid_descriptor(&descriptor_len);
    unsigned page=0,id=0,count=0,usages[32]={0},used=0,wire_to_usage[21]={0},axis_usages[4]={0};
    for(unsigned i=0;i<descriptor_len;) {
        uint8_t key=descriptor[i++];unsigned size=key&3;if(size==3)size=4;
        assert(i+size<=descriptor_len);unsigned value=0;
        for(unsigned b=0;b<size;b++)value|=(unsigned)descriptor[i+b]<<(8*b);
        i+=size;
        if(key==5||key==6)page=value;
        if(key==0x85)id=value;
        if(key==0x95)count=value;
        if(key==9 && used<32)usages[used++]=value;
        if((key&0xfc)==0x80 && page==9 && id==9 && !(value&1)) {
            assert(count==21 && used==21);memcpy(wire_to_usage,usages,sizeof(wire_to_usage));
        }
        if((key&0xfc)==0x80 && page==1 && id==9 && !(value&1)) {
            assert(count==4 && used==4);memcpy(axis_usages,usages,sizeof(axis_usages));
        }
        if((key&12)==0)used=0; /* HID local usages clear after a Main item. */
    }
    const unsigned browser_buttons[]={0,1,2,3,5,7,9,11,13,15,14,12,4,6,8,10,16,17,19,20,18};
    for(unsigned b=0;b<21;b++)assert(wire_to_usage[b]==browser_buttons[b]+1);
    for(unsigned a=0;a<4;a++)assert(axis_usages[a]==0x30+a);
    for(unsigned hs=0;hs<2;hs++) for(unsigned other=0;other<2;other++) {
        const uint8_t *d=pro2_config_descriptor(hs!=0,other!=0);assert(d[1]==(other?7:2)&&d[2]==PRO2_CONFIG_LEN&&d[4]==2);
        unsigned seen=0,endpoints=0;uint16_t hidlen;pro2_hid_descriptor(&hidlen);
        for(unsigned i=9;i<PRO2_CONFIG_LEN;i+=d[i]) {
            assert(d[i]>=2 && i+d[i]<=PRO2_CONFIG_LEN);
            if(d[i+1]==5) {
                unsigned num=d[i+2]&15;assert(num>=1&&num<=4&&!(seen&(1u<<num)));seen|=1u<<num;endpoints++;
                if(d[i+3]==2) assert((d[i+4]|d[i+5]<<8)==(hs?512:64));
            }
            if(d[i+1]==0x21) assert((d[i+7]|d[i+8]<<8)==hidlen);
        }
        assert(endpoints==4 && seen==30);
    }
    assert(pro2_compat_id[16]==1&&!memcmp(pro2_compat_id+18,"WINUSB",6));
    const uint8_t *os=pro2_os_properties();assert(get32(os)==146 && os[8]==1 && get32(os+10)==136 && get32(os+14)==7);
    assert(get32(os+62)==80 && os[66]=='{' && os[140]=='}' && os[142]==0 && os[144]==0);
    /* Replay the exact shape of SDL's 7 calibration reads and init commands. */
    const unsigned blocks[]={0x13000,0x13040,0x13080,0x130c0,0x13100,0x1fc040,0x1fc080};
    for(unsigned b=0;b<7;b++) {
        uint8_t p[16]={2,0x91,0x35,1,0,8};
        for(unsigned j=0;j<4;j++) p[12+j]=(uint8_t)(blocks[b]>>(8*j));
        assert(pro2_command(&s,p,16,reply)==80);
        assert(reply[1]==1&&reply[2]==0x35&&get32(reply+12)==blocks[b]&&reply[8]==64);
        if(b==0) assert(!memcmp(reply+18,"DS5-PRO2-00000000",16));
        if(b==2||b==3) assert(reply[56]==0&&reply[57]==8&&reply[58]==128);
    }
    assert(command(&s,0x0c,2,0x27,reply)==12);
    assert(command(&s,0x0c,4,0x27,reply)==12 && s.features==0x27);
    assert(command(&s,3,10,5,reply)==8 && s.mode==5);
    uint8_t start[16]={3,0x91,0,13,0,8,0,0,1};
    assert(pro2_command(&s,start,16,reply)==12 && s.streaming);
    pro2_report(&s,&in,4000,report);assert(report[0]==5 && get32(report+43)==4000);
    assert(report[11]==0 && report[12]==8 && report[13]==128);
    assert(report[5]==0 && report[6]==0 && report[7]==0);
    in.ds[7]=0xf1;in.ds[8]=0xff;in.ds[9]=7;
    pro2_report(&s,&in,8000,report);assert(report[5]==0xcf && report[6]==0x7f && report[7]==0xc6);
    /* +1g DS X -> +1g SDL X. DS Z and Y rotate into raw Y and Z. */
    memset(in.ds+15,0,12);in.ds[22]=0x20;in.ds[24]=0x10;in.ds[26]=0xf0;
    pro2_report(&s,&in,12000,report);assert(get16(report+49)==4096 && get16(report+51)==2048 && get16(report+53)==2048);
    uint8_t p[64]={2,0x50};p[17]=0x50;
    uint8_t ds[48];assert(pro2_rumble(&s,p,64,ds)&&ds[1]==1&&ds[3]==0&&ds[4]==0);
    /* Low-band amplitude 14500 on left, zero on right. */
    p[5]=(uint8_t)(14500&0xc0);p[6]=(uint8_t)(14500>>8);
    assert(pro2_rumble(&s,p,64,ds));assert(ds[4]>=126&&ds[4]<=127&&ds[3]==0&&ds[39]==4);
    p[1]=0x40;assert(!pro2_rumble(&s,p,64,ds));
    struct pro2_state before=s;
    uint8_t malformed[64]={0x0c,0x91,0,4,0,4,0,0,0x27};
    for(unsigned n=0;n<=64;n++) if(n!=12) {
        assert(pro2_command(&s,malformed,n,reply)==0);assert(!memcmp(&s,&before,sizeof(s)));
    }
    malformed[0]=0xff;assert(!pro2_command(&s,malformed,12,reply));
    assert(command(&s,0x0c,5,0x20,reply)==12 && !(s.features&0x20));
    p[1]=0x50;assert(!pro2_rumble(&s,p,64,ds));
    assert(command(&s,0x0c,5,7,reply)==12);
    pro2_report(&s,&in,16000,report);
    assert(report[5]==0&&report[6]==0&&report[7]==0&&get32(report+43)==0);
    assert(report[11]==0 && report[12]==8 && report[13]==128);
    for(unsigned n=0;n<=64;n++) { uint8_t fuzz[64];memset(fuzz,0xff,sizeof(fuzz));pro2_command(&s,fuzz,n,reply);pro2_rumble(&s,fuzz,n,ds); }
    puts("Pro2: native EP0, AES pairing, initialization, feature masks, SDL input, calibration, IMU, rumble fallback and USB/WinUSB descriptors passed.");
    return 0;
}
