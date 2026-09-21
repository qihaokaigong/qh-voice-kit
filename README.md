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

The repository now contains a compilable ESP32-S3 provisioning firmware and
host-tested protocol cores:

- versioned device configuration plus a JSON Schema;
- a compact serial wire format shared with `qh-voice-skill`;
- SHA-256 checked serial provisioning;
- double-slot NVS writes with readback before activation;
- Doubao streaming ASR framing and response parsing;
- OpenAI-compatible reply request and response handling;
- Doubao TTS V3 request and fragmented SSE response handling;
- the recorded N16R8 hardware profile.

This is still a candidate development build. Provider network transports,
microphone/display/speaker integration into the new sketch, the full voice
state machine, Release generation, and real-device acceptance are not complete.
There is therefore no `allowed` end-user Release yet.

Run all host contract tests with:

```bash
python3 -m unittest discover -s tests -v
```

Compile the current provisioning firmware for the recorded N16R8 profile:

```bash
./scripts/compile.sh
```

The exact first hardware profile remains the recorded ESP32-S3 N16R8 reference build. Do not infer compatibility from the chip family alone.
