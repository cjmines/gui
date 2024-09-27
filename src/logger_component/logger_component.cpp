#include "logger_component.hpp"

LoggerComponent::LoggerComponent(const std::string &logger_name, std::shared_ptr<spdlog::sink_ptr> shared_sink) {
    logger = std::make_shared<spdlog::logger>(logger_name, *shared_sink);
    logger->set_level(spdlog::level::debug);
    spdlog::register_logger(logger);
    logging_enabled = true;
}

LoggerComponent::LoggerComponent() {
    logger = nullptr;
    logging_enabled = false;
}

void LoggerComponent::set_logging(bool enable, const std::string &logger_name,
                                  std::shared_ptr<spdlog::sink_ptr> shared_sink) {
    if (enable && !logger && shared_sink) {
        logger = std::make_shared<spdlog::logger>(logger_name, *shared_sink);
        logger->set_level(spdlog::level::debug);
        spdlog::register_logger(logger);
    }
    logging_enabled = enable;
}

std::shared_ptr<spdlog::logger> LoggerComponent::get_logger() const { return logger; }
