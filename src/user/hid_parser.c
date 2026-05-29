#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hid_guard.h"

#define BIT(nr) (1 << (nr))

uint16_t get_unaligned_le16(const uint8_t *buf) {
  return buf[0] | (buf[1] << 8);
}

uint32_t get_unaligned_le32(const uint8_t *buf) {
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/*Copied from hid-core.c from linux kernel source*/
static const uint8_t *fetch_item(const uint8_t *start, const uint8_t *end,
                                 struct hid_item *item) {
  uint8_t b;

  if ((end - start) <= 0)
    return NULL;

  b = *start++;

  item->type = (b >> 2) & 3;
  item->tag = (b >> 4) & 15;

  // i have no use for this -> as my purpose is to just identify the whether
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
  item->size = BIT(b & 3) >> 1; /* 0, 1, 2, 3 -> 0, 1, 2, 4 */

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
  unsigned int key_array_offset = 0;
  unsigned int key_array_count = 0;
  int input_count = 0;

  while ((start = fetch_item(start, end, &item)) != NULL) {
    // GLOBAL(1), LOCAL(2), MAIN(0).
    switch (item.type) {
    case HID_ITEM_TYPE_GLOBAL:
      switch (item.tag) {
      case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
        usage_page = item_udata(&item);
        break;
      case HID_GLOBAL_ITEM_TAG_REPORT_ID:
        current_report_id = item_udata(&item);
        break;
      case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
        report_size = item_udata(&item);
        break;
      case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
        report_count = item_udata(&item);
        break;
      }
      break;
    case HID_ITEM_TYPE_LOCAL:
      usage = item_udata(&item);
      break;
    case HID_ITEM_TYPE_MAIN:
      switch (item.tag) {
      case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
        /*HID spec section 6.2.2.8:
         *"when the parser encounters a main item it concatenates the last
         declared Usage Page with a Usage to form a complete usage value."*/
        {
          unsigned int full_usage;
          full_usage = (usage_page << 16) | usage;
          if (full_usage == HID_GD_KEYBOARD) {
            in_keyboard = 1;
          }
          break;
        }
      case HID_MAIN_ITEM_TAG_INPUT:
        if (in_keyboard) {
          bit_offset += report_size * report_count;
          /*second item ie, the keycode array*/
          if (input_count == 1) {
            struct kbd_config *kc = malloc(sizeof(struct kbd_config));
            /*USB HID Class Specification 1.11
             * "If a device has multiple reports, each report is preceded by a
             * single byte report ID field. The report ID is not described in
             * the report descriptor."
             * so need to add +1 to key_array_offset*/
            key_array_offset = (bit_offset / 8) + 1;
            key_array_count = report_count;
            kc->report_id = current_report_id;
            kc->key_array_offset = key_array_offset;
            kc->key_array_count = key_array_count;
            return kc;
          }
          input_count++;
        }
        usage = 0;
        break;
      case HID_MAIN_ITEM_TAG_END_COLLECTION:
        in_keyboard = 0;
        bit_offset = 0;
        input_count = 0;
        usage = 0;
        break;
      }
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
