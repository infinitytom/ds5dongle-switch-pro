#include "switch_pro.h"
#include "switch_usb.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sub(struct switch_pro *s,unsigned id,unsigned arg,uint8_t *r){
 uint8_t p[64]={1,0,0,1,0x40,0x40,0,1,0x40,0x40};p[10]=(uint8_t)id;p[11]=(uint8_t)arg;
 assert(switch_pro_output(s,p,12,100,r)==64);assert(r[0]==0x21&&r[14]==id);
}
static unsigned ux(const uint8_t *p){return p[0]|(p[1]&15)<<8;}
static unsigned uy(const uint8_t *p){return (p[1]>>4)|p[2]<<4;}
static int rd16(const uint8_t *p){return (int16_t)(p[0]|p[1]<<8);}
static void wr16(uint8_t *p,int v){p[0]=(uint8_t)v;p[1]=(uint8_t)((uint16_t)v>>8);}
static void descriptor_tests(void){
 uint16_t hid_len;const uint8_t *hid=switch_usb_hid_descriptor(&hid_len);
 unsigned bits[256]={0},feature[256]={0},size=0,count=0,id=0;
 for(unsigned i=0;i<hid_len;){
   uint8_t prefix=hid[i++];unsigned n=prefix&3;if(n==3)n=4;
   assert(i+n<=hid_len);unsigned value=0;for(unsigned j=0;j<n;j++)value|=(unsigned)hid[i+j]<<(j*8);i+=n;
   unsigned type=(prefix>>2)&3,tag=prefix>>4;
   if(type==1&&tag==7)size=value;
   if(type==1&&tag==8)id=value;
   if(type==1&&tag==9)count=value;
   if(type==0&&tag==8)bits[id]+=size*count;
   if(type==0&&tag==11)feature[id]+=size*count;
 }
 assert(bits[0x3f]==88&&bits[0x30]==504&&feature[0xf7]==504&&feature[0xfb]==1016);
 for(unsigned mic=0;mic<2;mic++)for(unsigned high=0;high<2;high++)for(unsigned other=0;other<2;other++){
   uint16_t len;const uint8_t *p=switch_usb_config_descriptor(mic!=0,high!=0,other!=0,&len);
   assert((p[2]|p[3]<<8)==len&&p[1]==(other?7:2)&&p[4]==(mic?3:1));
   unsigned epmask=0,interfaces=0,eps=0;
   for(unsigned i=9;i<len;i+=p[i]){
     assert(p[i]>=2&&i+p[i]<=len);
     if(p[i+1]==4&&p[i+3]==0)interfaces++;
     if(p[i+1]==0x21)assert((p[i+7]|p[i+8]<<8)==hid_len);
     if(p[i+1]==5){
       unsigned ep=p[i+2],index=ep&15;
       /* Critical BL616 regression: IN and OUT cannot share a FIFO index. */
       assert(!(epmask&(1u<<index)));epmask|=1u<<index;eps++;
       assert(p[i+6]==(ep==SWITCH_USB_MIC_EP?(high?4:1):(high?7:8)));
     }
   }
   assert(interfaces==(mic?3u:1u)&&eps==(mic?3u:2u));
   assert(epmask==((1u<<4)|(1u<<1)|(mic?(1u<<2):0)));
 }
}
static void motion_tests(void){
 struct switch_pro s;uint8_t mac[6]={2},r[64],d[63],cal[34]={0};
 switch_pro_init(&s,mac);sub(&s,0x40,1,r);
 /* Invert SDL's Pro->PS mapping: SDL=(-Pro.y,Pro.z,-Pro.x).
  * Test every sensor axis separately so permutations/sign mistakes cannot hide. */
 const unsigned dest[3]={1,2,0};const int sign[3]={-1,1,-1};
 for(unsigned axis=0;axis<3;axis++){
   memset(d,0,63);memset(d,128,4);d[7]=8;
   wr16(d+15+axis*2,1600);wr16(d+21+axis*2,8192);
   switch_pro_input(&s,d);switch_pro_report(&s,10,r);
   for(unsigned sample=0;sample<3;sample++)for(unsigned pro_axis=0;pro_axis<3;pro_axis++){
     int acc=rd16(r+13+sample*12+pro_axis*2),gyro=rd16(r+19+sample*12+pro_axis*2);
     if(pro_axis==dest[axis]){assert(acc==sign[axis]*4096);assert(abs(gyro-sign[axis]*1428)<=1);}
     else{assert(acc==0&&gyro==0);}
   }
 }
 for(unsigned axis=0;axis<3;axis++){
   wr16(cal+axis*2,10+(int)axis*10);
   wr16(cal+6+axis*4,9600);wr16(cal+8+axis*4,-9600);
   wr16(cal+22+axis*4,8192+100+(int)axis*100);wr16(cal+24+axis*4,-8192+100+(int)axis*100);
   wr16(d+15+axis*2,10+(int)axis*10);wr16(d+21+axis*2,100+(int)axis*100);
 }
 wr16(cal+18,600);wr16(cal+20,600);switch_pro_calibration(&s,cal,34);
 switch_pro_input(&s,d);switch_pro_report(&s,10,r);
 for(unsigned i=13;i<49;i++)assert(r[i]==0);
 /* Disconnect clears controller-specific calibration and input, retains USB negotiation. */
 s.usb_stream=true;s.vibration=true;switch_pro_disconnect(&s);
 assert(s.usb_stream&&s.imu&&s.vibration&&s.bias[0]==0&&s.ds[0]==128);
 uint8_t sensitivity[15]={1};sensitivity[10]=0x41;sensitivity[11]=0;sensitivity[12]=2;sensitivity[13]=sensitivity[14]=1;
 switch_pro_output(&s,sensitivity,15,100,r);assert(r[13]==0x80&&s.gyro_range==0&&s.accel_range==2);
 memset(d,0,63);wr16(d+15,1600);wr16(d+21,8192);switch_pro_input(&s,d);switch_pro_report(&s,10,r);
 assert(rd16(r+15)==-16384&&abs(rd16(r+21)+11428)<=2);
 sensitivity[11]=4;switch_pro_output(&s,sensitivity,15,100,r);assert(r[13]==0&&s.gyro_range==0);
 sub(&s,0x40,0,r);sub(&s,0x40,1,r);assert(s.gyro_range==3&&s.accel_range==0);
}
int main(void){
 descriptor_tests();
 struct switch_pro s;uint8_t r[64],d[63]={0},mac[6]={2,3,4,5,6,7};
 switch_pro_init(&s,mac);switch_pro_report(&s,10,r);assert(r[0]==0x30&&ux(r+6)==2048&&uy(r+6)==2048);
 switch_pro_simple_report(&s,r);assert(s.mode==0x3f&&r[0]==0x3f&&r[3]==8&&r[4]==0&&r[5]==128);
 uint8_t usb[2]={0x80,1};assert(switch_pro_output(&s,usb,2,0,r)==64);assert(r[0]==0x81&&r[1]==1&&r[3]==3&&r[4]==7&&r[9]==2);
 usb[1]=2;assert(switch_pro_output(&s,usb,2,0,r)==64&&r[1]==2);
 usb[1]=3;assert(switch_pro_output(&s,usb,2,0,r)==64&&r[1]==3);
 usb[1]=4;assert(switch_pro_output(&s,usb,2,0,r)==0&&s.usb_stream);
 sub(&s,2,0,r);assert(r[13]==0x82&&r[17]==3&&!memcmp(r+19,mac,6));
 sub(&s,3,0x30,r);assert(s.mode==0x30&&r[13]==0x80);sub(&s,3,0x31,r);assert(r[13]==0);
 sub(&s,3,0x3f,r);assert(s.mode==0x3f&&r[13]==0x80);sub(&s,3,0x30,r);
 sub(&s,0x40,1,r);assert(s.imu);sub(&s,0x30,3,r);assert(s.player_leds==3);sub(&s,0x31,0,r);assert(r[13]==0xb0&&r[15]==3);
 /* Ryujinx NpadHdRumble.InitializeDevice: 80 05 arrives after SDL set 0x30.
  * Both timeout commands must preserve the stream, IMU and vibration state. */
 {
  struct switch_pro ry;switch_pro_init(&ry,mac);sub(&ry,3,0x30,r);
  usb[1]=5;assert(switch_pro_output(&ry,usb,2,0,r)==0);
  assert(ry.mode==0x30&&ry.usb_stream);
  sub(&ry,0x40,1,r);sub(&ry,0x48,1,r);
  uint8_t source[63]={128,128,128,128};source[7]=8;wr16(source+21,8192);
  switch_pro_input(&ry,source);switch_pro_report(&ry,100,r);
  assert(rd16(r+15)==-4096);uint8_t before[36];memcpy(before,r+13,36);
  for(unsigned cmd=4;cmd<=5;cmd++) {
   usb[1]=(uint8_t)cmd;assert(!switch_pro_output(&ry,usb,2,101,r));
   assert(ry.mode==0x30&&ry.usb_stream&&ry.imu&&ry.vibration);
   switch_pro_report(&ry,102,r);assert(!memcmp(before,r+13,36));
  }
 }
 memset(d,128,4);d[7]=8;
 const unsigned masks[4]={0x10,0x20,0x40,0x80},expected[4]={1,4,8,2};
 for(unsigned i=0;i<4;i++){d[7]=(uint8_t)(8|masks[i]);switch_pro_input(&s,d);switch_pro_report(&s,10,r);assert(r[3]==expected[i]);}
 d[7]=8;d[8]=0xf3;d[9]=3;switch_pro_input(&s,d);switch_pro_report(&s,10,r);assert(r[4]==0x3f&&r[3]==0x40&&r[5]==0x40);
 for(unsigned i=0;i<8;i++){d[7]=(uint8_t)i;d[8]=0;switch_pro_input(&s,d);switch_pro_report(&s,10,r);assert((r[5]&15)!=0);}
 d[7]=8;d[0]=0;d[1]=255;d[2]=255;d[3]=0;switch_pro_input(&s,d);switch_pro_report(&s,10,r);assert(ux(r+6)==0&&uy(r+6)==1&&ux(r+9)==4095&&uy(r+9)==4095);
 motion_tests();
 uint8_t spi[16]={1};spi[10]=0x10;spi[11]=0x3d;spi[12]=0x60;spi[15]=18;assert(switch_pro_output(&s,spi,16,0,r)==64&&r[13]==0x90);assert(r[20]==0xff&&r[21]==0xf7&&r[22]==0x7f);
 spi[15]=30;switch_pro_output(&s,spi,16,0,r);assert(r[13]==0);for(unsigned len=0;len<16;len++)switch_pro_output(&s,spi,len,0,r);
 sub(&s,0x48,1,r);assert(s.vibration);
 uint8_t rum[10]={0x10,0,0,0x89,0x40,0x50,0,1,0x40,0x40};assert(switch_pro_output(&s,rum,10,100,r)==0);
 assert(fabsf(s.motor[0].hz[0]-320)<.01f&&fabsf(s.motor[0].hz[1]-160)<.01f);
 int8_t audio[600];switch_pro_render(&s,101,1,audio,300);unsigned energy=0;for(unsigned i=0;i<300;i++){energy+=(unsigned)abs(audio[i*2]);assert(audio[i*2+1]==0);}assert(energy>1000);
 switch_pro_render(&s,351,1,audio,300);for(unsigned i=0;i<600;i++)assert(audio[i]==0);
 switch_pro_output(&s,rum,10,400,r);s.vibration=false;switch_pro_render(&s,401,2,audio,300);for(unsigned i=0;i<600;i++)assert(audio[i]==0);
 uint8_t packet[64];for(unsigned id=0;id<256;id++)for(unsigned len=0;len<=64;len++){memset(packet,0xff,64);packet[0]=(uint8_t)id;switch_pro_output(&s,packet,len,10,r);}
 puts("Switch Pro handshake, mapping, SPI, motion, stereo waveform and timeout tests passed.");return 0;
}
