#ifndef SWITCH_USB_H
#define SWITCH_USB_H
#include <stdint.h>
#include <stdbool.h>

#define SWITCH_USB_EP_IN  0x84
#define SWITCH_USB_EP_OUT 0x01
#define SWITCH_USB_MIC_EP 0x82
#define SWITCH_USB_MIC_CTRL 1
#define SWITCH_USB_MIC_STREAM 2

const uint8_t *switch_usb_hid_descriptor(uint16_t *len);
const uint8_t *switch_usb_config_descriptor(bool mic, bool high_speed,
                                           bool other_speed, uint16_t *len);
#endif
