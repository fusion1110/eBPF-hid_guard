#include <assert.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <linux/bpf.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/types.h>

#include "hid_guard.h"
#define _PATH "/sys/bus/hid/devices/"

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

char **get_hid_devices(void) {
  DIR *folder;
  struct dirent *entry;
  int number_of_devices = 0;

  /*here size doesnt matter as i am reallocating it in the loop anyways*/
  char **dev_list = malloc(sizeof(char *));
  if (dev_list == NULL)
    return NULL;

  folder = opendir(_PATH);

  if (folder == NULL) {
    perror("Unable to read directory");
    return (NULL);
  }

  while ((entry = readdir(folder))) {

    size_t total_size = strlen(_PATH) + strlen(entry->d_name) + 1;

    /*Filtering . and ..*/
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    char *dev_path = malloc(total_size);

    /*Creating path name*/
    snprintf(dev_path, total_size, "%s%s", _PATH, entry->d_name);

    /*Re-allocation of memory -> new device so more mem required + 1 for NULL*/
    char **temp = realloc(dev_list, (number_of_devices + 2) * sizeof(char *));
    if (temp == NULL)
      return NULL;

    dev_list = temp;

    dev_list[number_of_devices] = dev_path;

    number_of_devices++;
  }
  dev_list[number_of_devices] = NULL;
  closedir(folder);

  return (char **)dev_list;
}

/*Change how this works
 * Required: To return the report descriptor bytes when given a path -> a device
 * only*/
unsigned char *report_descriptor_raw(char *path, size_t *out_size) {
  size_t path_size =
      strlen(path) + strlen("/report_descriptor") + 1; // +1 for null sentinal

  char *report_descriptor_path;
  report_descriptor_path = malloc(path_size);
  snprintf(report_descriptor_path, path_size, "%s%s", path,
           "/report_descriptor");
  printf("report_descriptor_path: %s \n", report_descriptor_path);

  unsigned char *report_buffer = read_hid_file(report_descriptor_path, out_size,
                                               "rb", &report_descriptor_output);

  free(report_descriptor_path);

  return report_buffer;
}

unsigned char *read_hid_file(char *path, size_t *out_size,
                             const char *file_mode, struct hid_output *ops) {
  unsigned char *data = malloc(1024);
  if (data == NULL)
    return NULL;

  FILE *fptr;
  fptr = fopen(path, file_mode);

  if (fptr == NULL) {
    perror("Error opening file");
    return NULL;
  }
  size_t report_bytes = fread(data, 1 /*a single byte*/, 1024, fptr);

  *out_size = report_bytes;

  printf("Read %zu bytes from descriptor:\n", report_bytes);

  ops->print_data(data, report_bytes);

  fclose(fptr);
  return data;
}

int main() {
  size_t out_size;
  char **dev_list = get_hid_devices();
  /*TODO: need to loop through devices here.*/
  unsigned char *data = report_descriptor_raw(dev_list[0], &out_size);
  struct kbd_map *km = parse_hid_uevent(dev_list);
  if (km == NULL) {
    printf("no keyboard found\n");
    goto cleanup;
  }

  struct kbd_config *hdp = hid_desc_parse(data, out_size);
  if (hdp == NULL) {
    printf("Failed to parse the data \n");
    goto cleanup;
  }

cleanup:
  free(km);
  free(dev_list);
  free(hdp);
  free(data);

  return 0;
}
