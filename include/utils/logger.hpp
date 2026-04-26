#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace logger {

enum class Level { NONE = 0, ERROR = 1, WARN = 2, INFO = 3, DEBUG = 4 };

// ANSI color codes
namespace color {
constexpr const char *RESET = "\033[0m";
constexpr const char *RED = "\033[31m";
constexpr const char *YELLOW = "\033[33m";
constexpr const char *GREEN = "\033[32m";
constexpr const char *BLUE = "\033[34m";
constexpr const char *GRAY = "\033[90m";
} // namespace color

constexpr Level get_current_level() {
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

const char *get_level_string(Level level);

const char *get_level_color(Level level);

std::string get_timestamp();

void log(Level level, std::string_view message);

void error(std::string_view message);

void warn(std::string_view message);

void info(std::string_view message);

void debug(std::string_view message);

template <typename T>
  requires std::is_arithmetic_v<std::remove_reference_t<T>>
inline void error(T message) {
  log(Level::ERROR, std::to_string(message));
}

template <typename T>
  requires std::is_arithmetic_v<std::remove_reference_t<T>>
inline void warn(T message) {
  log(Level::WARN, std::to_string(message));
}

template <typename T>
  requires std::is_arithmetic_v<std::remove_reference_t<T>>
inline void info(T message) {
  log(Level::INFO, std::to_string(message));
}

template <typename T>
  requires std::is_arithmetic_v<std::remove_reference_t<T>>
inline void debug(T message) {
  log(Level::DEBUG, std::to_string(message));
}

} // namespace logger

#define LOG_ERROR(msg) logger::error(msg)
#define LOG_WARN(msg) logger::warn(msg)
#define LOG_INFO(msg) logger::info(msg)
#define LOG_DEBUG(msg) logger::debug(msg)

#endif // LOGGER_HPP
