# eBPF-HID

A Linux kernel-space security monitor for detecting and blocking malicious HID (Human Interface Device) attacks in real-time using eBPF.

## Overview

This project detects rapid keystroke patterns characteristic of automated attacks (Rubber Ducky, BadUSB, compromised Bluetooth devices) and blocks the offending device by unbinding it from the kernel HID driver. It tracks both USB and Bluetooth peripherals, ignoring pre-existing devices to focus only on newly connected threats.

## Requirements

- Linux kernel ≥ 5.15 (with HID-BPF support enabled)
- `libelf-dev` and `libz-dev`
- LLVM/Clang (for eBPF compilation)
- libbpf development headers
- Root privileges to run

## Build

```bash
make
```

## Usage

```bash
sudo ./main [hid_id]
```

- **Auto-detection**: Prompts for USB or Bluetooth device connection
- **Manual override**: Supply `hid_id` directly (e.g., `sudo ./main 5`)

## Configuration

Tunable thresholds in `main.c`:
- `ATTACK_MAX_MS` — Keystroke interval indicating attack (default: 5ms)
- `HUMAN_MIN_MS` — Minimum interval for human typing (default: 30ms)
- `ALERT_THRESHOLD` — Consecutive suspicious events before blocking (default: 3)

## License

GPL v2
