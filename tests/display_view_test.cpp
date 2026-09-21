#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/display_view.h"

int main() {
  const auto idle = qh_voice::displayViewFor(qh_voice::UiState::kIdle);
  assert(std::string(idle.title) == "按住说话");
  assert(std::string(idle.detail).find("松开") != std::string::npos);

  const auto recording =
      qh_voice::displayViewFor(qh_voice::UiState::kRecording);
  assert(std::string(recording.title) == "正在聆听");

  const auto thinking =
      qh_voice::displayViewFor(qh_voice::UiState::kThinking);
  assert(std::string(thinking.title) == "正在理解");

  const auto error = qh_voice::displayErrorView("provider_auth_failed");
  assert(std::string(error.title) == "连接失败");
  assert(std::string(error.detail).find("API Key") != std::string::npos);
  assert(std::string(error.detail).find("secret") == std::string::npos);
  return 0;
}
