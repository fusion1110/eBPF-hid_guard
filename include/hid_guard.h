#ifndef __HID_GUARD_H
#define __HID_GUARD_H

#define MAX_LEN 127

void file_reading(char *path);
void get_hid_id(void);
char **get_hid_devices(void);

struct _dev_info {
  unsigned int vendor_id;
  unsigned int product_id;
  unsigned int bus;

  unsigned int report_type;
  unsigned int actual_size;
  unsigned long long timestamp_ns;
  unsigned char payload[MAX_LEN];
};

#endif
