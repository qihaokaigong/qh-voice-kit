#pragma once

// Seeduplex can return a single Base64-encoded PCM event larger than the
// upstream arduinoWebSockets 2.7.2 default of 15 KiB. The N16R8 hardware
// profile has enough memory for a bounded 64 KiB receive frame.
#define QH_WEBSOCKETS_MAX_DATA_SIZE (64 * 1024)
