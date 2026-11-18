#include "battery_light.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    assert(battery_percent(0x00)==0);
    assert(battery_percent(0x0a)==100);
    assert(battery_percent(0x15)==50);
    assert(battery_percent(0x20)==100);
    assert(battery_percent(0x0f)==255);
    assert(battery_percent(0xa5)==255);
    assert(battery_percent(0xff)==255);
    const uint8_t colors[7][3]={{0,64,255},{0,220,220},{255,255,255},
        {0,180,0},{255,220,0},{255,80,0},{180,0,255}};
    for(unsigned percent=0;percent<256;percent++) {
        uint8_t mask=battery_player_lights((uint8_t)percent,0);
        unsigned count=0;
        for(unsigned i=0;i<5;i++)if(mask&(1U<<i))++count;
        unsigned expected=percent>100?0:percent==0?1:(percent+19)/20;
        assert(count==expected && !(mask&0xe0));
    }
    assert(battery_player_lights(100,2)==31);
    assert(battery_player_lights(10,1)==4);
    assert(battery_player_lights(50,10)==0);
    for(unsigned mode=0;mode<7;mode++) {
        uint8_t rgb[3];assert(mode_light_color((uint8_t)mode,rgb));
        assert(memcmp(rgb,colors[mode],3)==0);
        for(unsigned level=0;level<=100;level+=10) {
            uint8_t payload[64],before[64];
            for(unsigned i=0;i<64;i++)payload[i]=(uint8_t)(i*3+1);
            memcpy(before,payload,sizeof(payload));
            assert(controller_lights_apply(payload,64,(uint8_t)mode,(uint8_t)level,0));
            assert(payload[1]==((before[1]&~0x08)|0x14));
            assert(payload[43]==battery_player_lights((uint8_t)level,0));
            assert(memcmp(payload+44,colors[mode],3)==0);
            for(unsigned i=0;i<64;i++)if(i!=1 && (i<43 || i>46))assert(payload[i]==before[i]);
        }
    }
    uint8_t data[47]={0},before[47]={0};
    for(unsigned len=0;len<47;len++)assert(!controller_lights_apply(data,(uint16_t)len,3,80,0));
    for(unsigned mode=7;mode<256;mode++)assert(!controller_lights_apply(data,47,(uint8_t)mode,80,0));
    assert(memcmp(data,before,47)==0);
    assert(controller_lights_apply(data,47,3,255,0) && data[43]==0);
    puts("Controller lights: seven mode colors, five battery bars, unknown levels, bounds and unrelated output preservation passed.");
    return 0;
}
