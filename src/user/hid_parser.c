#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hid_guard.h"

uint16_t get_unaligned_le16(const uint8_t *buf) {
  return buf[0] | (buf[1] << 8);
}

uint32_t get_unaligned_le32(const uint8_t *buf) {
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static const uint8_t *fetch_item(const uint8_t *start, const uint8_t *end,
                                 struct hid_item *item) {
  uint8_t b;

  if ((end - start) <= 0)
    return NULL;

  b = *start++;

  item->type = (b >> 2) & 3;
  item->tag = (b >> 4) & 15;

  // i may have no use for this -> as my purpose is to just identify the whether
  // the dev is a keyboard or not
  if (item->tag == HID_ITEM_TAG_LONG) {

    item->format = HID_ITEM_FORMAT_LONG;

    if ((end - start) < 2)
      return NULL;

    item->size = *start++;
    item->tag = *start++;

    if ((end - start) < item->size)
      return NULL;

    item->data.longdata = start;
    start += item->size;
    return start;
  }

  item->format = HID_ITEM_FORMAT_SHORT;
  // item->size = BIT(b & 3) >> 1; /* 0, 1, 2, 3 -> 0, 1, 2, 4 */

  if (end - start < item->size)
    return NULL;

  switch (item->size) {
  case 0:
    break;

  case 1:
    item->data.u8 = *start;
    break;

  case 2:
    item->data.u16 = get_unaligned_le16(start);
    break;

  case 4:
    item->data.u32 = get_unaligned_le32(start);
    break;
  }

  return start + item->size;
}

/*
 * Read data value from item.
 */

static uint32_t item_udata(struct hid_item *item) {
  switch (item->size) {
  case 1:
    return item->data.u8;
  case 2:
    return item->data.u16;
  case 4:
    return item->data.u32;
  }
  return 0;
}

struct kbd_config *hid_desc_parse(const uint8_t *buf, size_t len) {
  const uint8_t *start = buf;
  const uint8_t *end = buf + len;
  struct hid_item item;

  int in_keyboard = 0;
  uint8_t current_report_id = 0;
  unsigned int usage_page = 0;
  unsigned int usage = 0;
  unsigned int report_size = 0;
  unsigned int report_count = 0;
  unsigned int bit_offset = 0;
  int input_count = 0;

  while ((start = fetch_item(start, end, &item)) != NULL) {
    switch (item.type) {
    case HID_ITEM_TYPE_GLOBAL:
      // handle usage_page, report_id, report_size, report_count
      break;
    case HID_ITEM_TYPE_LOCAL:
      // handle usage
      break;
    case HID_ITEM_TYPE_MAIN:
      // handle BEGIN_COLLECTION, END_COLLECTION, INPUT
      // reset usage after each main item
      usage = 0;
      break;
    }
  }
  return NULL;
}

struct kbd_map *parse_hid_uevent(char **dev_list) {
  char **head = dev_list;
  size_t uevent_len;
  while (*head != NULL) {
    size_t path_size = strlen(*head) + strlen("/uevent") + 1;

    char *uevent_path;
    uevent_path = malloc(path_size);

    snprintf(uevent_path, path_size, "%s%s", *head, "/uevent");

    printf("uevent_path: %s \n", uevent_path);
    unsigned char *uevent_data =
        read_hid_file(uevent_path, &uevent_len, "r", &uevent_output);
    char *token = strtok(uevent_data, "\n");

    while (token != NULL) {
      if (strncmp(token, "HID_ID=", 7) == 0) {
        printf(" %s\n", token);
        char *str_tok = token;
        int ret;
        struct kbd_map *km = malloc(sizeof(struct kbd_map));
        unsigned int bus, vendor, product;
        ret = sscanf(str_tok + 7, "%x:%x:%x", &bus, &vendor, &product);

        if (ret != 3)
          continue;

        km->bus = bus;
        km->vendor = vendor;
        km->product = product;

        printf("Bus: %x \n", km->bus);
        printf("vendor: %x \n", km->vendor);
        printf("product: %x \n", km->product);
        // TODO: need to add report desc check for keyboard
        free(uevent_data);
        return km;
      }
      token = strtok(NULL, "\n");
    }
    free(uevent_path);
    free(uevent_data);
    head++;
  }
  return NULL;
}
