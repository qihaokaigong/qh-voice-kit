# QH Voice Kit

Open ESP32-S3 firmware and Release contracts for the QH voice assistant.

The default runtime is deliberately direct:

```text
ESP32 microphone
  -> Doubao Realtime Voice Model 3.0 (Seeduplex), one WebSocket session
  -> ESP32 speaker and ST7789 screen
```

There is no Voice Gateway or computer-side runtime service. The user-owned API
Key and Wi-Fi configuration are written after flashing; they do not belong in
source code, public firmware images, Agent messages, or QH Platform.

## Repository boundary

This repository owns ESP32 firmware, device configuration and provisioning
protocols, the Seeduplex adapter, optional QH structured-data sync, screen UI,
hardware profiles, Release manifests, build assets, and host-testable protocol
and state-machine code. Agent workflow and cross-platform installation live in
the separate `qh-voice-skill` repository.

Legacy ASR, OpenAI-compatible reply, and TTS protocol files remain temporarily
as migration references. The default firmware does not include or call them.

## Current implementation status

The development candidate now includes:

- configuration Schema/Wire v3 with one Doubao realtime API Key;
- SHA-256 checked serial provisioning and double-slot NVS activation;
- direct verified-TLS WebSocket connection to the official Seeduplex endpoint;
- official session-create, mute/unmute, audio append, commit, transcription,
  response text, response PCM, completion, and error event handling;
- push-to-talk half-duplex input using 16 kHz mono PCM in 20 ms / 640 byte
  frames;
- incremental 24 kHz PCM playback through MAX98357A;
- mandatory ST7789 Chinese UI for boot, setup, connection, idle, recording,
  waiting, playback, success, and recoverable errors;
- optional screen transcript/reply text and configurable speaker volume;
- optional best-effort QH structured turn-event sync without raw audio or
  Provider credentials;
- a reproducible N16R8 Arduino profile with pinned networking and display
  dependencies.

The exact hardware profile binds the display to the verified GMT130-V1.0
240x240 module and hardware SPI mode 3. It has no recorded screen-backlight
control pin, so the configuration intentionally does not offer a non-functional
brightness field.

This is still a candidate build. Host contract tests and an Arduino compile are
not proof of a successful real-device conversation. Real hardware display,
Provider, microphone, playback, reconnect, and QH-sync acceptance are still
required before an `allowed` end-user Release can exist.

## Development loop versus release

Firmware iteration uses a local-only fast path. It runs selected host tests,
reuses the persistent Arduino build cache, verifies that the bootloader,
partition table, OTA data image, app address, and exact hardware profile still
match the firmware already installed on the board, then writes only the app
partition. It never erases flash, creates a Candidate, commits, or pushes.

For the currently identified development board:

```bash
python3 scripts/dev_cycle.py --apply \
  --build-root .build/esp32_voice_kit \
  --baseline-manifest .build/releases/<installed-release-id>/release-manifest.json \
  --hardware-profile hardware-profiles/qh.voice-kit.breadboard.n16r8.v1.json \
  --confirm-hardware-profile-id qh.voice-kit.breadboard.n16r8.v1 \
  --port <serial-port> \
  --esptool <path-to-esptool> \
  --test-module tests.test_firmware_cpp \
  --json
```

Repeat `--test-module` for more targeted modules, or omit it to run the full
host suite. Complete command output is stored under `.build/dev-logs/`; stdout
contains only the compact result. If a non-app image or address changes, the
tool refuses app-only flashing and requires a newly reviewed full baseline.

Only after real-device acceptance should the same verified source be run
through the full test suite, clean compile, Candidate packaging, repository
commit/push, and the formal release workflow below.

Run host tests:

```bash
python3 -m unittest discover -s tests -v
```

Compile for the recorded N16R8 profile:

```bash
./scripts/compile.sh
```

Package the exact build output as a non-stable candidate:

```bash
python3 scripts/candidate_release.py \
  --build-root .build/esp32_voice_kit \
  --output .build/releases/<release-id> \
  --release-id <release-id>
```

The packager always writes `acceptance.status: candidate`; promotion requires
separate real-device evidence. Do not infer compatibility from the ESP32-S3
chip family alone.
