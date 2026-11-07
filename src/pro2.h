#ifndef PRO2_H
#define PRO2_H
#include "switch_pro.h"
#define PRO2_MODE 6
#define PRO2_EP_IN 0x84
#define PRO2_EP_OUT 0x01
#define PRO2_BULK_IN 0x82
#define PRO2_BULK_OUT 0x03
#define PRO2_CONFIG_LEN 64
struct pro2_state { uint8_t mode, features, mask; bool streaming; uint32_t counter; };
void pro2_init(struct pro2_state *s);
/* A response can span two USB packets (64 + 16 bytes). */
unsigned pro2_command(struct pro2_state *s, const uint8_t *data, unsigned len, uint8_t reply[80]);
void pro2_report(struct pro2_state *s, const struct switch_pro *input, uint32_t us, uint8_t out[64]);
/* The SDL single-frame 0x5 rumble format is translated to classic DS rumble.
 * Other packed formats are rejected, rather than inventing HD waveforms. */
bool pro2_rumble(const struct pro2_state *s, const uint8_t *data, unsigned len, uint8_t ds[48]);
const uint8_t *pro2_config_descriptor(bool high_speed, bool other_speed);
const uint8_t *pro2_hid_descriptor(uint16_t *len);
extern const uint8_t pro2_compat_id[40];
const uint8_t *pro2_os_properties(void);
#endif
