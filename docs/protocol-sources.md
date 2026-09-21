# Provider protocol sources

The default runtime protocol is implemented against first-party, versioned
material:

- Doubao Realtime Voice Model 3.0 (Seeduplex) integration guide and Realtime
  event protocol:
  <https://www.volcengine.com/docs/6561/2549732?AuditDocumentID=397063&lang=zh>
- Current-console API Key management linked by that guide:
  <https://console.volcengine.com/speech/new/setting/apikeys?projectName=default.>
- Official Python 3.7 duplex demo linked by that guide:
  <https://portal.volccdn.com/obj/volcfe/cloud-universal-doc/upload_148ee77d3245e465d244b912d1e83c91.zip>
- Arduino CLI reproducible sketch profiles:
  <https://docs.arduino.cc/arduino-cli/sketch-project-file/>
- Arduino WebSockets 2.7.2:
  <https://github.com/Links2004/arduinoWebSockets/releases/tag/2.7.2>
- Espressif esptool documentation:
  <https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/>

The checked protocol constants are:

- endpoint: `wss://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue`;
- authentication header: `X-Api-Key`;
- model: `1.2.6.1`;
- input: PCM, 16 kHz, signed 16-bit mono;
- output: `pcm_s16le`, 24 kHz;
- input frame: 320 samples / 640 bytes / 20 ms.

These references and software tests do not constitute a successful call with
user credentials or a real-device voice conversation.
