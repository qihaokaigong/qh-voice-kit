from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    "button_debouncer_test.cpp",
    "config_transaction_test.cpp",
    "device_config_wire_test.cpp",
    "device_config_test.cpp",
    "doubao_asr_protocol_test.cpp",
    "doubao_tts_protocol_test.cpp",
    "doubao_tts_sse_test.cpp",
    "openai_reply_protocol_test.cpp",
    "pcm_audio_test.cpp",
    "provider_request_test.cpp",
    "provisioning_command_test.cpp",
    "qh_sync_protocol_test.cpp",
    "secure_endpoint_test.cpp",
    "voice_turn_state_test.cpp",
]


class FirmwareHostContractsTest(unittest.TestCase):
    def test_contracts_compile_and_run(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for source_name in SOURCES:
                with self.subTest(source=source_name):
                    source = ROOT / "tests" / source_name
                    executable = output / source.stem
                    subprocess.run(
                        [
                            "c++",
                            "-std=c++17",
                            "-Wall",
                            "-Wextra",
                            "-Werror",
                            str(source),
                            "-o",
                            str(executable),
                        ],
                        cwd=ROOT,
                        check=True,
                    )
                    subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
