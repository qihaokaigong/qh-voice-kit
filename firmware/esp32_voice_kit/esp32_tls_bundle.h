#pragma once

#include <NetworkClientSecure.h>

#include <cstddef>
#include <cstdint>

namespace qh_voice {

extern const uint8_t kEspRootCaBundleStart[]
    asm("_binary_x509_crt_bundle_start");
extern const uint8_t kEspRootCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

inline const uint8_t* rootCaBundleData() { return kEspRootCaBundleStart; }

inline std::size_t rootCaBundleSize() {
  return static_cast<std::size_t>(kEspRootCaBundleEnd -
                                  kEspRootCaBundleStart);
}

inline void enableRootCaBundle(NetworkClientSecure& client) {
  client.setCACertBundle(rootCaBundleData(), rootCaBundleSize());
}

}  // namespace qh_voice
