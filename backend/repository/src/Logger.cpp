#include "../include/Repository/Logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Repository {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    shutdown();
}

bool Logger::initialize(const std::string& logFilePath, LogLevel minLevel, bool enableConsole) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        if (m_initialized) {
            return true;  // Already initialized
        }

        m_logFilePath = logFilePath;
        m_minLevel = minLevel;
        m_enableConsole = enableConsole;

        // Open log file
        m_logFile.open(m_logFilePath, std::ios::app);
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file: " << m_logFilePath << std::endl;
            return false;
        }

        // Start the logging worker thread
        m_shutdown = false;
        m_logWorker = std::thread(&Logger::logWorker, this);

        m_initialized = true;
    }  // Release lock before calling log()

    // Log initialization
    log(LogLevel::INFO, "Logger", "Logger initialized", "LogFile: " + m_logFilePath);

    return true;
}

void Logger::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Signal shutdown
    m_shutdown = true;
    m_queueCondition.notify_all();

    // Wait for worker thread to finish
    if (m_logWorker.joinable()) {
        m_logWorker.join();
    }

    // Close log file
    if (m_logFile.is_open()) {
        m_logFile.close();
    }

    m_initialized = false;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message,
                 const std::string& details) {
    if (!m_initialized || level < m_minLevel) {
        return;
    }

    LogEntry entry(level, component, message, details);

    // Add to queue for async processing
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_logQueue.push(entry);
    }
    m_queueCondition.notify_one();

    // Also add to recent entries cache
    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        m_recentEntries.push_back(entry);
        if (m_recentEntries.size() > MAX_RECENT_ENTRIES) {
            m_recentEntries.erase(m_recentEntries.begin());
        }
    }
}

void Logger::debug(const std::string& component, const std::string& message,
                   const std::string& details) {
    log(LogLevel::DEBUG, component, message, details);
}

void Logger::info(const std::string& component, const std::string& message,
                  const std::string& details) {
    log(LogLevel::INFO, component, message, details);
}

void Logger::warning(const std::string& component, const std::string& message,
                     const std::string& details) {
    log(LogLevel::WARNING, component, message, details);
}

void Logger::error(const std::string& component, const std::string& message,
                   const std::string& details) {
    log(LogLevel::ERROR, component, message, details);
}

void Logger::critical(const std::string& component, const std::string& message,
                      const std::string& details) {
    log(LogLevel::CRITICAL, component, message, details);
}

void Logger::setMinLevel(LogLevel level) {
    m_minLevel = level;
}

std::vector<LogEntry> Logger::getRecentEntries(size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    if (maxEntries >= m_recentEntries.size()) {
        return m_recentEntries;
    }

    return std::vector<LogEntry>(m_recentEntries.end() - maxEntries, m_recentEntries.end());
}

void Logger::logWorker() {
    while (!m_shutdown) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCondition.wait(lock, [this] {
            return !m_logQueue.empty() || m_shutdown;
        });

        while (!m_logQueue.empty()) {
            LogEntry entry = m_logQueue.front();
            m_logQueue.pop();
            lock.unlock();

            // Format and write log entry
            std::string logLine = formatLogEntry(entry);

            // Write to file
            if (m_logFile.is_open()) {
                m_logFile << logLine << std::endl;
                m_logFile.flush();
            }

            // Write to console if enabled
            if (m_enableConsole) {
                if (entry.level >= LogLevel::ERROR) {
                    std::cerr << logLine << std::endl;
                } else {
                    std::cout << logLine << std::endl;
                }
            }

            lock.lock();
        }
    }
}

std::string Logger::formatLogEntry(const LogEntry& entry) const {
    std::ostringstream oss;

    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()) %
        1000;

    std::tm timeInfo = {};
#ifdef _WIN32
    localtime_s(&timeInfo, &time_t);
#else
    localtime_r(&time_t, &timeInfo);
#endif
    oss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    // Add log level
    oss << " [" << logLevelToString(entry.level) << "]";

    // Add component
    oss << " [" << entry.component << "]";

    // Add message
    oss << " " << entry.message;

    // Add details if present
    if (!entry.details.empty()) {
        oss << " | " << entry.details;
    }

    return oss.str();
}

std::string Logger::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRIT";
        default:
            return "UNKNOWN";
    }
}

// ScopedLogger implementation
ScopedLogger::ScopedLogger(const std::string& component, const std::string& operation)
    : m_component(component),
      m_operation(operation),
      m_startTime(std::chrono::steady_clock::now()),
      m_completed(false) {
    Logger::getInstance().debug(m_component, "Starting operation: " + m_operation);
}

ScopedLogger::~ScopedLogger() {
    if (!m_completed) {
        auto duration = std::chrono::steady_clock::now() - m_startTime;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        std::string details = "Duration: " + std::to_string(ms) + "ms";
        if (!m_context.empty()) {
            details += " | " + m_context;
        }

        Logger::getInstance().debug(m_component, "Completed operation: " + m_operation, details);
    }
}

void ScopedLogger::success(const std::string& details) {
    if (m_completed)
        return;

    auto duration = std::chrono::steady_clock::now() - m_startTime;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::string logDetails = "SUCCESS - Duration: " + std::to_string(ms) + "ms";
    if (!details.empty()) {
        logDetails += " | " + details;
    }
    if (!m_context.empty()) {
        logDetails += " | " + m_context;
    }

    Logger::getInstance().info(m_component, "Operation completed: " + m_operation, logDetails);
    m_completed = true;
}

void ScopedLogger::failure(const std::string& error, const std::string& details) {
    if (m_completed)
        return;

    auto duration = std::chrono::steady_clock::now() - m_startTime;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::string logDetails = "FAILED - Duration: " + std::to_string(ms) + "ms | Error: " + error;
    if (!details.empty()) {
        logDetails += " | " + details;
    }
    if (!m_context.empty()) {
        logDetails += " | " + m_context;
    }

    Logger::getInstance().error(m_component, "Operation failed: " + m_operation, logDetails);
    m_completed = true;
}

void ScopedLogger::addContext(const std::string& key, const std::string& value) {
    if (!m_context.empty()) {
        m_context += ", ";
    }
    m_context += key + "=" + value;
}

}  // namespace Repository