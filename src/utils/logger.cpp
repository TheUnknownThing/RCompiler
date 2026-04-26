#include "utils/logger.hpp"

namespace Logger {

const char *getLevelString(Level level) {
  switch (level) {
  case Level::ERROR:
    return "ERROR";
  case Level::WARN:
    return "WARN ";
  case Level::INFO:
    return "INFO ";
  case Level::DEBUG:
    return "DEBUG";
  default:
    return "NONE ";
  }
}
const char *getLevelColor(Level level) {
  switch (level) {
  case Level::ERROR:
    return Color::RED;
  case Level::WARN:
    return Color::YELLOW;
  case Level::INFO:
    return Color::GREEN;
  case Level::DEBUG:
    return Color::BLUE;
  default:
    return Color::RESET;
  }
}
std::string getTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
  ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
  return ss.str();
}
void log(Level level, std::string_view message) {
  if (static_cast<int>(level) <= static_cast<int>(getCurrentLevel())) {
    std::cerr << Color::GRAY << "[" << getTimestamp() << "] "
              << getLevelColor(level) << "[" << getLevelString(level) << "] "
              << Color::RESET << message << std::endl;
  }
}
void error(std::string_view message) { log(Level::ERROR, message); }
void warn(std::string_view message) { log(Level::WARN, message); }
void info(std::string_view message) { log(Level::INFO, message); }
void debug(std::string_view message) { log(Level::DEBUG, message); }

} // namespace Logger
