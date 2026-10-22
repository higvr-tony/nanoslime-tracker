#ifndef SLIMENRF_SYSTEM_HAPTICS
#define SLIMENRF_SYSTEM_HAPTICS

#include <stdint.h>

// parse a haptics state table received as ESB ACK payload, safe to call from isr
void haptics_parse_table(const uint8_t *data, uint8_t len, uint8_t tracker_id);

#endif
