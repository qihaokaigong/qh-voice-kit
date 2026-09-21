#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(dirname "$script_dir")
sketch="$repo_root/firmware/esp32_voice_kit"
build_path="$repo_root/.build/esp32_voice_kit"
fqbn='esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=default,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default'

if command -v arduino-cli >/dev/null 2>&1; then
  cli=$(command -v arduino-cli)
else
  cli='/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli'
fi

if [ ! -x "$cli" ]; then
  printf '%s\n' 'Arduino CLI is required. Set it on PATH or install Arduino IDE.' >&2
  exit 1
fi

mkdir -p "$build_path"
"$cli" compile --fqbn "$fqbn" --build-path "$build_path" "$sketch"
