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

The exact hardware profile has no recorded screen-backlight control pin, so the
configuration intentionally does not offer a non-functional brightness field.

This is still a candidate build. Host contract tests and an Arduino compile are
not proof of a successful real-device conversation. Real hardware display,
Provider, microphone, playback, reconnect, and QH-sync acceptance are still
required before an `allowed` end-user Release can exist.

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
