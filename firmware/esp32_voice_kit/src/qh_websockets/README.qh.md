# Vendored arduinoWebSockets client

This directory contains the client-side source files from
arduinoWebSockets 2.7.2, licensed under the included LGPL-2.1-or-later
`LICENSE` file.

Upstream source:
<https://github.com/Links2004/arduinoWebSockets/releases/tag/2.7.2>

QH carries one bounded compatibility change in `WebSockets.h`: ESP32 receive
frames use `QH_WEBSOCKETS_MAX_DATA_SIZE` from
`../../qh_websocket_limits.h`. The current N16R8 profile sets that limit to
64 KiB because a real Seeduplex response exceeded upstream's 15 KiB limit.
Other platform branches remain byte-for-byte equivalent to upstream 2.7.2.
