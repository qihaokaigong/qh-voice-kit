#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/openai_reply_protocol.h"

namespace {

void buildsSingleTurnRequestWithoutCredential() {
  qh_voice::OpenAiReplyRequest request{
      "model-a", "Answer in one line.\nNo markdown.", "He said \"hello\".", 120};

  const std::string body = qh_voice::buildOpenAiReplyBody(request);

  assert(body.find("\"model\":\"model-a\"") != std::string::npos);
  assert(body.find("Answer in one line.\\nNo markdown.") != std::string::npos);
  assert(body.find("He said \\\"hello\\\".") != std::string::npos);
  assert(body.find("\"max_tokens\":120") != std::string::npos);
  assert(body.find("api-key") == std::string::npos);
}

void parsesAssistantContent() {
  const std::string response = R"json({
    "id":"request-1",
    "choices":[{"index":0,"message":{"role":"assistant","content":"你好，\n世界！"}}]
  })json";

  const auto parsed = qh_voice::parseOpenAiReply(response);

  assert(parsed.has_value());
  assert(parsed->text == "你好，\n世界！");
}

void rejectsMissingOrBlankAssistantContent() {
  assert(!qh_voice::parseOpenAiReply("{}").has_value());
  assert(!qh_voice::parseOpenAiReply(
              R"json({"choices":[{"message":{"content":"   "}}]})json")
              .has_value());
}

void mapsHttpFailuresToStableCodes() {
  assert(qh_voice::mapReplyHttpStatus(401) == qh_voice::ReplyError::kAuth);
  assert(qh_voice::mapReplyHttpStatus(403) == qh_voice::ReplyError::kAuth);
  assert(qh_voice::mapReplyHttpStatus(429) == qh_voice::ReplyError::kRateLimited);
  assert(qh_voice::mapReplyHttpStatus(500) == qh_voice::ReplyError::kProvider);
  assert(qh_voice::mapReplyHttpStatus(200) == qh_voice::ReplyError::kNone);
  assert(qh_voice::mapReplyHttpStatus(418) == qh_voice::ReplyError::kProtocol);
}

}  // namespace

int main() {
  buildsSingleTurnRequestWithoutCredential();
  parsesAssistantContent();
  rejectsMissingOrBlankAssistantContent();
  mapsHttpFailuresToStableCodes();
  return 0;
}
