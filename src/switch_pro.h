#ifndef SWITCH_PRO_H
#define SWITCH_PRO_H
#include <stdbool.h>
#include <stdint.h>

/* Protocol core has no RTOS/USB dependencies so packet fixtures run on a PC. */
struct switch_motor { float hz[2], amp[2], phase[2]; };
struct switch_pro {
    uint8_t ds[63], mac[6], mode, player_leds;
    bool imu, vibration, usb_stream;
    uint8_t gyro_range, accel_range;
    uint32_t rumble_ms;
    struct switch_motor motor[2];
    float bias[6], scale[6];
};
void switch_pro_init(struct switch_pro *s, const uint8_t mac[6]);
void switch_pro_disconnect(struct switch_pro *s);
void switch_pro_input(struct switch_pro *s, const uint8_t ds[63]);
void switch_pro_calibration(struct switch_pro *s, const uint8_t *data, unsigned len);
void switch_pro_report(const struct switch_pro *s, uint32_t ms, uint8_t out[64]);
void switch_pro_simple_report(const struct switch_pro *s, uint8_t out[64]);
/* Returns response length (64), or zero when no reply is required. */
unsigned switch_pro_output(struct switch_pro *s, const uint8_t *p, unsigned len,
                           uint32_t ms, uint8_t reply[64]);
void switch_pro_render(struct switch_pro *s, uint32_t ms, float gain,
                       int8_t *stereo, unsigned frames);
#endif
