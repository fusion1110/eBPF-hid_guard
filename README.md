## eBPF-hid_guard

See [Why This Was Archived](#why-this-was-archived) below.

A Linux kernel-space security monitor for detecting and blocking HID injection attacks (Rubber Ducky, O.MG Cable, ESP32-based keyboard emulators) using eBPF HID-BPF struct_ops.

## What This Does

Malicious HID devices emulate keyboards and inject keystrokes far faster than any human typist. This tool detects that timing anomaly and unbinds the offending device from the kernel HID driver before the attack completes.

On startup, the tool snapshots all currently connected HID devices and ignores them — only newly connected devices are monitored. It supports both USB and Bluetooth HID devices.

## Branch Structure

### `main` — Working Proof of Concept

Functional end-to-end pipeline, tested against an ESP32-based keyboard emulator:

- Snapshots pre-existing devices at startup; ignores them
- Parallel USB and BLE discovery threads — first to detect a new device wins
- Attaches a HID-BPF `struct_ops` program to the discovered device
- BPF side captures raw HID report bytes, timestamp, vendor/product/bus via ring buffer
- Userspace computes inter-keystroke deltas, tracks a suspicion counter, and unbinds the device via sysfs when the threshold is crossed

**Known limitations that triggered the rewrite:**

- Report size hardcoded to 8 or 9 bytes (standard boot-protocol keyboards only) — breaks on non-standard descriptors
- No HID report descriptor parsing — device type is assumed, not verified; will misfire on mice, touchpads, or composite devices
- Detection logic lives entirely in userspace — BPF is used only as a raw data pipe
- `hid_id` derived from the sysfs minor number, which is not stable across reboots

### `v0.1` — Clean Rewrite (Halted)

Portable foundation built before re-wiring the BPF layer:

- Enumerates all HID devices via `/sys/bus/hid/devices/`
- Reads and hex-dumps raw report descriptors from sysfs
- HID report descriptor parser in progress (`hid_desc_parse.c`) — device type identification not completed
- Memory-clean (verified with AddressSanitizer)

## Why This Was Archived

Both branches hit the same wall: HID-BPF operates on raw byte arrays, so all descriptor parsing and device-type verification has to live in userspace regardless of how the detection logic is split. Rather than keep building that scaffolding from scratch, I moved to [`udev-hid-bpf`].

## What This Does

Malicious HID devices emulate keyboards and inject keystrokes far faster than any human typist. This tool detects that timing anomaly and unbinds the offending device from the kernel HID driver before the attack completes.

On startup, the tool snapshots all currently connected HID devices and ignores them — only newly connected devices are monitored. It supports both USB and Bluetooth HID devices.

## Branch Structure

### `main` — Working Proof of Concept

Functional end-to-end pipeline, tested against an ESP32-based keyboard emulator:

- Snapshots pre-existing devices at startup; ignores them
- Parallel USB and BLE discovery threads — first to detect a new device wins
- Attaches a HID-BPF `struct_ops` program to the discovered device
- BPF side captures raw HID report bytes, timestamp, vendor/product/bus via ring buffer
- Userspace computes inter-keystroke deltas, tracks a suspicion counter, and unbinds the device via sysfs when the threshold is crossed

**Known limitations that triggered the rewrite:**

- Report size hardcoded to 8 or 9 bytes (standard boot-protocol keyboards only) — breaks on non-standard descriptors
- No HID report descriptor parsing — device type is assumed, not verified; will misfire on mice, touchpads, or composite devices
- Detection logic lives entirely in userspace — BPF is used only as a raw data pipe
- `hid_id` derived from the sysfs minor number, which is not stable across reboots

### `v0.1` — Clean Rewrite (Active Development)

Portable foundation being built before re-wiring the BPF layer:

- Enumerates all HID devices via `/sys/bus/hid/devices/`
- Reads and hex-dumps raw report descriptors from sysfs
- HID report descriptor parser in progress (`hid_desc_parse.c`) — will identify device type before attaching
- Memory-clean (verified with AddressSanitizer)

## Intended Architecture

```
┌─────────────────────────────────────────────────────┐
│                    Userspace                         │
│                                                      │
│  udev monitor → descriptor parse → keyboard?         │
│       ↓                                              │
│  populate BPF config maps (thresholds, device info)  │
│       ↓                                              │
│  attach HID-BPF struct_ops to hid_id                 │
│       ↓                                              │
│  ring buffer consumer → sysfs unbind on attack       │
└──────────────────────────┬──────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────┐
│                  BPF (kernel side)                   │
│                                                      │
│  hid_device_event → timing math (Welford's online    │
│  variance, fixed-point) → per-key timestamp maps     │
│  → ring buffer submit on anomaly                     │
└─────────────────────────────────────────────────────┘
```

Detection logic moves into BPF. Userspace handles descriptor parsing, config map population, and the unbind response. The enumeration window between device connection and BPF attachment is a documented, bounded limitation — real-world HID injection tools include deliberate post-enumeration delays that this window falls within.

## Requirements

- Linux kernel ≥ 6.3 (HID-BPF struct_ops stable)
- `libbpf` development headers
- LLVM/Clang (for BPF compilation)
- `libelf` and `zlib` development headers
- Root privileges

## Build

```bash
make
```

## Usage

```bash
# Auto-detect: plug in or pair the device when prompted
sudo ./hid_guard

# Manual override with known hid_id
sudo ./hid_guard <hid_id>

# List available HID devices
ls /sys/bus/hid/devices/
```

## Detection Tunables (`main.c`)

| Parameter | Default | Meaning |
|---|---|---|
| `ATTACK_MAX_MS` | 5 ms | Inter-keystroke interval indicating injection |
| `HUMAN_MIN_MS` | 30 ms | Minimum interval for human typing |
| `ALERT_THRESHOLD` | 3 | Consecutive suspicious events before blocking |

## Status

| Component | State |
|---|---|
| Device enumeration | Done |
| Report descriptor read | Done |
| Descriptor parser (keyboard identification) | In progress |
| BPF config map population | Planned |
| HID-BPF struct_ops attachment | Done (main branch) |
| Timing detection (Welford variance, BPF-side) | Planned |
| Sysfs unbind blocking | Done (main branch) |

## Testing 

ESP32-S2 was used for BLE keystroke transmission. Next, test with hardware like rubber ducky or ESP32-S3, targetting usb ports


## License

GPL v2
