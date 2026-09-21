import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from scripts import dev_cycle


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "dev_cycle.py"


class DevCycleCliTests(unittest.TestCase):
    def _fixture(self, root: Path) -> tuple[Path, Path, Path]:
        build = root / "build"
        build.mkdir()
        artifacts = {
            "esp32_voice_kit.ino.bootloader.bin": b"boot",
            "esp32_voice_kit.ino.partitions.bin": b"parts",
            "boot_app0.bin": b"ota",
            "esp32_voice_kit.ino.bin": b"new-app",
        }
        for name, payload in artifacts.items():
            (build / name).write_bytes(payload)
        (build / "flash_args").write_text(
            "--flash-mode dio --flash-freq 80m --flash-size 16MB\n"
            "0x0 esp32_voice_kit.ino.bootloader.bin\n"
            "0x8000 esp32_voice_kit.ino.partitions.bin\n"
            "0xe000 boot_app0.bin\n"
            "0x10000 esp32_voice_kit.ino.bin\n",
            encoding="utf-8",
        )
        profile = root / "profile.json"
        profile.write_text(
            json.dumps({"profileId": "qh.voice-kit.breadboard.n16r8.v1"}),
            encoding="utf-8",
        )
        role_by_name = {
            "esp32_voice_kit.ino.bootloader.bin": "bootloader",
            "esp32_voice_kit.ino.partitions.bin": "partition-table",
            "boot_app0.bin": "ota-data",
            "esp32_voice_kit.ino.bin": "app",
        }
        address_by_name = {
            "esp32_voice_kit.ino.bootloader.bin": 0,
            "esp32_voice_kit.ino.partitions.bin": 0x8000,
            "boot_app0.bin": 0xE000,
            "esp32_voice_kit.ino.bin": 0x10000,
        }
        baseline = root / "release-manifest.json"
        baseline.write_text(
            json.dumps(
                {
                    "schemaVersion": 1,
                    "releaseId": "candidate.baseline",
                    "hardwareProfileId": "qh.voice-kit.breadboard.n16r8.v1",
                    "flash": {
                        "eraseAll": False,
                        "files": [
                            {
                                "role": role_by_name[name],
                                "address": address_by_name[name],
                                "path": f"firmware/{name}",
                                "size": len(payload),
                                "sha256": hashlib.sha256(payload).hexdigest(),
                            }
                            for name, payload in artifacts.items()
                        ],
                    },
                }
            ),
            encoding="utf-8",
        )
        return build, profile, baseline

    def test_plan_is_app_only_and_verifies_non_app_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, profile, baseline = self._fixture(root)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--plan-only",
                    "--build-root",
                    str(build),
                    "--baseline-manifest",
                    str(baseline),
                    "--hardware-profile",
                    str(profile),
                    "--confirm-hardware-profile-id",
                    "qh.voice-kit.breadboard.n16r8.v1",
                    "--port",
                    "/dev/cu.test",
                    "--json",
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            self.assertEqual(payload["status"], "dev_plan_ready")
            self.assertEqual(payload["flashMode"], "app-only")
            self.assertFalse(payload["eraseAll"])
            self.assertEqual(payload["appAddress"], "0x10000")
            command = payload["command"]
            self.assertIn("esp32_voice_kit.ino.bin", " ".join(command))
            self.assertNotIn("bootloader", " ".join(command))
            self.assertNotIn("partitions", " ".join(command))
            self.assertNotIn("boot_app0", " ".join(command))
            self.assertNotIn("erase-flash", command)

    def test_apply_runs_tests_compile_and_app_flash_with_one_quiet_result(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, profile, baseline = self._fixture(root)
            log_path = root / "dev-cycle.log"
            calls: list[list[str]] = []

            def fake_runner(command, **kwargs):
                calls.append([str(item) for item in command])
                kwargs["stdout"].write(f"verbose output for {command[0]}\n")
                return SimpleNamespace(returncode=0)

            result = dev_cycle.run_dev_cycle(
                repo_root=REPO_ROOT,
                build_root=build,
                baseline_manifest=baseline,
                hardware_profile=profile,
                confirmed_profile_id="qh.voice-kit.breadboard.n16r8.v1",
                port="/dev/cu.test",
                esptool="fake-esptool",
                test_modules=["tests.test_firmware_cpp"],
                log_path=log_path,
                runner=fake_runner,
            )

            self.assertEqual(result["status"], "dev_flash_complete")
            self.assertEqual(result["flashMode"], "app-only")
            self.assertEqual(result["logPath"], str(log_path.resolve()))
            self.assertEqual(len(calls), 3)
            self.assertEqual(calls[0][1:4], ["-m", "unittest", "-v"])
            self.assertIn("tests.test_firmware_cpp", calls[0])
            self.assertEqual(calls[1], [str(REPO_ROOT / "scripts" / "compile.sh")])
            self.assertEqual(calls[2][0], "fake-esptool")
            self.assertIn("esp32_voice_kit.ino.bin", " ".join(calls[2]))
            self.assertNotIn("bootloader", " ".join(calls[2]))
            self.assertTrue(log_path.read_text(encoding="utf-8").startswith("$ "))

    def test_refuses_app_only_flash_when_partition_image_changed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, profile, baseline = self._fixture(root)
            (build / "esp32_voice_kit.ino.partitions.bin").write_bytes(b"changed")

            with self.assertRaisesRegex(
                dev_cycle.DevCycleError,
                "partition-table changed",
            ):
                dev_cycle.build_plan(
                    build,
                    baseline,
                    profile,
                    "qh.voice-kit.breadboard.n16r8.v1",
                    "/dev/cu.test",
                    "esptool",
                )


if __name__ == "__main__":
    unittest.main()
