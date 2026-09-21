from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RepositoryContractsTest(unittest.TestCase):
    def test_hardware_profile_matches_recorded_reference_build(self) -> None:
        profile = json.loads(
            (
                ROOT
                / "hardware-profiles"
                / "qh.voice-kit.breadboard.n16r8.v1.json"
            ).read_text(encoding="utf-8")
        )

        self.assertEqual(profile["profileId"], "qh.voice-kit.breadboard.n16r8.v1")
        self.assertEqual(profile["status"], "candidate")
        self.assertEqual(profile["chipFamily"], "ESP32-S3")
        self.assertEqual(profile["memory"]["flashBytes"], 16 * 1024 * 1024)
        self.assertEqual(profile["memory"]["psramMode"], "opi")
        self.assertEqual(profile["pins"]["microphone"], {"sck": 4, "ws": 5, "sd": 6})
        self.assertEqual(profile["pins"]["speaker"], {"bclk": 16, "lrc": 17, "din": 18})
        self.assertIn("FlashSize=16M", profile["arduino"]["fqbn"])
        self.assertIn("PSRAM=opi", profile["arduino"]["fqbn"])
        self.assertIn(
            "PartitionScheme=app3M_fat9M_16MB", profile["arduino"]["fqbn"]
        )

    def test_sketch_profile_pins_core_and_websocket_library(self) -> None:
        profile = (
            ROOT / "firmware" / "esp32_voice_kit" / "sketch.yaml"
        ).read_text(encoding="utf-8")

        self.assertIn("platform: esp32:esp32 (3.3.11)", profile)
        self.assertIn("WebSockets (2.7.2)", profile)
        self.assertIn("PartitionScheme=app3M_fat9M_16MB", profile)

    def test_device_config_schema_marks_every_secret_write_only(self) -> None:
        schema = json.loads(
            (ROOT / "schemas" / "device-config.schema.json").read_text(
                encoding="utf-8"
            )
        )

        paths = [
            ("network", "password"),
            ("stt", "credential"),
            ("reply", "credential"),
            ("tts", "credential"),
            ("qhSync", "credential"),
        ]
        for section, field in paths:
            with self.subTest(section=section, field=field):
                definition = schema["properties"][section]["properties"][field]
                self.assertTrue(definition["writeOnly"])
                self.assertTrue(definition["x-qh-secret"])

        self.assertEqual(
            schema["properties"]["stt"]["properties"]["endpoint"]["pattern"],
            "^wss://",
        )
        self.assertEqual(
            schema["properties"]["reply"]["properties"]["endpoint"]["pattern"],
            "^https://",
        )


if __name__ == "__main__":
    unittest.main()
