#include "battery_light.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(battery_percent(0x00)==0);
    assert(battery_percent(0x0a)==100);
    assert(battery_percent(0x15)==50);
    assert(battery_percent(0x20)==100);
    assert(battery_percent(0x0f)==255);
    assert(battery_percent(0xa5)==255);
    assert(battery_percent(0xff)==255);
    const uint8_t automatic[3]={255,255,255},custom[3]={10,20,30};uint8_t rgb[3];
    assert(battery_light_color(4,80,0,automatic,rgb) && rgb[0]==0 && rgb[1]==160 && rgb[2]==0);
    assert(battery_light_color(4,30,0,automatic,rgb) && rgb[0]==255 && rgb[1]==128);
    assert(battery_light_color(4,20,0,automatic,rgb) && rgb[0]==255 && rgb[1]==0);
    assert(battery_light_color(3,0,1,automatic,rgb) && rgb[0]==255 && rgb[1]==96);
    assert(battery_light_color(6,100,2,automatic,rgb) && rgb[0]==0 && rgb[1]==160);
    assert(!battery_light_color(4,255,0,automatic,rgb));
    assert(!battery_light_color(4,50,10,automatic,rgb));
    for(uint8_t mode=0;mode<3;mode++)assert(!battery_light_color(mode,80,0,automatic,rgb));
    assert(!battery_light_color(5,80,0,automatic,rgb));
    assert(battery_light_color(4,255,10,custom,rgb) && rgb[0]==10 && rgb[1]==20 && rgb[2]==30);
    puts("battery light: all tests passed");return 0;
}
