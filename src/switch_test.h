#ifndef SWITCH_TEST_H
#define SWITCH_TEST_H
#include "switch_pro.h"
/* Local diagnostic lease, separate from the host's negotiated state. */
struct switch_test { struct switch_pro player; uint32_t start; uint16_t duration; uint8_t pattern; };
bool switch_test_begin(struct switch_test *t, unsigned pattern, unsigned duration, uint32_t now);
bool switch_test_active(const struct switch_test *t, uint32_t now);
unsigned switch_test_remaining(const struct switch_test *t, uint32_t now);
#endif
