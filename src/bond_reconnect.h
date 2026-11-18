#ifndef BOND_RECONNECT_H
#define BOND_RECONNECT_H
#include <stdbool.h>
#include <stdint.h>

/* Give incoming connections a quiet window before paging the saved address.
 * Do not replace this with inquiry: paired controllers may not be discoverable. */
struct bond_reconnect {
    uint32_t idle_since;
    uint8_t attempts;
    bool waiting, complete;
};
static inline bool bond_reconnect_due(struct bond_reconnect *s, uint32_t now,
                                      bool connected, bool can_page)
{
    /* End this boot/manual recovery episode on success. Do not wake a pad
     * again after a later deliberate power-off or inactivity disconnect. */
    if (connected) { *s = (struct bond_reconnect){ .complete = true }; return false; }
    if (s->complete) return false;
    if (!can_page) { s->waiting = false; return false; }
    if (s->attempts >= 3) return false;
    if (!s->waiting) { s->idle_since = now; s->waiting = true; }
    uint32_t delay = s->attempts == 0 ? 2000 : s->attempts == 1 ? 5000 : 10000;
    if ((uint32_t)(now - s->idle_since) < delay) return false;
    ++s->attempts;
    s->waiting = false;
    return true;
}
#endif
