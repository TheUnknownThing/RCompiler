#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace Logger {

enum class Level { NONE = 0, ERROR = 1, WARN = 2, INFO = 3, DEBUG = 4 };

// ANSI color codes
namespace Color {
constexpr const char *RESET = "\033[0m";
constexpr const char *RED = "\033[31m";
constexpr const char *YELLOW = "\033[33m";
constexpr const char *GREEN = "\033[32m";
constexpr const char *BLUE = "\033[34m";
constexpr const char *GRAY = "\033[90m";
} // namespace Color

constexpr Level getCurrentLevel() {
#ifdef LOGGING_LEVEL_NONE
  return Level::NONE;
#elif defined(LOGGING_LEVEL_ERROR)
  return Level::ERROR;
#elif defined(LOGGING_LEVEL_WARN)
  return Level::WARN;
#elif defined(LOGGING_LEVEL_INFO)
  return Level::INFO;
#elif defined(LOGGING_LEVEL_DEBUG)
  return Level::DEBUG;
#else
  return Level::INFO;
#endif
}

inline const char *getLevelString(Level level) {
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

inline const char *getLevelColor(Level level) {
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

inline std::string getTimestamp() {
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

inline void log(Level level, const std::string &message) {
  if (static_cast<int>(level) <= static_cast<int>(getCurrentLevel())) {
    std::cerr << Color::GRAY << "[" << getTimestamp() << "] "
              << getLevelColor(level) << "[" << getLevelString(level) << "] "
              << Color::RESET << message << std::endl;
  }
}

inline void error(const std::string &message) { log(Level::ERROR, message); }

inline void warn(const std::string &message) { log(Level::WARN, message); }

inline void info(const std::string &message) { log(Level::INFO, message); }

inline void debug(const std::string &message) { log(Level::DEBUG, message); }

// Template specialization for string literals
template <size_t N> inline void error(const char (&message)[N]) {
  log(Level::ERROR, std::string(message));
}

template <size_t N> inline void warn(const char (&message)[N]) {
  log(Level::WARN, std::string(message));
}

template <size_t N> inline void info(const char (&message)[N]) {
  log(Level::INFO, std::string(message));
}

template <size_t N> inline void debug(const char (&message)[N]) {
  log(Level::DEBUG, std::string(message));
}

template <typename T> inline void error(const T &message) {
  log(Level::ERROR, std::to_string(message));
}

template <typename T> inline void warn(const T &message) {
  log(Level::WARN, std::to_string(message));
}

template <typename T> inline void info(const T &message) {
  log(Level::INFO, std::to_string(message));
}

template <typename T> inline void debug(const T &message) {
  log(Level::DEBUG, std::to_string(message));
}

} // namespace Logger

#define LOG_ERROR(msg) Logger::error(msg)
#define LOG_WARN(msg) Logger::warn(msg)
#define LOG_INFO(msg) Logger::info(msg)
#define LOG_DEBUG(msg) Logger::debug(msg)

#endif // LOGGER_HPP