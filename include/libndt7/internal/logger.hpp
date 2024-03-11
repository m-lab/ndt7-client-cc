// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENTLAB_LIBNDT7_INTERNAL_LOGGER_HPP
#define MEASUREMENTLAB_LIBNDT7_INTERNAL_LOGGER_HPP

// libndt7/internal/logger.hpp - logger API

#include <sstream>
#include <string>

namespace measurementlab {
namespace libndt7 {
namespace internal {

class Logger {
 public:
  virtual bool is_warning_enabled() const noexcept = 0;
  virtual bool is_info_enabled() const noexcept = 0;
  virtual bool is_debug_enabled() const noexcept = 0;
  virtual void emit_warning(const std::string &) const noexcept = 0;
  virtual void emit_info(const std::string &) const noexcept = 0;
  virtual void emit_debug(const std::string &) const noexcept = 0;
  virtual ~Logger() noexcept;
};

class NoLogger : public Logger {
 public:
  bool is_warning_enabled() const noexcept override;
  bool is_info_enabled() const noexcept override;
  bool is_debug_enabled() const noexcept override;
  void emit_warning(const std::string &) const noexcept override;
  void emit_info(const std::string &) const noexcept override;
  void emit_debug(const std::string &) const noexcept override;
  ~NoLogger() noexcept override;
};

#define LIBNDT7_LOGGER_LEVEL_(logger, level, statements) \
  if ((logger).is_##level##_enabled()) {                 \
    std::stringstream ss;                                \
    ss << statements;                                    \
    logger.emit_##level(ss.str());                       \
  }

#define LIBNDT7_LOGGER_WARNING(logger, statements) \
  LIBNDT7_LOGGER_LEVEL_(logger, warning, statements)

#define LIBNDT7_LOGGER_INFO(logger, statements) \
  LIBNDT7_LOGGER_LEVEL_(logger, info, statements)

#define LIBNDT7_LOGGER_DEBUG(logger, statements) \
  LIBNDT7_LOGGER_LEVEL_(logger, debug, statements)

Logger::~Logger() noexcept {}

bool NoLogger::is_warning_enabled() const noexcept { return false; }

bool NoLogger::is_info_enabled() const noexcept { return false; }

bool NoLogger::is_debug_enabled() const noexcept { return false; }

void NoLogger::emit_warning(const std::string &) const noexcept {}

void NoLogger::emit_info(const std::string &) const noexcept {}

void NoLogger::emit_debug(const std::string &) const noexcept {}

NoLogger::~NoLogger() noexcept {}

}  // namespace internal
}  // namespace libndt7
}  // namespace measurementlab
#endif  // measurementlab_LIBNDT7_INTERNAL_LOGGER_HPP
