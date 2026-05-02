# eBPF-hid_guard — v0.1 (rewrite in progress)

This branch is a clean rewrite of the `main` branch PoC. The goal is a portable, architecture-correct foundation before re-wiring the BPF layer.

## Current State

- Enumerates all connected HID devices via `/sys/bus/hid/devices/`
- Reads and hex-dumps raw report descriptors from sysfs
- HID report descriptor parser in progress — will identify device type before attaching any BPF program

BPF attachment, timing detection, and blocking are not present on this branch. See `main` for the working PoC.

## Status

| Component | State |
|---|---|
| Device enumeration | Done |
| Report descriptor read | Done |
| Descriptor parser (keyboard identification) | In progress |
| BPF config map population | Planned |
| HID-BPF struct_ops attachment | Planned |
| Timing detection (Welford variance, BPF-side) | Planned |
| Sysfs unbind blocking | Planned |

## Build

```bash
gcc -o hid_guard hid_guard.c
```

## Usage

```bash
sudo ./hid_guard
```

Prints report descriptors for all currently connected HID devices.

## Requirements

- Linux with sysfs HID support (`/sys/bus/hid/devices/`)
- Root privileges (for reading report descriptors)

## License

GPL v2
