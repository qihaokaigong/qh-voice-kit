# QH Voice Kit

Open ESP32-S3 firmware and Release contracts for the QH voice assistant.

The target runtime is deliberately direct:

```text
ESP32 -> STT Provider -> Reply Provider -> TTS Provider -> ESP32
```

There is no Voice Gateway or computer-side runtime service. User configuration is written after flashing and does not belong in source code or public firmware images.

## Repository boundary

This repository owns:

- ESP32 firmware;
- device configuration and provisioning protocols;
- STT, reply, TTS, and QH structured-data adapters;
- hardware profile, Release Manifest, build, and acceptance assets;
- host-testable protocol and state-machine code.

Agent workflow and cross-platform installer orchestration live in the separate `qh-voice-skill` repository.

## Current implementation status

The repository now contains a compilable ESP32-S3 direct-Provider voice-loop
candidate and host-tested protocol cores:

- versioned device configuration plus a JSON Schema;
- current-console Doubao ASR authentication with a single `X-Api-Key` (legacy
  APP ID + Access Token configuration is not part of this candidate);
- a compact serial wire format shared with `qh-voice-skill`;
- SHA-256 checked serial provisioning;
- double-slot NVS writes with readback before activation;
- Doubao streaming ASR framing and response parsing;
- OpenAI-compatible reply request and response handling;
- Doubao TTS V3 request and fragmented SSE response handling;
- verified TLS via the ESP32 root-CA bundle, with NTP synchronization before
  Provider calls;
- hold-to-record INMP441 capture, PCM conversion, and Doubao ASR upload;
- OpenAI-compatible reply generation, Doubao PCM speech synthesis, and
  MAX98357A playback;
- optional QH structured turn-event upload that never contains raw audio or
  Provider credentials and never changes the local turn result;
- an explicit runtime state machine and redacted serial status/error output;
- a reproducible Arduino build profile pinned to ESP32 core 3.3.11 and
  WebSockets 2.7.2;
- the recorded N16R8 hardware profile.

This is still a candidate development build. Candidate Release packaging is
available, but the display UI, durable QH sync outbox/retry path, deterministic
layered health checks, and real-device Provider/playback acceptance are not
complete. There is therefore no `allowed` end-user Release yet. A successful
compile or candidate package is not a successful real conversation.

Run all host contract tests with:

```bash
python3 -m unittest discover -s tests -v
```

Compile the current voice-loop candidate for the recorded N16R8 profile. The
Arduino profile downloads pinned build dependencies into its isolated cache on
the first run:

```bash
./scripts/compile.sh
```

Package the exact build output as a non-stable candidate for controlled
hardware acceptance:

```bash
python3 scripts/candidate_release.py \
  --build-root .build/esp32_voice_kit \
  --output .build/releases/<release-id> \
  --release-id <release-id>
```

The packager reads the compiler-produced `flash_args`, requires the exact N16R8
artifact set and addresses, copies only those binaries, and records their sizes
and SHA-256 values. It always writes `acceptance.status: candidate`; promotion
requires separate real-device evidence.

The exact first hardware profile remains the recorded ESP32-S3 N16R8 reference build. Do not infer compatibility from the chip family alone.
