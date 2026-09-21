#include <cassert>

#include "../firmware/esp32_voice_kit/button_debouncer.h"

int main() {
  qh_voice::ButtonDebouncer button(true, 40);
  button.begin(false, 100);
  assert(!button.update(true, 110));
  assert(!button.update(true, 149));
  assert(button.update(true, 150));
  assert(button.changedToPressed());
  assert(!button.changedToReleased());

  assert(!button.update(false, 160));
  assert(button.update(false, 200));
  assert(button.changedToReleased());
  assert(!button.changedToPressed());
  return 0;
}
