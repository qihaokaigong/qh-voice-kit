from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DeviceConfigSchemaTest(unittest.TestCase):
    def test_realtime_voice_uses_one_api_key_and_schema_three(self) -> None:
        schema = json.loads(
            (ROOT / "schemas/device-config.schema.json").read_text(encoding="utf-8")
        )

        self.assertEqual(schema["properties"]["schemaVersion"]["const"], 3)
        self.assertIn("realtimeVoice", schema["required"])
        self.assertNotIn("stt", schema["properties"])
        self.assertNotIn("reply", schema["properties"])
        self.assertNotIn("tts", schema["properties"])
        realtime = schema["properties"]["realtimeVoice"]
        self.assertEqual(
            realtime["properties"]["adapter"]["const"], "doubao-seeduplex-v1"
        )
        self.assertEqual(realtime["required"], ["adapter", "apiKey", "voice"])
        self.assertTrue(realtime["properties"]["apiKey"]["x-qh-secret"])
        self.assertNotIn("endpoint", realtime["properties"])

    def test_does_not_offer_unsupported_screen_brightness_setting(self) -> None:
        schema = json.loads(
            (ROOT / "schemas/device-config.schema.json").read_text(encoding="utf-8")
        )

        preferences = schema["properties"]["preferences"]
        self.assertEqual(preferences["required"], ["volumePercent"])
        self.assertNotIn("screenBrightnessPercent", preferences["properties"])


if __name__ == "__main__":
    unittest.main()
