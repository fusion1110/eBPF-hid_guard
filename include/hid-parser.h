#ifndef _HID_PARSER_H
#define _HID_PARSER_H

#include <stdint.h>

struct kbd_config {
  uint32_t hid_id;          /*Device identification*/
  uint8_t report_id;        /*kbd report filtering*/
  uint8_t key_array_offset; /*Offset where the array of active key usage IDs
                               begins*/
  uint8_t key_array_count;
};

struct kbd_map {
  uint8_t bus;
  uint32_t vendor;
  uint32_t product;
};

#endif
