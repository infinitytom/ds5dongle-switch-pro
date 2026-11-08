#ifndef AUDIO_RETRY_H
#define AUDIO_RETRY_H
#include <stdint.h>

/* Pro haptic output congestion is not evidence that input has disconnected.
 * Drop stale waveforms and bound retries; the input/link watchdog owns teardown. */
static inline uint32_t pro_audio_retry_delay_us(uint32_t *failures, int result)
{
    if(!result) { *failures=0;return 21333; }
    if(*failures<50) ++*failures;
    return *failures>=50?1000000:*failures>=10?250000:*failures>=3?100000:21333;
}
#endif
