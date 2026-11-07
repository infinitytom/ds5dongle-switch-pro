#include "switch_test.h"
#include <string.h>
bool switch_test_begin(struct switch_test *t,unsigned pattern,unsigned duration,uint32_t now)
{
    if(pattern>3 || (pattern && (duration<100 || duration>1000))) return false;
    memset(t,0,sizeof(*t));
    if(!pattern) return true;
    const uint8_t mac[6]={0};switch_pro_init(&t->player,mac);
    /* Standard Nintendo 320/160Hz two-band encoding, modest amplitudes.
     * Use the same decoder and waveform renderer as native host effects. */
    uint8_t packet[10]={0x10,0,0,1,0x40,0x40,0,1,0x40,0x40},reply[64];
    const uint8_t tone[4]={0,0x21,0x40,0x48};
    if(pattern&1) memcpy(packet+2,tone,4);
    if(pattern&2) memcpy(packet+6,tone,4);
    switch_pro_output(&t->player,packet,10,now,reply);
    t->player.vibration=true;t->start=now;t->duration=(uint16_t)duration;t->pattern=(uint8_t)pattern;
    return true;
}
bool switch_test_active(const struct switch_test *t,uint32_t now)
{ return t->pattern && (uint32_t)(now-t->start)<t->duration; }
unsigned switch_test_remaining(const struct switch_test *t,uint32_t now)
{ return switch_test_active(t,now)?t->duration-(uint32_t)(now-t->start):0; }
