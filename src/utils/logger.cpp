#include "utils/logger.hpp"

namespace logger {

const char *get_level_string(Level level) {
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
const char *get_level_color(Level level) {
  switch (level) {
  case Level::ERROR:
    return color::RED;
  case Level::WARN:
    return color::YELLOW;
  case Level::INFO:
    return color::GREEN;
  case Level::DEBUG:
    return color::BLUE;
  default:
    return color::RESET;
  }
}
std::string get_timestamp() {
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
  if (static_cast<int>(level) <= static_cast<int>(get_current_level())) {
    std::cerr << color::GRAY << "[" << get_timestamp() << "] "
              << get_level_color(level) << "[" << get_level_string(level) << "] "
              << color::RESET << message << std::endl;
  }
}
void error(std::string_view message) { log(Level::ERROR, message); }
void warn(std::string_view message) { log(Level::WARN, message); }
void info(std::string_view message) { log(Level::INFO, message); }
void debug(std::string_view message) { log(Level::DEBUG, message); }

} // namespace logger
