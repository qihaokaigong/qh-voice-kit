#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qh_voice {

struct HeaderValue {
  std::string name;
  std::string value;
};

struct ProviderTransportRequest {
  std::string endpoint;
  std::vector<HeaderValue> headers;
};

inline ProviderTransportRequest buildDoubaoAsrTransportRequest(
    std::string endpoint, std::string api_key, std::string resource_id,
    std::string request_id) {
  return {
      std::move(endpoint),
      {{"X-Api-Key", std::move(api_key)},
       {"X-Api-Resource-Id", std::move(resource_id)},
       {"X-Api-Connect-Id", std::move(request_id)}}};
}

inline ProviderTransportRequest buildOpenAiReplyTransportRequest(
    std::string endpoint, std::string credential) {
  return {std::move(endpoint),
          {{"Authorization", "Bearer " + credential},
           {"Content-Type", "application/json"}}};
}

inline ProviderTransportRequest buildDoubaoTtsTransportRequest(
    std::string endpoint, std::string credential, std::string resource_id,
    std::string request_id) {
  return {
      std::move(endpoint),
      {{"X-Api-Key", std::move(credential)},
       {"X-Api-Resource-Id", std::move(resource_id)},
       {"X-Api-Request-Id", std::move(request_id)},
       {"Accept", "text/event-stream"},
       {"Content-Type", "application/json"}}};
}

inline std::string redactedTransportSummary(
    std::string_view provider, const ProviderTransportRequest& request) {
  return "provider=" + std::string(provider) + " endpoint=" +
         request.endpoint + " headers=" +
         std::to_string(request.headers.size());
}

}  // namespace qh_voice
