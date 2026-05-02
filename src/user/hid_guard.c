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
 * monitoring -> detection(delta keystrokes, delta connection time & first packet recieved, wellfords algo)
*/


void file_reading(char *path);
char **get_hid_devices(void);
void get_hid_id(void);


static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s <sysfs_path>\n", prog);
  fprintf(stderr, "Example: %s /sys/bus/hid/devices/\n", prog);
}
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

   while (*list != NULL) {
   size_t path_size =
      strlen(*list) + strlen("/report_descriptor") + 1; // +1 for null sentinal

  char *report_descriptor_path;
    report_descriptor_path = malloc(path_size);
    snprintf(report_descriptor_path, path_size, "%s%s", *list,
             "/report_descriptor");
    printf("report_descriptor_path: %s \n", report_descriptor_path);

    file_reading(report_descriptor_path);

    free(*list);
    free(report_descriptor_path);
    list++;
  }

  free(head);
}

void file_reading(char *path) {
  unsigned char data[1024];

  FILE *fptr;
  fptr = fopen(path, "rb");

  if (fptr == NULL) {
    perror("Error opening file");
    return;
  }
  size_t report_bytes = fread(data, 1 /*a single byte*/, sizeof(data), fptr);

  printf("Read %zu bytes from descriptor:\n", report_bytes);

  for (size_t i = 0; i < report_bytes; i++) {
    printf("%02x ", data[i]);

    if ((i + 1) % 16 == 0)
      printf("\n");
  }
  printf("\n");

  fclose(fptr);
}

int main() {
  printf("Report Descriptor paths \n");
  get_hid_id();

  return 0;
}
