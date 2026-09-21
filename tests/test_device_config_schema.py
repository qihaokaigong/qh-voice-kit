from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DeviceConfigSchemaTest(unittest.TestCase):
    def test_new_console_asr_uses_one_api_key(self) -> None:
        schema = json.loads(
            (ROOT / "schemas/device-config.schema.json").read_text(encoding="utf-8")
        )

        self.assertEqual(schema["properties"]["schemaVersion"]["const"], 2)
        stt = schema["properties"]["stt"]
        self.assertIn("apiKey", stt["required"])
        self.assertNotIn("appKey", stt["properties"])
        self.assertNotIn("credential", stt["properties"])
        self.assertTrue(stt["properties"]["apiKey"]["x-qh-secret"])


if __name__ == "__main__":
    unittest.main()
