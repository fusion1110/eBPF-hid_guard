#ifndef HID_GUARD_H
#define HID_GUARD_H

#define MAX_LEN 127
#include "hid_parser.h"

struct hid_output {
  void (*print_data)(unsigned char *buffer, size_t length);
};

extern struct hid_output report_descriptor_output;
extern struct hid_output uevent_output;

char **get_hid_devices(void);
unsigned char *read_hid_file(const char *path, size_t *out_size,
                             const char *file_mode, struct hid_output *ops);

struct kbd_map *hid_uevent_parse(char **dev_list);
struct kbd_config *hid_desc_parse(const uint8_t *buf, size_t len);

#endif
