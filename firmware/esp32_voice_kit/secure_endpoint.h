#pragma once

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace qh_voice {

struct SecureEndpoint {
  std::string host;
  uint16_t port;
  std::string path;
};

inline std::optional<SecureEndpoint> parseSecureEndpoint(
    std::string_view endpoint, std::string_view scheme,
    uint16_t default_port) {
  if (endpoint.size() <= scheme.size() ||
      endpoint.substr(0, scheme.size()) != scheme ||
      endpoint.find_first_of("\r\n\t #") != std::string_view::npos) {
    return std::nullopt;
  }

  const std::size_t authority_start = scheme.size();
  const std::size_t path_start = endpoint.find_first_of("/?", authority_start);
  const std::string_view authority = endpoint.substr(
      authority_start, path_start == std::string_view::npos
                           ? std::string_view::npos
                           : path_start - authority_start);
  if (authority.empty() || authority.find('@') != std::string_view::npos ||
      authority.front() == '[') {
    return std::nullopt;
  }

  std::string_view host = authority;
  uint16_t port = default_port;
  const std::size_t colon = authority.rfind(':');
  if (colon != std::string_view::npos) {
    host = authority.substr(0, colon);
    const std::string_view port_text = authority.substr(colon + 1);
    if (host.empty() || port_text.empty()) return std::nullopt;
    uint32_t parsed_port = 0;
    for (const unsigned char character : port_text) {
      if (!std::isdigit(character)) return std::nullopt;
      parsed_port = parsed_port * 10 + (character - '0');
      if (parsed_port > 65535) return std::nullopt;
    }
    if (parsed_port == 0) return std::nullopt;
    port = static_cast<uint16_t>(parsed_port);
  }
  if (host.empty()) return std::nullopt;

  std::string path = path_start == std::string_view::npos
                         ? "/"
                         : std::string(endpoint.substr(path_start));
  if (!path.empty() && path.front() == '?') path.insert(path.begin(), '/');
  return SecureEndpoint{std::string(host), port, std::move(path)};
}

}  // namespace qh_voice
