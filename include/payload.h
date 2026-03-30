#ifndef __PAYLOAD_H
#define __PAYLOAD_H

#define MAX_LEN 127

struct _payload {
  unsigned int vendor_id;
  unsigned int product_id;
  unsigned int bus; //bluetooth or USB

  unsigned int report_type;
  unsigned int actual_size;
  unsigned long long timestamp_ns;
  unsigned char payload[MAX_LEN];
};

#endif
