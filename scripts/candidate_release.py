from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path, PurePath


class CandidateReleaseError(ValueError):
    """Raised before an unsafe or incomplete candidate package is created."""


EXPECTED_ARTIFACTS = {
    "esp32_voice_kit.ino.bootloader.bin": ("bootloader", 0x0),
    "esp32_voice_kit.ino.partitions.bin": ("partition-table", 0x8000),
    "boot_app0.bin": ("ota-data", 0xE000),
    "esp32_voice_kit.ino.bin": ("app", 0x10000),
}


def _flash_entries(build_root: Path) -> list[tuple[int, str]]:
    try:
        lines = (build_root / "flash_args").read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise CandidateReleaseError(f"cannot read build flash_args: {error}") from error

    entries: list[tuple[int, str]] = []
    for line in lines:
        fields = line.split()
        if not fields or fields[0].startswith("--"):
            continue
        if len(fields) != 2:
            raise CandidateReleaseError("flash_args contains an invalid artifact entry")
        try:
            address = int(fields[0], 0)
        except ValueError as error:
            raise CandidateReleaseError("flash_args contains an invalid address") from error
        name = fields[1]
        if PurePath(name).name != name or name not in EXPECTED_ARTIFACTS:
            raise CandidateReleaseError("flash_args contains an unexpected artifact")
        entries.append((address, name))
    return entries


def package_candidate_release(
    build_root: Path, release_root: Path, release_id: str
) -> Path:
    if not release_id.strip():
        raise CandidateReleaseError("release id must be non-empty")
    entries = _flash_entries(build_root)
    if {name for _, name in entries} != set(EXPECTED_ARTIFACTS):
        raise CandidateReleaseError("flash_args does not contain the exact artifact set")
    if release_root.exists() and any(release_root.iterdir()):
        raise CandidateReleaseError("release directory must not already contain files")

    firmware_root = release_root / "firmware"
    firmware_root.mkdir(parents=True, exist_ok=True)
    manifest_files: list[dict[str, object]] = []
    for address, name in sorted(entries):
        role, expected_address = EXPECTED_ARTIFACTS[name]
        if address != expected_address:
            raise CandidateReleaseError(f"unexpected address for artifact {name}")
        source = build_root / name
        try:
            payload = source.read_bytes()
        except OSError as error:
            raise CandidateReleaseError(f"cannot read build artifact {name}: {error}") from error
        if not payload:
            raise CandidateReleaseError(f"build artifact is empty: {name}")
        target = firmware_root / name
        shutil.copyfile(source, target)
        manifest_files.append(
            {
                "role": role,
                "address": address,
                "path": f"firmware/{name}",
                "size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        )

    manifest = {
        "schemaVersion": 1,
        "releaseId": release_id,
        "hardwareProfileId": "qh.voice-kit.breadboard.n16r8.v1",
        "chipFamily": "ESP32-S3",
        "acceptance": {"status": "candidate", "reportId": None},
        "flash": {"eraseAll": False, "files": manifest_files},
    }
    manifest_path = release_root / "release-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--release-id", required=True)
    arguments = parser.parse_args()
    manifest = package_candidate_release(
        arguments.build_root, arguments.output, arguments.release_id
    )
    print(manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
