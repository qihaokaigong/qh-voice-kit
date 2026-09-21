from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from scripts.candidate_release import CandidateReleaseError, package_candidate_release


class CandidateReleaseTest(unittest.TestCase):
    def make_build(self, root: Path) -> dict[str, bytes]:
        artifacts = {
            "esp32_voice_kit.ino.bootloader.bin": b"bootloader",
            "esp32_voice_kit.ino.partitions.bin": b"partitions",
            "boot_app0.bin": b"boot-app",
            "esp32_voice_kit.ino.bin": b"application",
        }
        for name, payload in artifacts.items():
            (root / name).write_bytes(payload)
        (root / "flash_args").write_text(
            "--flash-mode dio --flash-freq 80m --flash-size 16MB\n"
            "0x0 esp32_voice_kit.ino.bootloader.bin\n"
            "0x8000 esp32_voice_kit.ino.partitions.bin\n"
            "0xe000 boot_app0.bin\n"
            "0x10000 esp32_voice_kit.ino.bin\n",
            encoding="utf-8",
        )
        return artifacts

    def test_packages_exact_build_artifacts_with_hashes_and_candidate_status(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            release = root / "release"
            build.mkdir()
            artifacts = self.make_build(build)

            manifest_path = package_candidate_release(
                build,
                release,
                "qh-voice-kit-0.1.0-candidate.6966d9c",
            )

            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["acceptance"]["status"], "candidate")
            self.assertIsNone(manifest["acceptance"]["reportId"])
            self.assertFalse(manifest["flash"]["eraseAll"])
            self.assertEqual(
                [item["address"] for item in manifest["flash"]["files"]],
                [0, 0x8000, 0xE000, 0x10000],
            )
            for item in manifest["flash"]["files"]:
                name = Path(item["path"]).name
                self.assertEqual(item["size"], len(artifacts[name]))
                self.assertEqual(
                    item["sha256"], hashlib.sha256(artifacts[name]).hexdigest()
                )

    def test_refuses_unexpected_or_escaping_flash_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            self.make_build(build)
            (build / "flash_args").write_text(
                "0x0 ../outside.bin\n", encoding="utf-8"
            )

            with self.assertRaisesRegex(CandidateReleaseError, "artifact"):
                package_candidate_release(build, root / "release", "candidate-1")


if __name__ == "__main__":
    unittest.main()
