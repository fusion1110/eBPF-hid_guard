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

#define _PATH "/sys/bus/hid/devices/"

static volatile bool running = true;

static void int_exit(int sig) {
  running = false;
  exit(0);
}

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s <sysfs_path>\n", prog);
  fprintf(stderr, "Example: %s /sys/bus/hid/devices/\n", prog);
}

/*
 * 1. list the devices in sys/bus/hid/-------devices---------
 * 2. print the report_des of all the devices.
 * */

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

/*TODO: modify this to get the hid_id from the dev_list*/
/*
static int get_hid_id(const char *path) {
  const char *dir;
  char uevent[1024];
  int fd;

  memset(uevent, 0, sizeof(uevent));
  snprintf(uevent, sizeof(uevent) - 1, "%s/uevent", path);

  fd = open(uevent, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    return -ENOENT;

  close(fd);

  return (int)strtol(str_id, NULL, 16);
}
*/

int main() {

  char **res = get_hid_devices();
  char **head = res;
  while (*res != NULL) {
    printf("%s \n", *res);
    free(*res);
    res++;
  }
  goto cleanup;

cleanup:
  free(head);
  return 0;
}
