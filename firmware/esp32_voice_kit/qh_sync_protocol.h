#pragma once

#include <string>

#include "json_string.h"
#include "provider_request.h"

namespace qh_voice {

struct QhConversationTurn {
  std::string conversation_id;
  std::string turn_id;
  std::string occurred_at;
  std::string transcript;
  std::string reply;
};

inline std::string buildQhConversationEventBody(
    const QhConversationTurn& turn) {
  const std::string conversation_id = escapeJsonString(turn.conversation_id);
  const std::string turn_id = escapeJsonString(turn.turn_id);
  const std::string occurred_at = escapeJsonString(turn.occurred_at);
  const std::string event_prefix = escapeJsonString(turn.turn_id + ".");
  return
      "{\"events\":[{\"eventId\":\"" + event_prefix +
      "started\",\"conversationId\":\"" + conversation_id +
      "\",\"turnId\":\"" + turn_id +
      "\",\"type\":\"turn.started\",\"occurredAt\":\"" + occurred_at +
      "\"},{\"eventId\":\"" + event_prefix +
      "transcribed\",\"conversationId\":\"" + conversation_id +
      "\",\"turnId\":\"" + turn_id +
      "\",\"type\":\"input.transcribed\",\"occurredAt\":\"" +
      occurred_at + "\",\"text\":\"" + escapeJsonString(turn.transcript) +
      "\"},{\"eventId\":\"" + event_prefix +
      "reply\",\"conversationId\":\"" + conversation_id +
      "\",\"turnId\":\"" + turn_id +
      "\",\"type\":\"reply.generated\",\"occurredAt\":\"" +
      occurred_at + "\",\"text\":\"" + escapeJsonString(turn.reply) +
      "\"}]}";
}

inline ProviderTransportRequest buildQhSyncTransportRequest(
    std::string endpoint, std::string credential) {
  return {std::move(endpoint),
          {{"Authorization", "Bearer " + credential},
           {"Content-Type", "application/json"}}};
}

}  // namespace qh_voice
