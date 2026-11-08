#ifndef BATTERY_LIGHT_H
#define BATTERY_LIGHT_H
#include <stdbool.h>
#include <stdint.h>

/* DualSense PowerState is an enum, not independent charging/full bits.
 * Error/unknown states and reserved levels must never become a false 100%. */
static inline uint8_t battery_percent(uint8_t raw)
{
    unsigned state=raw>>4, level=raw&15;
    if(state>2 || level>10)return 0xff;
    return state==2?100:(uint8_t)(level*10);
}

/* Native Sony modes retain host lighting; default white in Nintendo modes
 * means automatic battery color. A custom RGB remains a fixed-color choice. */
static inline bool battery_light_color(uint8_t mode,uint8_t percent,uint8_t state,
                                      const uint8_t configured[3],uint8_t rgb[3])
{
    if(mode!=3 && mode!=4 && mode!=6)return false;
    if(configured[0]!=255 || configured[1]!=255 || configured[2]!=255) {
        for(unsigned i=0;i<3;i++)rgb[i]=configured[i];
        return true;
    }
    if(percent>100 || state>2)return false;
    rgb[0]=rgb[1]=rgb[2]=0;
    if(state==1) { rgb[0]=255;rgb[1]=96; } /* charging: amber */
    else if(state==2 || percent>=60)rgb[1]=160;
    else if(percent>=30) { rgb[0]=255;rgb[1]=128; }
    else rgb[0]=255;
    return true;
}
#endif
