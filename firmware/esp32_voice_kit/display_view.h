#pragma once

#include <string_view>

namespace qh_voice {

enum class UiState {
  kBooting,
  kSetupRequired,
  kConnecting,
  kIdle,
  kRecording,
  kThinking,
  kPlaying,
  kSuccess,
  kError,
};

struct DisplayView {
  const char* title;
  const char* detail;
};

inline DisplayView displayViewFor(UiState state) {
  switch (state) {
    case UiState::kBooting:
      return {"正在启动", "QH Voice Kit"};
    case UiState::kSetupRequired:
      return {"等待配置", "连接 USB，运行“开始配置”"};
    case UiState::kConnecting:
      return {"正在连接", "请稍候"};
    case UiState::kIdle:
      return {"按住说话", "说完后松开按钮"};
    case UiState::kRecording:
      return {"正在聆听", "松开按钮后发送"};
    case UiState::kThinking:
      return {"正在理解", "正在生成回答"};
    case UiState::kPlaying:
      return {"正在回答", "请听扬声器"};
    case UiState::kSuccess:
      return {"对话完成", "可以继续提问"};
    case UiState::kError:
      return {"出现错误", "请查看恢复提示"};
  }
  return {"出现错误", "请重新启动"};
}

inline DisplayView displayErrorView(std::string_view code) {
  if (code == "provider_auth_failed") {
    return {"连接失败", "请检查 API Key"};
  }
  if (code == "wifi_connect_timeout" || code == "wifi_disconnected") {
    return {"网络异常", "请检查 Wi-Fi"};
  }
  if (code == "microphone_start_failed" ||
      code == "microphone_read_failed") {
    return {"麦克风异常", "请检查接线"};
  }
  if (code == "playback_failed") {
    return {"播放失败", "请检查功放和喇叭"};
  }
  return {"出现错误", "请稍后重试"};
}

}  // namespace qh_voice
