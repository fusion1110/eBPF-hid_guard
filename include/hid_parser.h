#ifndef _HID_PARSER_H
#define _HID_PARSER_H

#include <stdint.h>

/*copied from hid.h -> linux kernel source*/
#define HID_ITEM_TYPE_MAIN 0
#define HID_ITEM_TYPE_GLOBAL 1
#define HID_ITEM_TYPE_LOCAL 2

#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION 10
#define HID_MAIN_ITEM_TAG_END_COLLECTION 12
#define HID_MAIN_ITEM_TAG_INPUT 8

#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE 0
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE 7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID 8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT 9

#define HID_GD_KEYBOARD 0x00010006
#define HID_UP_GENDESK 0x00010000

/*copied from hid.h -> linux kernel source*/
struct hid_item {
  unsigned format;
  uint8_t size;
  uint8_t type;
  uint8_t tag;
  union {
    uint8_t u8;
    int8_t s8;
    uint16_t u16;
    int16_t s16;
    uint32_t u32;
    int32_t s32;
    const uint8_t *longdata;
  } data;
};

/*
 * HID report item format
 */

#define HID_ITEM_FORMAT_SHORT 0
#define HID_ITEM_FORMAT_LONG 1

/*
 * Special tag indicating long items
 */

#define HID_ITEM_TAG_LONG 15

struct kbd_map {
  uint16_t bus;
  uint32_t vendor;
  uint32_t product;
};

struct kbd_config {
  struct kbd_map key;       /*Device identification*/
  uint8_t report_id;        /*kbd report filtering*/
  uint8_t key_array_offset; /*Offset where the array of active key usage IDs
                               begins*/
  uint8_t key_array_count;
};

#endif
