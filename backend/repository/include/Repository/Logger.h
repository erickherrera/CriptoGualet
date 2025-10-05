#pragma once

#include "RepositoryTypes.h"
#include <memory>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace Repository {

/**
 * @brief Thread-safe logger for repository operations
 */
class Logger {
public:
    /**
     * @brief Get the singleton logger instance
     */
    static Logger& getInstance();

    /**
     * @brief Initialize the logger with file path and minimum log level
     * @param logFilePath Path to the log file
     * @param minLevel Minimum log level to record
     * @param enableConsole Whether to also log to console
     * @return true if initialization successful
     */
    bool initialize(const std::string& logFilePath, LogLevel minLevel = LogLevel::INFO, bool enableConsole = false);

    /**
     * @brief Shutdown the logger and flush all pending logs
     */
    void shutdown();

    /**
     * @brief Log a message
     * @param level Log level
     * @param component Component name (e.g., "UserRepository")
     * @param message Main log message
     * @param details Optional additional details
     */
    void log(LogLevel level, const std::string& component, const std::string& message, const std::string& details = "");

    /**
     * @brief Convenience logging methods
     */
    void debug(const std::string& component, const std::string& message, const std::string& details = "");
    void info(const std::string& component, const std::string& message, const std::string& details = "");
    void warning(const std::string& component, const std::string& message, const std::string& details = "");
    void error(const std::string& component, const std::string& message, const std::string& details = "");
    void critical(const std::string& component, const std::string& message, const std::string& details = "");

    /**
     * @brief Set the minimum log level
     * @param level New minimum log level
     */
    void setMinLevel(LogLevel level);

    /**
     * @brief Get recent log entries
     * @param maxEntries Maximum number of entries to return
     * @return Vector of recent log entries
     */
    std::vector<LogEntry> getRecentEntries(size_t maxEntries = 100) const;

    /**
     * @brief Check if logger is initialized
     */
    bool isInitialized() const { return m_initialized; }

private:
    Logger() : m_initialized(false), m_minLevel(LogLevel::INFO), m_enableConsole(false), m_shutdown(false) {}
    ~Logger();

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief Background thread function for async logging
     */
    void logWorker();

    /**
     * @brief Format log entry to string
     * @param entry Log entry to format
     * @return Formatted log string
     */
    std::string formatLogEntry(const LogEntry& entry) const;

    /**
     * @brief Convert log level to string
     * @param level Log level to convert
     * @return String representation of log level
     */
    std::string logLevelToString(LogLevel level) const;

private:
    std::atomic<bool> m_initialized;
    std::atomic<LogLevel> m_minLevel;
    std::atomic<bool> m_enableConsole;
    std::atomic<bool> m_shutdown;

    std::string m_logFilePath;
    std::ofstream m_logFile;

    // Async logging
    std::queue<LogEntry> m_logQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::thread m_logWorker;

    // Recent entries cache (for GUI display)
    mutable std::mutex m_entriesMutex;
    std::vector<LogEntry> m_recentEntries;
    static constexpr size_t MAX_RECENT_ENTRIES = 1000;
};

/**
 * @brief RAII class for scoped logging of operations
 */
class ScopedLogger {
public:
    ScopedLogger(const std::string& component, const std::string& operation);
    ~ScopedLogger();

    /**
     * @brief Mark the operation as successful
     */
    void success(const std::string& details = "");

    /**
     * @brief Mark the operation as failed
     */
    void failure(const std::string& error, const std::string& details = "");

    /**
     * @brief Add additional context information
     */
    void addContext(const std::string& key, const std::string& value);

private:
    std::string m_component;
    std::string m_operation;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_completed;
    std::string m_context;
};

} // namespace Repository

// Convenience macros for logging
#define REPO_LOG_DEBUG(component, message, ...) \
    Repository::Logger::getInstance().debug(component, message, ##__VA_ARGS__)

#define REPO_LOG_INFO(component, message, ...) \
    Repository::Logger::getInstance().info(component, message, ##__VA_ARGS__)

#define REPO_LOG_WARNING(component, message, ...) \
    Repository::Logger::getInstance().warning(component, message, ##__VA_ARGS__)

#define REPO_LOG_ERROR(component, message, ...) \
    Repository::Logger::getInstance().error(component, message, ##__VA_ARGS__)

#define REPO_LOG_CRITICAL(component, message, ...) \
    Repository::Logger::getInstance().critical(component, message, ##__VA_ARGS__)

#define REPO_SCOPED_LOG(component, operation) \
    Repository::ScopedLogger _scopedLogger(component, operation)
