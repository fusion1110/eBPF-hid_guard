#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "payload.h"

char LICENSE[] SEC("license") = "GPL";

//ring buffer map
struct {
  __uint(type, BPF_MAP_TYPE_RINGBUF);
  __uint(max_entries, 256 * 1024);
} rb SEC(".maps");

SEC("struct_ops/hid_device_event")

int BPF_PROG(detect_peripherals, struct hid_bpf_ctx *hctx, enum hid_report_type type, __u64 data)
{
  struct _payload *p;
  struct hid_device *hdev;
  __u32 actual_size;

  p = bpf_ringbuf_reserve(&rb, sizeof(*p), 0);
  if(!p) return 0;

  hdev = (struct hid_device *)BPF_CORE_READ(hctx, hid);

  p->vendor_id = BPF_CORE_READ(hdev, vendor);
  p->product_id = BPF_CORE_READ(hdev, product);
  p->bus = BPF_CORE_READ(hdev, bus);
  p->report_type = type;
  //data speed in nanoseconds
  p->timestamp_ns = bpf_ktime_get_ns();

  actual_size = BPF_CORE_READ(hctx, size);
  if(actual_size > MAX_LEN) actual_size = MAX_LEN;
  p->actual_size = actual_size;

  __u8 *data_ptr = NULL;

  /* The eBPF verifier requires a constant size argument. 
     branch to handle standard USB (8) and Bluetooth with Report ID (9) */
  if (actual_size == 9) {
      data_ptr = hid_bpf_get_data(hctx, 0, 9);
  } else if (actual_size == 8) {
      data_ptr = hid_bpf_get_data(hctx, 0, 8);
  } else {
      // If it's not a standard keyboard report size, drop it
      bpf_ringbuf_discard(p, 0);
      return 0;
  }

  if(!data_ptr){
    bpf_ringbuf_discard(p, 0);
    return 0;
  }

  bpf_probe_read_kernel(&p->payload, actual_size, data_ptr);
  
  bpf_ringbuf_submit(p, 0);

  return 0;
}SEC(".struct_ops.link")
struct hid_bpf_ops my_hid_ops = {
    .hid_device_event = (void *) detect_peripherals,
};
