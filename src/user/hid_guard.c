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

/* Status
 * devices list *DONE*
 * reading of the report_descriptions *DONE*
 * Parsing of the report descriptor to identify the keyboard -> LEFT
 * pass the struct to bpf map
 * ebpf hook to that device
 * monitoring -> detection(delta keystrokes, delta connection time & first
 * packet recieved, wellfords algo)
 */

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

/*TODO: modify this to get the hid_id from the dev_list
 * read the list from get_hid_devices
 * *DONE*
 */

void get_hid_id(void) {
  char **list = get_hid_devices();
  char **head = list;
  size_t out_size;

  while (*list != NULL) {
    size_t path_size = strlen(*list) + strlen("/report_descriptor") +
                       1; // +1 for null sentinal

    char *report_descriptor_path;
    report_descriptor_path = malloc(path_size);
    snprintf(report_descriptor_path, path_size, "%s%s", *list,
             "/report_descriptor");
    printf("report_descriptor_path: %s \n", report_descriptor_path);

    unsigned char *buffer =
        file_reading(report_descriptor_path, &out_size, "rb", &report_descriptor_output);
    //    struct kbd_config *config = hid_desc_parse(buffer, out_size, hid_id);

    free(buffer);
    free(*list);
    free(report_descriptor_path);
    list++;
  }

  free(head);
}
/*Modify the function to use malloc and realloc
 * Error handling
 */
unsigned char *file_reading(char *path, size_t *out_size,
                            const char *file_mode, struct hid_output *ops) {
  unsigned char *data = malloc(1024);

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
  printf("Report Descriptor paths \n");
  get_hid_id();

  hid_uevent_parse();

  return 0;
}

/*refactor the code to dynamically decide how to print/view the descriptor data
 * (raw bytes) and uevent data (ascii) reference code from include/linux/fs.h
 * and hid.h -> linux kernel
 * */
