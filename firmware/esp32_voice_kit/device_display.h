#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <string>
#include <string_view>

#include "display_view.h"

namespace qh_voice {

class DeviceDisplay {
 public:
  DeviceDisplay()
      : panel_(&SPI, kChipSelectPin, kDataCommandPin, kResetPin) {}

  void begin() {
    SPI.begin(kClockPin, -1, kMosiPin, kChipSelectPin);
    panel_.init(240, 240, SPI_MODE3);
    panel_.setRotation(0);
    panel_.fillScreen(ST77XX_BLACK);
    text_.begin(panel_);
    text_.setFontMode(1);
    text_.setFontDirection(0);
    ready_ = true;
    show(UiState::kBooting);
  }

  void show(UiState state) {
    const auto view = displayViewFor(state);
    showView(view, colorFor(state));
  }

  void showError(std::string_view code) {
    showView(displayErrorView(code), ST77XX_RED);
  }

  void showConversation(std::string_view title, std::string_view text,
                        uint16_t color = ST77XX_CYAN) {
    const std::string title_copy(title);
    const std::string text_copy(text);
    showView({title_copy.c_str(), text_copy.c_str()}, color);
  }

 private:
  static constexpr int kClockPin = 9;
  static constexpr int kMosiPin = 10;
  static constexpr int kResetPin = 11;
  static constexpr int kDataCommandPin = 12;
  static constexpr int kChipSelectPin = -1;

  static uint16_t colorFor(UiState state) {
    switch (state) {
      case UiState::kRecording: return ST77XX_RED;
      case UiState::kThinking: return ST77XX_YELLOW;
      case UiState::kPlaying: return ST77XX_GREEN;
      case UiState::kSuccess: return ST77XX_GREEN;
      case UiState::kError: return ST77XX_RED;
      default: return ST77XX_CYAN;
    }
  }

  String fitLine(const char* value, int16_t maximum_width) {
    String line(value == nullptr ? "" : value);
    while (!line.isEmpty() && text_.getUTF8Width(line.c_str()) > maximum_width) {
      int index = line.length() - 1;
      while (index > 0 &&
             (static_cast<uint8_t>(line[index]) & 0xC0U) == 0x80U) {
        --index;
      }
      line.remove(index);
    }
    return line;
  }

  void drawCentered(const char* value, int16_t baseline, uint16_t color,
                    const uint8_t* font) {
    text_.setFont(font);
    text_.setForegroundColor(color);
    const String line = fitLine(value, 216);
    const int16_t width = text_.getUTF8Width(line.c_str());
    text_.setCursor((240 - width) / 2, baseline);
    text_.print(line);
  }

  void showView(DisplayView view, uint16_t accent) {
    if (!ready_) return;
    const std::string title(view.title == nullptr ? "" : view.title);
    const std::string detail(view.detail == nullptr ? "" : view.detail);
    if (title == last_title_ && detail == last_detail_ &&
        accent == last_accent_) {
      return;
    }
    last_title_ = title;
    last_detail_ = detail;
    last_accent_ = accent;

    panel_.fillScreen(ST77XX_BLACK);
    panel_.drawRoundRect(12, 20, 216, 200, 14, accent);
    panel_.fillCircle(120, 56, 7, accent);
    drawCentered(last_title_.c_str(), 112, accent,
                 u8g2_font_wqy16_t_gb2312b);
    drawCentered(last_detail_.c_str(), 166, ST77XX_WHITE,
                 u8g2_font_wqy12_t_gb2312b);
  }

  Adafruit_ST7789 panel_;
  U8G2_FOR_ADAFRUIT_GFX text_;
  bool ready_ = false;
  std::string last_title_;
  std::string last_detail_;
  uint16_t last_accent_ = 0;
};

}  // namespace qh_voice
