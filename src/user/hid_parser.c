#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hid_guard.h"
/*
struct kbd_config *hid_desc_parse(unsigned char *buffer, size_t size,
                                  uint32_t hid_id) {


}
*/
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
