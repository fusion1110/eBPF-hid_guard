#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hid-parser.h"
#include "hid_guard.h"

struct hid_output {
  void (*print_data)(unsigned char *buffer, size_t length);
};

void hid_output_ascii(unsigned char *buffer, size_t length) {
  for (size_t i = 0; i < length; i++) {
    printf("%c", buffer[i]);
  }
  printf("\n");
}

void hid_output_raw(unsigned char *buffer, size_t length) {
  for (size_t i = 0; i < length; i++) {
    printf("%02x", buffer[i]);
  }
  printf("\n");
}

struct hid_output report_descriptor_output = {.print_data = hid_output_raw};

struct hid_output uevent_output = {.print_data = hid_output_ascii};

/*
struct kbd_config *hid_desc_parse(unsigned char *buffer, size_t size,
                                  uint32_t hid_id) {


}
*/
struct kbd_map *hid_uevent_parse(void) {
  char **list = get_hid_devices();
  char **head = list;
  size_t size;
  while (*list != NULL) {
    size_t path_size = strlen(*list) + strlen("/uevent") + 1;

    char *uevent_path;
    uevent_path = malloc(path_size);

    snprintf(uevent_path, path_size, "%s%s", *list, "/uevent");

    printf("uevent_path: %s \n", uevent_path);
    unsigned char *uevent_data =
        file_reading(uevent_path, &size, "r", &uevent_output);
    char *token = strtok(uevent_data, "\n");

    while (token != NULL) {
      if (strncmp(token, "HID_ID=", 7) == 0) {
        printf(" %s\n", token);
        char *str_tok = token;
        int ret;
        struct kbd_map *km = malloc(sizeof(struct kbd_map));
        unsigned int bus, vendor, product;
        ret = sscanf(str_tok + 7, "%x:%x:%x", &bus, &vendor, &product);

        km.bus = bus;
        km.vendor = vendor;
        km.product = product;

        printf("Bus: %x \n", km.bus);
        printf("vendor: %x \n", km.vendor);
        printf("product: %x \n", km.product);
      }
      token = strtok(NULL, "\n");
    }
    free(*list);
    free(uevent_path);
    list++;
  }

  free(head);
}
