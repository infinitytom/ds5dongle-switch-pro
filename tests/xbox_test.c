#include "xbox.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int16_t get16(const uint8_t *p) { return (int16_t)(p[0] | p[1]<<8); }
int main(void)
{
    uint8_t ds[63]={128,128,128,128}, report[20]; ds[7]=8;
    xbox_input(ds,report);
    assert(report[0]==0 && report[1]==20);
    for(unsigned i=2;i<20;i++) assert(report[i]==0);
    for(unsigned a=0;a<4;a++) {
        int previous=(a&1)?32767:-32768;
        for(unsigned v=0;v<256;v++) {
            ds[a]=(uint8_t)v; xbox_input(ds,report);
            int current=get16(report+6+2*a);
            assert((a&1)?current<=previous:current>=previous);
            if(v==0) assert(current==((a&1)?32767:-32768));
            if(v==128) assert(current==0);
            if(v==255) assert(current==((a&1)?-32768:32767));
            previous=current;
        }
        ds[a]=128;
    }
    static const uint16_t hats[]={1,9,8,10,2,6,4,5,0};
    for(unsigned h=0;h<16;h++) { ds[7]=(uint8_t)h; xbox_input(ds,report); assert(report[2]==(h<9?hats[h]:0)); }
    const uint16_t face[]={0x4000,0x1000,0x2000,0x8000};
    for(unsigned b=0;b<4;b++) { ds[7]=(uint8_t)(8|(16<<b)); xbox_input(ds,report); assert((uint16_t)get16(report+2)==face[b]); }
    ds[7]=8; ds[8]=0xff; ds[9]=1; ds[4]=19; ds[5]=250;
    xbox_input(ds,report); assert((uint16_t)get16(report+2)==0x7f0); assert(report[4]==19 && report[5]==250);
    uint8_t output[48], rumble[64]={0,8,0,200,75};
    assert(xbox_output(rumble,8,output)); assert(output[0]==2 && output[1]==2 && output[39]==4 && output[3]==75 && output[4]==200);
    for(unsigned n=0;n<=64;n++) assert(xbox_output(rumble,n,output)==(n>=8 && n<=32));
    rumble[0]=1; assert(!xbox_output(rumble,8,output));
    rumble[0]=0; rumble[1]=3; assert(!xbox_output(rumble,8,output));
    rumble[1]=8; rumble[3]=rumble[4]=0; assert(xbox_output(rumble,8,output)); assert(output[3]==0 && output[4]==0 && output[1]==1 && output[39]==0);
    uint16_t hid_len; const uint8_t *hid=xbox_hid_descriptor(&hid_len); assert(hid_len>40 && hid[hid_len-1]==0xc0);
    for(unsigned hs=0;hs<2;hs++) for(unsigned other=0;other<2;other++) {
        const uint8_t *d=xbox_config_descriptor(hs!=0,other!=0);
        assert(d[1]==(other?7:2) && d[2]==XBOX_CONFIG_LEN && d[3]==0 && d[4]==4);
        unsigned endpoints=0,interfaces=0,seen=0;
        for(unsigned i=0;i<XBOX_CONFIG_LEN;i+=d[i]) {
            assert(d[i]>=2 && i+d[i]<=XBOX_CONFIG_LEN);
            if(d[i+1]==4 && d[i+3]==0) { assert(d[i+2]==interfaces++); }
            if(d[i+1]==5) {
                unsigned number=d[i+2]&15; assert(!(seen&(1u<<number))); seen|=1u<<number; endpoints++;
                assert(d[i+6]==(d[i+2]==XBOX_CONFIG_EP?(hs?7:10):(hs?4:1)));
                if(d[i+2]==1) assert(d[i+3]==9 && (d[i+4]|d[i+5]<<8)==392);
            }
            if(d[i+1]==0x24 && d[i]==11) assert(d[i+4]==4 && d[i+6]==16 && d[i+8]==0x80 && d[i+9]==0xbb);
            if(d[i+1]==0x21 && d[i]==9) assert((d[i+7]|d[i+8]<<8)==hid_len);
        }
        assert(interfaces==4 && endpoints==4);
    }
    assert(xbox_os_string[0]==18 && xbox_os_string[16]==0x20);
    assert(xbox_compat_id[0]==40 && xbox_compat_id[16]==2 && !memcmp(xbox_compat_id+18,"XUSB10",6));
    puts("Xbox: axes, all buttons, triggers, malformed output, stop rumble, FS/HS descriptors passed.");
    return 0;
}
