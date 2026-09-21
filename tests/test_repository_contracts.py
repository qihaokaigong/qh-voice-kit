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
        self.assertEqual(profile["pins"]["display"].get("module"), "GMT130-V1.0")
        self.assertEqual(profile["pins"]["display"].get("spiMode"), 3)
        self.assertIn("FlashSize=16M", profile["arduino"]["fqbn"])
        self.assertIn("PSRAM=opi", profile["arduino"]["fqbn"])
        self.assertIn(
            "PartitionScheme=app3M_fat9M_16MB", profile["arduino"]["fqbn"]
        )

    def test_sketch_profile_pins_core_and_display_libraries(self) -> None:
        profile = (
            ROOT / "firmware" / "esp32_voice_kit" / "sketch.yaml"
        ).read_text(encoding="utf-8")

        self.assertIn("platform: esp32:esp32 (3.3.11)", profile)
        self.assertIn("Adafruit BusIO (1.17.4)", profile)
        self.assertIn("Adafruit GFX Library (1.12.6)", profile)
        self.assertIn("Adafruit ST7735 and ST7789 Library (1.11.0)", profile)
        self.assertIn("U8g2_for_Adafruit_GFX (1.8.0)", profile)
        self.assertIn("PartitionScheme=app3M_fat9M_16MB", profile)

    def test_vendored_websocket_client_accepts_large_realtime_frames(self) -> None:
        sketch = ROOT / "firmware" / "esp32_voice_kit"
        profile = (sketch / "sketch.yaml").read_text(encoding="utf-8")
        limits = sketch / "qh_websocket_limits.h"
        vendored_header = sketch / "src" / "qh_websockets" / "WebSockets.h"
        license_file = sketch / "src" / "qh_websockets" / "LICENSE"

        self.assertTrue(limits.is_file())
        self.assertTrue(vendored_header.is_file())
        self.assertTrue(license_file.is_file())
        self.assertNotIn("WebSockets (2.7.2)", profile)
        self.assertIn(
            "#define QH_WEBSOCKETS_MAX_DATA_SIZE (64 * 1024)",
            limits.read_text(encoding="utf-8"),
        )
        self.assertIn(
            "#define WEBSOCKETS_MAX_DATA_SIZE QH_WEBSOCKETS_MAX_DATA_SIZE",
            vendored_header.read_text(encoding="utf-8"),
        )

    def test_display_uses_verified_hardware_spi_mode_for_gmt130(self) -> None:
        display = (
            ROOT / "firmware" / "esp32_voice_kit" / "device_display.h"
        ).read_text(encoding="utf-8")

        spi_begin = "SPI.begin(kClockPin, -1, kMosiPin, kChipSelectPin);"
        panel_init = "panel_.init(240, 240, SPI_MODE3);"
        self.assertIn(spi_begin, display)
        self.assertIn(panel_init, display)
        self.assertLess(display.index(spi_begin), display.index(panel_init))

    def test_device_config_schema_marks_every_secret_write_only(self) -> None:
        schema = json.loads(
            (ROOT / "schemas" / "device-config.schema.json").read_text(
                encoding="utf-8"
            )
        )

        paths = [
            ("network", "password"),
            ("realtimeVoice", "apiKey"),
            ("qhSync", "credential"),
        ]
        for section, field in paths:
            with self.subTest(section=section, field=field):
                definition = schema["properties"][section]["properties"][field]
                self.assertTrue(definition["writeOnly"])
                self.assertTrue(definition["x-qh-secret"])

        self.assertNotIn(
            "endpoint", schema["properties"]["realtimeVoice"]["properties"]
        )


if __name__ == "__main__":
    unittest.main()
