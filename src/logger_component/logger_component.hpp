#ifndef LOGGER_COMPONENT_HPP
#define LOGGER_COMPONENT_HPP

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

class LoggerComponent {
  public:
    LoggerComponent(const std::string &logger_name, std::shared_ptr<spdlog::sink_ptr> shared_sink);
    LoggerComponent();
    void set_logging(bool enable, const std::string &logger_name = "",
                     std::shared_ptr<spdlog::sink_ptr> shared_sink = nullptr);

    std::shared_ptr<spdlog::logger> get_logger() const;
    bool logging_enabled; /// true implies logger != null

  private:
    std::shared_ptr<spdlog::logger> logger;
};

#endif // LOGGER_COMPONENT_HPP
