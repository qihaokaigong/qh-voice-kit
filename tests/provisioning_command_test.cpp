#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/provisioning_command.h"

int main() {
  const std::string digest(64, 'a');
  auto command = qh_voice::parseProvisioningCommand(
      "QH_VOICE_CONFIG 4096 " + digest);
  assert(command.type == qh_voice::ProvisioningCommandType::kConfig);
  assert(command.payload_size == 4096);
  assert(command.sha256 == digest);

  command = qh_voice::parseProvisioningCommand("QH_VOICE_STATUS");
  assert(command.type == qh_voice::ProvisioningCommandType::kStatus);

  command = qh_voice::parseProvisioningCommand(
      "QH_VOICE_CONFIG 9000 " + digest);
  assert(command.type == qh_voice::ProvisioningCommandType::kInvalid);

  command = qh_voice::parseProvisioningCommand(
      "QH_VOICE_CONFIG 10 ../not-a-digest");
  assert(command.type == qh_voice::ProvisioningCommandType::kInvalid);

  command = qh_voice::parseProvisioningCommand(
      "QH_VOICE_CONFIG 10 " + digest + " trailing");
  assert(command.type == qh_voice::ProvisioningCommandType::kInvalid);
  return 0;
}
