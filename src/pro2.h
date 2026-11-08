#ifndef PRO2_H
#define PRO2_H
#include "switch_pro.h"
#include "pro2_motion.h"
#define PRO2_MODE 6
#define PRO2_EP_IN 0x84
#define PRO2_EP_OUT 0x01
#define PRO2_BULK_IN 0x82
#define PRO2_BULK_OUT 0x03
#define PRO2_CONFIG_LEN 80
#define PRO2_REPLY_MAX 96
#define PRO2_COMMAND_MAX 128
#define PRO2_USER_CAL_SIZE 128
struct pro2_state {
    uint8_t mode, features, mask, mac[6], ltk[16], player_leds;
    bool streaming, edge, motion_valid, led_pending, calibration_dirty;
    uint32_t counter, motion_sequence, sensor_time_us;
    uint8_t motion_pdu[30], user_calibration[PRO2_USER_CAL_SIZE];
    ns2_ds5_motion_state_t motion;
};
struct pro2_rx { uint8_t data[PRO2_COMMAND_MAX]; unsigned used, expected; };
/* One Bulk transfer may contain fragments of an 80-byte calibration write. */
unsigned pro2_rx_feed(struct pro2_rx *rx, const uint8_t *p, unsigned len);
void pro2_init(struct pro2_state *s);
void pro2_disconnect(struct pro2_state *s);
void pro2_input(struct pro2_state *s, const struct switch_pro *in, bool edge, uint32_t us);
bool pro2_take_led(struct pro2_state *s, uint8_t report[48]);
void pro2_set_mac(struct pro2_state *s, const uint8_t mac[6]);
/* Native console EP0 identity requests, independent of the WinUSB PC path. */
bool pro2_control(const struct pro2_state *s, uint8_t type, uint8_t request,
                  uint16_t value, uint16_t index, uint16_t requested,
                  uint8_t reply[64], uint32_t *len);
/* A response can span two USB packets. */
unsigned pro2_command(struct pro2_state *s, const uint8_t *data, unsigned len, uint8_t reply[PRO2_REPLY_MAX]);
void pro2_report(struct pro2_state *s, const struct switch_pro *input, uint32_t us, uint8_t out[64]);
/* The SDL single-frame 0x5 rumble format is translated to classic DS rumble.
 * Other packed formats are rejected, rather than inventing HD waveforms. */
bool pro2_rumble(const struct pro2_state *s, const uint8_t *data, unsigned len, uint8_t ds[48]);
const uint8_t *pro2_config_descriptor(bool high_speed, bool other_speed);
const uint8_t *pro2_hid_descriptor(uint16_t *len);
extern const uint8_t pro2_compat_id[40];
const uint8_t *pro2_os_properties(void);
#endif
