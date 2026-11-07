#ifndef XBOX_H
#define XBOX_H
#include <stdint.h>
#include <stdbool.h>

#define XBOX_MODE 5
#define XBOX_EP_IN 0x84
#define XBOX_EP_OUT 0x03
#define XBOX_CONFIG_EP 0x82
void xbox_input(const uint8_t ds[63], uint8_t report[20]);
bool xbox_output(const uint8_t *packet, unsigned len, uint8_t ds_report[48]);
const uint8_t *xbox_config_descriptor(bool high_speed, bool other_speed);
const uint8_t *xbox_hid_descriptor(uint16_t *len);
extern const uint8_t xbox_os_string[18];
extern const uint8_t xbox_compat_id[40];
#define XBOX_CONFIG_LEN 185
#endif
