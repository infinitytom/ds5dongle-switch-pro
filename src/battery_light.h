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

/* The vertical light strips identify the actual USB mode in every profile. */
static inline bool mode_light_color(uint8_t mode, uint8_t rgb[3])
{
    static const uint8_t colors[7][3] = {
        {0, 64, 255}, {0, 220, 220}, {255, 255, 255},
        {0, 180, 0}, {255, 220, 0}, {255, 80, 0}, {180, 0, 255}
    };
    if(mode > 6)return false;
    for(unsigned i=0;i<3;i++)rgb[i]=colors[mode][i];
    return true;
}

/* Five white player indicators below the touchpad become battery bars.
 * Use symmetric Sony patterns with one through five lit dots; unknown is off.
 * Raw DS battery levels are coarse, so the lowest valid level still shows one. */
static inline uint8_t battery_player_lights(uint8_t percent, uint8_t state)
{
    static const uint8_t patterns[5] = {4, 10, 21, 27, 31};
    if(percent>100 || state>2)return 0;
    unsigned bars = percent ? (percent + 19U) / 20U : 1;
    return patterns[bars-1];
}

/* Overlay only the color/player-light fields in 47-byte SetStateData.
 * Preserve triggers, rumble, audio, mute, and the required primer animation. */
static inline bool controller_lights_apply(uint8_t *data, uint16_t len,
                                           uint8_t mode, uint8_t percent,
                                           uint8_t state)
{
    uint8_t rgb[3];
    if(len<47 || !mode_light_color(mode,rgb))return false;
    data[1]=(data[1]&~0x08)|0x14;
    data[43]=battery_player_lights(percent,state);
    for(unsigned i=0;i<3;i++)data[44+i]=rgb[i];
    return true;
}
#endif
