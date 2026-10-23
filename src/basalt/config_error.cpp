#include "basalt/config_error.hpp"

#include <string>

namespace basalt {

std::string ConfigError::to_string() const
{
  if (path.empty()) {
    return message;
  }
  std::string result;
  for (const auto &part : path) {
    result += part;
    result += " -> ";
  }
  // Replace the trailing arrow with the separator before the message.
  result.resize(result.size() - 4);
  result += ": ";
  result += message;
  return result;
}

} // namespace basalt
