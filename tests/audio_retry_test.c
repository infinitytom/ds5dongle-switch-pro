#include "audio_retry.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    uint32_t failures=0,elapsed=0;
    /* A blocked output path remains bounded through a long outage, and one
     * successful packet restores normal pacing rather than stale playback. */
    for(unsigned i=0;i<10000;i++) {
        uint32_t delay=pro_audio_retry_delay_us(&failures,-1);
        assert(delay>=21333&&delay<=1000000);
        if(i<50) elapsed+=delay;
    }
    assert(failures==50&&elapsed>8000000);
    assert(pro_audio_retry_delay_us(&failures,0)==21333&&failures==0);
    assert(pro_audio_retry_delay_us(&failures,-1)==21333&&failures==1);
    /* Reconnect creates a fresh policy: failures from an earlier link must
     * not cause its replacement to inherit the maximum retry delay. */
    failures=0;assert(pro_audio_retry_delay_us(&failures,-1)==21333);
    puts("Pro audio: bounded congestion backoff, saturation and recovery passed.");
    return 0;
}
