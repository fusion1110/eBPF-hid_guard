#ifndef __HID_GUARD_H
#define __HID_GUARD_H

#define MAX_LEN 127
#include "hid-parser.h"

struct hid_output {
  void (*print_data)(unsigned char *buffer, size_t length);
};


extern struct hid_output report_descriptor_output;
extern struct hid_output uevent_output;

unsigned char *file_reading(char *path, size_t *out_size,
                            const char *file_mode, struct hid_output *ops);
void get_hid_id(void);
char **get_hid_devices(void);

struct kbd_map *hid_uevent_parse(void);

#endif
