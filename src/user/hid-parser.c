#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hid-parser.h"
#include "hid_guard.h"

struct hid_output {
  void (*print_data)(unsigned char *buffer, size_t length);
};

void hid_output_ascii(unsigned char *buffer, size_t length){
  for (size_t i = 0; i < length; i++) {
    printf("%c", buffer[i]);
  }
  printf("\n");
}

void hid_output_raw(unsigned char *buffer, size_t length){
for (size_t i = 0; i < length; i++) {
    printf("%02x", buffer[i]);
  }
  printf("\n");
}

struct hid_output report_descriptor_output = {
  .print_data = hid_output_raw
};


struct hid_output uevent_output = {
  .print_data = hid_output_ascii
};

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
    file_reading(uevent_path, &size, "r", &uevent_output);
    // char *strtok()
    free(*list);
    free(uevent_path);
    list++;
  }

  free(head);
}
