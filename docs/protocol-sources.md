# Provider protocol sources

Implementation must be checked against first-party, versioned material. The
current protocol cores were based on these sources:

- Doubao streaming ASR documentation:
  <https://www.volcengine.com/docs/6561/1354869?lang=zh>
- Doubao ASR new-console `X-Api-Key` authentication example:
  <https://www.volcengine.com/docs/6561/1631584?lang=zh>
- Volcengine official `ai-app-lab` ASR client and binary protocol at commit
  `88c983d70a098110fc839f8cd05e29fa7715e6ce`:
  <https://github.com/volcengine/ai-app-lab>
- Doubao TTS V3 HTTP SSE documentation:
  <https://www.volcengine.com/docs/6561/1598757?lang=zh>
- ByteDance official AgentKit TTS sample:
  <https://github.com/bytedance/agentkit-samples/blob/main/skills/byted-text-to-speech/scripts/text_to_speech.py>
- Arduino CLI reproducible sketch profiles:
  <https://docs.arduino.cc/arduino-cli/sketch-project-file/>
- Arduino WebSockets release 2.7.2:
  <https://github.com/Links2004/arduinoWebSockets/releases/tag/2.7.2>
- Espressif esptool documentation:
  <https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/>

These references support protocol implementation and host tests. They do not
constitute a successful call with user credentials or a real-device voice
conversation.
