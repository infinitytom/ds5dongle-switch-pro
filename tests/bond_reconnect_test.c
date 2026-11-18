#include <assert.h>
#include <stdio.h>
#include "bond_reconnect.h"

int main(void)
{
    struct bond_reconnect s = {0};
    assert(!bond_reconnect_due(&s, 0, false, true));
    assert(!bond_reconnect_due(&s, 1999, false, true));
    assert(bond_reconnect_due(&s, 2000, false, true));
    assert(s.attempts == 1);
    /* Busy connecting, scanning, suspended, and unbonded are all ineligible.
     * They must not spend retries or carry idle time across the transition. */
    assert(!bond_reconnect_due(&s, 2001, false, false));
    assert(!bond_reconnect_due(&s, 60000, false, false));
    assert(s.attempts == 1);
    assert(!bond_reconnect_due(&s, 60001, false, true));
    assert(!bond_reconnect_due(&s, 65000, false, true));
    assert(bond_reconnect_due(&s, 65001, false, true));
    assert(!bond_reconnect_due(&s, 65002, false, true));
    assert(!bond_reconnect_due(&s, 75001, false, true));
    assert(bond_reconnect_due(&s, 75002, false, true));
    for (uint32_t t = 80000; t < 1000000; t += 1000)
        assert(!bond_reconnect_due(&s, t, false, true));
    assert(s.attempts == 3);
    assert(!bond_reconnect_due(&s, 1000000, false, false));
    assert(!bond_reconnect_due(&s, 2000000, false, true));
    /* Success ends this episode; a later power-off must not wake the pad. */
    assert(!bond_reconnect_due(&s, 2000001, true, false));
    assert(s.attempts == 0 && !s.waiting && s.complete);
    assert(!bond_reconnect_due(&s, 3000000, false, true));
    assert(!bond_reconnect_due(&s, 3002000, false, true));
    /* Manual BOOT retry uses the same zero-initialized policy. */
    s = (struct bond_reconnect){0};
    assert(!bond_reconnect_due(&s, UINT32_MAX - 1000, false, true));
    assert(!bond_reconnect_due(&s, 998, false, true));
    assert(bond_reconnect_due(&s, 999, false, true));
    /* An incoming connection wins before the grace period expires. */
    s = (struct bond_reconnect){0};
    assert(!bond_reconnect_due(&s, 100, false, true));
    assert(!bond_reconnect_due(&s, 1500, true, false));
    assert(!bond_reconnect_due(&s, 5000, true, true));
    assert(s.attempts == 0);
    puts("Bond reconnect: grace, bounded backoff, eligibility, recovery and clock wrap passed.");
    return 0;
}
