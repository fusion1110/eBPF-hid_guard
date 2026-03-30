/*
 *
 * On startup, snapshots all currently connected HID devices into a known-good
 * list. Discovery threads then only return devices that appear AFTER startup —
 * this ensures we never attach to a pre-existing mouse, keyboard, or touchpad.
 *
 * Resources:
 * pthread_create/join  : https://man7.org/linux/man-pages/man3/pthread_create.3.html
 * pthread_cancel       : https://man7.org/linux/man-pages/man3/pthread_cancel.3.html
 * pthread_mutex        : https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3.html
 * pthread_cond         : https://man7.org/linux/man-pages/man3/pthread_cond_wait.3.html
 * HID-BPF docs         : https://docs.kernel.org/hid/hid-bpf.html
 * libbpf API           : https://libbpf.readthedocs.io/en/latest/api.html
 * BPF ring buffer      : https://www.kernel.org/doc/html/latest/bpf/ringbuf.html
 * BPF struct_ops       : https://docs.kernel.org/bpf/struct_ops.html
 * HID boot protocol    : https://www.usb.org/sites/default/files/hid1_11.pdf
 * HID usage tables     : https://usb.org/sites/default/files/hut1_4.pdf
 * sysfs unbind         : https://lwn.net/Articles/143397/
 * Bus type constants   : https://github.com/torvalds/linux/blob/master/include/uapi/linux/input.h
 * opendir/readdir      : https://man7.org/linux/man-pages/man3/opendir.3.html
 * signal/sig_atomic    : https://man7.org/linux/man-pages/man2/signal.2.html
 * sscanf               : https://man7.org/linux/man-pages/man3/scanf.3.html
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#include <bpf/libbpf.h>
#include "main.skel.h"
#include "payload.h"

/* ─── bus type constants (from linux/input.h) ────────────────────────────── */
#define BUS_USB         0x0003
#define BUS_BLUETOOTH   0x0005

/* ─── detection tunables ─────────────────────────────────────────────────── */
#define ATTACK_MAX_MS   5
#define HUMAN_MIN_MS    30
#define ALERT_THRESHOLD 3

/* ─── snapshot: known-good devices present at startup ───────────────────── */
#define MAX_KNOWN_DEVICES 64

static char     known_devices[MAX_KNOWN_DEVICES][256];  /* sysfs entry names  */
static int      known_device_count = 0;
static pthread_mutex_t known_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ─── shared discovery result ────────────────────────────────────────────── */
static pthread_mutex_t discovery_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  discovery_cond  = PTHREAD_COND_INITIALIZER;

static int          discovery_hid_id = -1;
static int          discovery_done   = 0;
static char         device_name[256]  = {0};
static unsigned int detected_bus     = 0;

/* ─── detection state ────────────────────────────────────────────────────── */
static volatile sig_atomic_t exiting;
static unsigned long long last_keypress_ns = 0;
static int                suspicion_count  = 0;
static int                attack_blocked   = 0;

/* ─────────────────────────────────────────────────────────────────────────── */

static int libbpf_print_fn(enum libbpf_print_level level,
                            const char *fmt, va_list args)
{
  if (level == LIBBPF_DEBUG) return 0;
  return vfprintf(stderr, fmt, args);
}

static void sig_handler(int sig) { exiting = 1; }

/* ── snapshot: build the known-good list ─────────────────────────────────── */
static void snapshot_existing_devices(void)
{
  DIR *d = opendir("/sys/bus/hid/devices");
  if (!d) { perror("opendir /sys/bus/hid/devices"); return; }

  struct dirent *e;
  while ((e = readdir(d)) != NULL && known_device_count < MAX_KNOWN_DEVICES) {
    unsigned int b, v, p, minor;
    if (sscanf(e->d_name, "%04x:%04x:%04x.%04x", &b, &v, &p, &minor) == 4) {
      snprintf(known_devices[known_device_count],
               sizeof(known_devices[0]),
               "%s", e->d_name);
      known_device_count++;
      printf("[SNAP] Known device: %s\n", e->d_name);
    }
  }
  closedir(d);
  printf("[SNAP] %d existing device(s) recorded. Will ignore these.\n\n",
         known_device_count);
}

/* ── snapshot: check if a device was present at startup ─────────────────── */
static int is_known_device(const char *name)
{
  pthread_mutex_lock(&known_mutex);
  for (int i = 0; i < known_device_count; i++) {
    if (strcmp(known_devices[i], name) == 0) {
      pthread_mutex_unlock(&known_mutex);
      return 1;
    }
  }
  pthread_mutex_unlock(&known_mutex);
  return 0;
}

/* ── sysfs scan ──────────────────────────────────────────────────────────── */
static int find_new_hid_id_by_bus(unsigned int target_bus,
                                   char *out_name, size_t out_name_sz,
                                   unsigned int *out_bus)
{
  DIR *d = opendir("/sys/bus/hid/devices");
  if (!d) { perror("opendir /sys/bus/hid/devices"); return -1; }

  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    unsigned int b, v, p, minor;
    if (sscanf(e->d_name, "%04x:%04x:%04x.%04x", &b, &v, &p, &minor) != 4)
      continue;

    if (b != target_bus)
      continue;

    if (is_known_device(e->d_name))
      continue;

    snprintf(out_name, out_name_sz, "%s", e->d_name);
    *out_bus = b;
    closedir(d);
    return (int)minor;
  }
  closedir(d);
  return -1;
}

static void publish_result(int hid_id, const char *name, unsigned int bus)
{
  pthread_mutex_lock(&discovery_mutex);
  if (!discovery_done) {
    discovery_hid_id = hid_id;
    discovery_done   = 1;
    detected_bus     = bus;
    snprintf(device_name, sizeof(device_name), "%s", name);
    pthread_cond_signal(&discovery_cond);
  }
  pthread_mutex_unlock(&discovery_mutex);
}

/* ── thread: USB discovery ───────────────────────────────────────────────── */
static void *discover_hid_USB(void *arg)
{
  (void)arg;
  char local_name[256];
  unsigned int local_bus;

  printf("[USB ] Waiting for new USB HID device (plug it in now)...\n");
  fflush(stdout);

  while (!exiting) {
    pthread_testcancel();

    int hid_id = find_new_hid_id_by_bus(BUS_USB, local_name,
                                         sizeof(local_name), &local_bus);
    if (hid_id >= 0) {
      printf("[USB ] New device detected: %s  (hid_id=%d)\n",
             local_name, hid_id);
      fflush(stdout);
      publish_result(hid_id, local_name, local_bus);
      return NULL;
    }

    usleep(200000);
  }
  return NULL;
}

/* ── thread: BLE discovery ───────────────────────────────────────────────── */
static void *discover_hid_BLE(void *arg)
{
  (void)arg;
  char local_name[256];
  unsigned int local_bus;

  printf("[BLE ] Waiting for new Bluetooth HID device (pair it now)...\n");
  fflush(stdout);

  while(1) {
    pthread_testcancel();

    int hid_id = find_new_hid_id_by_bus(BUS_BLUETOOTH, local_name,
                                         sizeof(local_name), &local_bus);
    if (hid_id >= 0) {
      printf("[BLE ] New device detected: %s  (hid_id=%d)\n",
             local_name, hid_id);
      fflush(stdout);
      publish_result(hid_id, local_name, local_bus);
      return NULL;
    }

    usleep(200000);
  }

  printf("[BLE ] No new Bluetooth device found after %ds.\n", BT_WAIT_SECS);
  fflush(stdout);
  return NULL;
}

/* ── wait for either thread to find a device ─────────────────────────────── */
static int wait_for_discovery(pthread_t usb_thread, pthread_t ble_thread)
{
  pthread_mutex_lock(&discovery_mutex);
  while (!discovery_done && !exiting)
    pthread_cond_wait(&discovery_cond, &discovery_mutex);
  int result = discovery_done ? discovery_hid_id : -1;
  pthread_mutex_unlock(&discovery_mutex);

  pthread_cancel(usb_thread);
  pthread_cancel(ble_thread);
  pthread_join(usb_thread, NULL);
  pthread_join(ble_thread, NULL);

  return result;
}

/* ── sysfs unbind ────────────────────────────────────────────────────────── */
static void block_device(void)
{
  if (attack_blocked) return;
  attack_blocked = 1;

  if (device_name[0] == '\0') {
    fprintf(stderr, "[BLOCK] Device name unknown — cannot unbind\n");
    return;
  }

  const char *usb_paths[] = {
    "/sys/bus/hid/drivers/usbhid/unbind",
    "/sys/bus/hid/drivers/hid-generic/unbind",
    NULL
  };
  const char *bt_paths[] = {
    "/sys/bus/hid/drivers/hid-generic/unbind",
    "/sys/bus/hid/drivers/hid-multitouch/unbind",
    NULL
  };

  const char **paths = (detected_bus == BUS_USB) ? usb_paths : bt_paths;

  for (int i = 0; paths[i]; i++) {
    FILE *f = fopen(paths[i], "w");
    if (!f) continue;
    fprintf(f, "%s\n", device_name);
    fclose(f);
    printf("[BLOCK] %s unbound via %s\n", device_name, paths[i]);
    printf("[BLOCK] To restore: echo '%s' | sudo tee %s\n",
           device_name,
           (detected_bus == BUS_USB)
             ? "/sys/bus/hid/drivers/usbhid/bind"
             : "/sys/bus/hid/drivers/hid-generic/bind");
    return;
  }

  fprintf(stderr,
    "[BLOCK] Could not write to any unbind path (are you root?).\n"
    "        echo '%s' | sudo tee /sys/bus/hid/drivers/%s/unbind\n",
    device_name,
    (detected_bus == BUS_USB) ? "usbhid" : "hid-generic");
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
  printf("\n[DEBUG] --- Event Reached Userspace ---\n");
  printf("[DEBUG] Received data_sz = %zu bytes\n", data_sz);

  if (data_sz < 24) {
    printf("[DEBUG] Dropping: data_sz too small for header\n");
    return 0;
  }

  struct _payload *p = (struct _payload *)data;
  
  /* 3. Print the raw variables populated by the kernel */
  printf("[DEBUG] BPF actual_size = %u\n", p->actual_size);
  printf("[DEBUG] Raw payload: %02x %02x %02x %02x\n", 
         p->payload[0], p->payload[1], p->payload[2], p->payload[3]);

  if (p->actual_size < 8) {
      printf("[DEBUG] Dropping: actual_size < 8\n");
      return 0;
  }

  unsigned char modifier;
  unsigned char keycode;

  if (p->actual_size == 9) {
    if (p->payload[0] != 0x01) {
        printf("[DEBUG] Dropping: Not Report ID 0x01\n");
        return 0; 
    }
    modifier = p->payload[1];
    keycode  = p->payload[3];
  } else {
    modifier = p->payload[0];
    keycode  = p->payload[2];
  }

  if (keycode == 0x00) {
      printf("[DEBUG] Dropping: Key release event (keycode 0x00)\n");
      return 0;
  }

  /* Timing and blocking logic remains the same below... */
  if (last_keypress_ns != 0) {
    unsigned long long delta_ns = p->timestamp_ns - last_keypress_ns;
    unsigned long long delta_ms = delta_ns / 1000000ULL;

    printf("[EVENT] key=0x%02x  mod=0x%02x  delta=%4llums  ", keycode, modifier, delta_ms);

    if (delta_ms < ATTACK_MAX_MS) {
      suspicion_count++;
      printf("SUSPICIOUS (%d/%d)\n", suspicion_count, ALERT_THRESHOLD);

      if (suspicion_count >= ALERT_THRESHOLD && !attack_blocked) {
        printf("\n[BLOCK INITIATED]\n");
        block_device();
      }
    } else {
      if (delta_ms > HUMAN_MIN_MS) suspicion_count = 0;
      printf("OK\n");
    }
  } else {
    printf("[EVENT] key=0x%02x  mod=0x%02x  delta=   --ms  (first key)\n", keycode, modifier);
  }

  last_keypress_ns = p->timestamp_ns;
  return 0;
}

int main(int argc, char **argv)
{
  struct main_bpf    *skel = NULL;
  struct bpf_link    *link = NULL;
  struct ring_buffer *rb   = NULL;
  int err    = 0;
  int hid_id = -1;

  signal(SIGINT,  sig_handler);
  signal(SIGTERM, sig_handler);

  libbpf_set_print(libbpf_print_fn);

  snapshot_existing_devices();

  if (argc >= 2) {
    hid_id = atoi(argv[1]);
    printf("[INIT] hid_id=%d from argv (manual override)\n", hid_id);

  } else {
    pthread_t usb_thread, ble_thread;
    pthread_create(&usb_thread, NULL, discover_hid_USB, NULL);
    pthread_create(&ble_thread, NULL, discover_hid_BLE, NULL);

    hid_id = wait_for_discovery(usb_thread, ble_thread);

    if (hid_id < 0) {
      fprintf(stderr,
        "[INIT] No new HID device detected.\n"
        "       For USB : plug in Rubber Ducky and retry.\n"
        "       For BT  : pair ESP32 and retry.\n"
        "       Manual  : sudo %s <hid_id>\n"
        "       List    : ls /sys/bus/hid/devices/\n",
        argv[0]);
      return 1;
    }

    printf("[INIT] Attaching to NEW %s device: %s  (hid_id=%d)\n",
           detected_bus == BUS_USB ? "USB" : "Bluetooth",
           device_name, hid_id);
  }

  skel = main_bpf__open();
  if (!skel) {
    fprintf(stderr, "[INIT] Failed to open BPF skeleton\n");
    return 1;
  }

  skel->struct_ops.my_hid_ops->hid_id = (unsigned int)hid_id;

  err = main_bpf__load(skel);
  if (err) {
    fprintf(stderr, "[INIT] Failed to load BPF skeleton: %d\n", err);
    goto cleanup;
  }

  link = bpf_map__attach_struct_ops(skel->maps.my_hid_ops);
  if (!link) {
    fprintf(stderr, "[INIT] Failed to attach BPF program: %s\n",
            strerror(errno));
    err = -errno;
    goto cleanup;
  }

  rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
  if (!rb) {
    fprintf(stderr, "[INIT] Failed to create ring buffer\n");
    err = -1;
    goto cleanup;
  }

  printf("[INIT] Monitoring started on %s device.\n",
         detected_bus == BUS_USB ? "USB" : "Bluetooth");
  printf("[INIT] Thresholds — attack: <%dms | human: >%dms | block after: %d hits\n\n",
         ATTACK_MAX_MS, HUMAN_MIN_MS, ALERT_THRESHOLD);

  while (!exiting) {
    err = ring_buffer__poll(rb, 100);
    if (err == -EINTR) { err = 0; break; }
    if (err < 0) { fprintf(stderr, "[POLL] Error: %d\n", err); break; }
  }

  printf("\n[EXIT] Done.\n");

cleanup:
  ring_buffer__free(rb);
  bpf_link__destroy(link);
  main_bpf__destroy(skel);
  return err < 0 ? -err : 0;
}
