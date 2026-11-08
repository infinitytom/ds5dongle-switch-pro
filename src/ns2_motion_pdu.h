/* SPDX-License-Identifier: Apache-2.0
 * Adapted from notsosaelin/PicoSwitch2 e054bfd1311289f392069ff0d2447e01748c2124.
 * See NOTICE and lib/PicoSwitch2-APACHE-2.0.txt. */
#ifndef NS2_MOTION_PDU_H
#define NS2_MOTION_PDU_H
#include <stdbool.h>
#include <stdint.h>
#define NS2_MOTION_PDU30_LENGTH 30u
#define NS2_MOTION_ORIENTATION_MASK 0x03ffffffu
#define NS2_MOTION30_ACCEL_Q16_PER_COUNT 68963
bool ns2_motion_pdu30_get_orientation(const uint8_t pdu[30],uint32_t out[3]);
bool ns2_motion_pdu30_set_orientation(uint8_t pdu[30],const uint32_t values[3]);
#endif
