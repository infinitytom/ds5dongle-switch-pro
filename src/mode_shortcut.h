#ifndef MODE_SHORTCUT_H
#define MODE_SHORTCUT_H
#include <stdbool.h>
#include <stdint.h>
struct mode_shortcut { bool armed, holding; int target; uint64_t start, last; };
/* Physical DualSense byte 7: face buttons plus hat. Ambiguous chords are invalid. */
static inline int mode_shortcut_target(uint8_t buttons)
{
    switch (buttons) {
    case 0x28: return 0; /* Cross */
    case 0x48: return 1; /* Circle */
    case 0x18: return 2; /* Square */
    case 0x88: return 3; /* Triangle */
    case 0x00: return 4; /* Up */
    case 0x02: return 5; /* Right */
    case 0x04: return 6; /* Down */
    default: return -1;
    }
}
/* Returns a mode once per release. Never completes a hold across an input gap. */
static inline int mode_shortcut_update(struct mode_shortcut *s, bool ps,
                                       bool options, int target, uint64_t now)
{
    if (s->last && now - s->last > 500000ULL) {
        s->armed = false;
        s->holding = false;
    }
    s->last = now;
    if (!ps && !options) s->armed = true;
    if (!ps || !options || target < 0 || target > 6) {
        s->holding = false;
        return -1;
    }
    if (!s->armed) return -1;
    if (!s->holding || s->target != target) {
        s->holding = true;
        s->target = target;
        s->start = now;
    }
    if (now - s->start < 2000000ULL) return -1;
    s->armed = false;
    s->holding = false;
    return target;
}
#endif
