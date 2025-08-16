#include "j2/LoggerManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace j2 {

LoggerManager::LoggerManager() {
}

LoggerManager::~LoggerManager() {
    stopAutoReload();
}

// init()은 내부에서 락을 잡지만, 오토리로드 시작/중지는 락 해제 후 호출해 교착을 피한다.
bool LoggerManager::init(const std::string& defaultConfigPath,
                         const std::string& sectionName,
                         const std::string& loggerName,
                         const std::string& envName) {
    unsigned interval_to_start = 0;
    bool need_start = false;

    {
        std::lock_guard<std::mutex> lk(mu_);

        loggerName_ = loggerName;
        logSection_ = sectionName;

        if (!envName.empty()) {
            if (const char* envPath = std::getenv(envName.c_str())) {
                iniPath_ = envPath;
                std::cout << "[LoggerManager] Using config from " << envName << ": " << iniPath_ << "\n";
            } else {
                iniPath_ = defaultConfigPath;
                std::cout << "[LoggerManager] Env var " << envName << " not set, using default: " << iniPath_ << "\n";
            }
        } else {
            iniPath_ = defaultConfigPath;
            std::cout << "[LoggerManager] Using default config path: " << iniPath_ << "\n";
        }

        ini_.SetUnicode();
        ini_.SetMultiKey(false);

        if (!loadConfig(true)) {
            std::cerr << "[LoggerManager] Failed to load config.\n";
            return false;
        }

        try {
            lastWriteTime_ = std::filesystem::last_write_time(iniPath_);
        } catch (...) {
            lastWriteTime_ = std::filesystem::file_time_type{};
        }

        auto time_type = utcMode_ ? spdlog::pattern_time_type::utc
                                  : spdlog::pattern_time_type::local;
        auto console_fmt = std::make_unique<spdlog::pattern_formatter>(patternConsole_, time_type);
        auto file_fmt    = std::make_unique<spdlog::pattern_formatter>(patternFile_,    time_type);

        distSink_ = std::make_shared<spdlog::sinks::dist_sink_mt>();

        if (enableConsole_) {
            consoleSink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink_->set_level(consoleMin_);
            consoleSink_->set_formatter(console_fmt->clone());
            distSink_->add_sink(consoleSink_);
        }

        if (enableFileAll_) {
            ensureParentDir(allPath_);
            allSink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                allPath_, allMaxSize_, allMaxFiles_, false);
            allSink_->set_level(allFileMin_);
            allSink_->set_formatter(file_fmt->clone());
            distSink_->add_sink(allSink_);
        }

        if (enableFileAlerts_) {
            ensureParentDir(alertsPath_);
            alertsSink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                alertsPath_, alertMaxSize_, alertMaxFiles_, false);
            alertsSink_->set_level(alertsMin_);
            alertsSink_->set_formatter(file_fmt->clone());
            distSink_->add_sink(alertsSink_);
        }

        if (distSink_->sinks().empty()) {
            consoleSink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink_->set_level(spdlog::level::trace);
            consoleSink_->set_formatter(
                std::make_unique<spdlog::pattern_formatter>(patternConsole_, time_type));
            distSink_->add_sink(consoleSink_);
            std::cerr << "[LoggerManager] No sinks enabled, fallback to console.\n";
        }

        logger_ = std::make_shared<spdlog::logger>(loggerName_, distSink_);
        spdlog::register_logger(logger_);

        applySoftSettings();

        if (flushEverySec_ > 0) {
            spdlog::flush_every(std::chrono::seconds(flushEverySec_));
        }

        // 초기 1회 디스크 확인
        checkDiskAndAct();

        // 오토리로드 시작 여부와 주기를 보관해두고, 락 해제 후에 실제 호출한다.
        need_start = (autoReloadIntervalSec_ > 0);
        interval_to_start = autoReloadIntervalSec_;
    } // 여기서 mu_ 잠금 해제

    if (need_start) {
        startAutoReload(interval_to_start);
    } else {
        stopAutoReload();
    }

    return true;
}

std::shared_ptr<spdlog::logger> LoggerManager::getLogger() const {
    std::lock_guard<std::mutex> lk(mu_);
    return logger_;
}

void LoggerManager::applySoftSettings() {
    auto time_type = utcMode_ ? spdlog::pattern_time_type::utc
                              : spdlog::pattern_time_type::local;

    auto console_fmt = std::make_unique<spdlog::pattern_formatter>(patternConsole_, time_type);
    auto file_fmt    = std::make_unique<spdlog::pattern_formatter>(patternFile_,    time_type);

    if (consoleSink_) {
        consoleSink_->set_level(consoleMin_);
        consoleSink_->set_formatter(console_fmt->clone());
    }
    if (allSink_) {
        allSink_->set_level(allFileMin_);
        allSink_->set_formatter(file_fmt->clone());
    }
    if (alertsSink_) {
        alertsSink_->set_level(alertsMin_);
        alertsSink_->set_formatter(file_fmt->clone());
    }

    if (logger_) {
        logger_->set_level(loggerMin_);
        logger_->flush_on(flushOn_);
    }
}

void LoggerManager::applyHardSettingsIfNeeded(
    bool old_enableConsole, bool old_enableFileAll, bool old_enableFileAlerts,
    const std::string& old_allPath, const std::string& old_alertsPath,
    std::size_t old_allMaxSize, std::size_t old_allMaxFiles,
    std::size_t old_alertMaxSize, std::size_t old_alertMaxFiles) {

    auto time_type = utcMode_ ? spdlog::pattern_time_type::utc
                              : spdlog::pattern_time_type::local;
    auto console_fmt = std::make_unique<spdlog::pattern_formatter>(patternConsole_, time_type);
    auto file_fmt    = std::make_unique<spdlog::pattern_formatter>(patternFile_,    time_type);

    bool console_add   =  enableConsole_ && !consoleSink_;
    bool console_remove= !enableConsole_ &&  consoleSink_;
    if (console_add) {
        auto s = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        s->set_level(consoleMin_);
        s->set_formatter(console_fmt->clone());
        distSink_->add_sink(s);
        consoleSink_ = s;
    } else if (console_remove) {
        consoleSink_->flush();
        distSink_->remove_sink(consoleSink_);
        consoleSink_.reset();
    }

    bool need_new_all =
        (enableFileAll_ && !allSink_) ||
        (!old_enableFileAll && enableFileAll_) ||
        (allSink_ && (allPath_ != old_allPath ||
                      allMaxSize_ != old_allMaxSize ||
                      allMaxFiles_ != old_allMaxFiles));

    if (enableFileAll_) {
        if (need_new_all) {
            ensureParentDir(allPath_);
            auto new_all = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                allPath_, allMaxSize_, allMaxFiles_, false);
            new_all->set_level(allFileMin_);
            new_all->set_formatter(file_fmt->clone());
            distSink_->add_sink(new_all);
            if (allSink_) {
                allSink_->flush();
                distSink_->remove_sink(allSink_);
            }
            allSink_.swap(new_all);
        }
    } else {
        if (allSink_) {
            allSink_->flush();
            distSink_->remove_sink(allSink_);
            allSink_.reset();
        }
    }

    bool need_new_alerts =
        (enableFileAlerts_ && !alertsSink_) ||
        (!old_enableFileAlerts && enableFileAlerts_) ||
        (alertsSink_ && (alertsPath_ != old_alertsPath ||
                         alertMaxSize_ != old_alertMaxSize ||
                         alertMaxFiles_ != old_alertMaxFiles));

    if (enableFileAlerts_) {
        if (need_new_alerts) {
            ensureParentDir(alertsPath_);
            auto new_alerts = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                alertsPath_, alertMaxSize_, alertMaxFiles_, false);
            new_alerts->set_level(alertsMin_);
            new_alerts->set_formatter(file_fmt->clone());
            distSink_->add_sink(new_alerts);
            if (alertsSink_) {
                alertsSink_->flush();
                distSink_->remove_sink(alertsSink_);
            }
            alertsSink_.swap(new_alerts);
        }
    } else {
        if (alertsSink_) {
            alertsSink_->flush();
            distSink_->remove_sink(alertsSink_);
            alertsSink_.reset();
        }
    }

    if (distSink_->sinks().empty()) {
        auto fallback = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        fallback->set_level(spdlog::level::trace);
        fallback->set_formatter(
            std::make_unique<spdlog::pattern_formatter>(patternConsole_, time_type));
        distSink_->add_sink(fallback);
        consoleSink_ = fallback;
        if (logger_) {
            logger_->warn("No sinks enabled after hard-reload. Fallback to console sink.");
        }
    }
}

bool LoggerManager::reloadIfChanged() {
    std::lock_guard<std::mutex> lk(mu_);

    std::filesystem::file_time_type now;
    try {
        now = std::filesystem::last_write_time(iniPath_);
    } catch (...) {
        checkDiskAndAct();
        return false;
    }
    if (now == lastWriteTime_) {
        checkDiskAndAct();
        return false;
    }
    lastWriteTime_ = now;

    bool old_enableConsole    = enableConsole_;
    bool old_enableFileAll    = enableFileAll_;
    bool old_enableFileAlerts = enableFileAlerts_;
    std::string old_allPath   = allPath_;
    std::string old_alertsPath= alertsPath_;
    std::size_t old_allMaxSize= allMaxSize_;
    std::size_t old_allMaxFiles=allMaxFiles_;
    std::size_t old_alertMaxSize=alertMaxSize_;
    std::size_t old_alertMaxFiles=alertMaxFiles_;

    bool ok = loadConfig(false);
    if (!ok) {
        checkDiskAndAct();
        return false;
    }

    applyHardSettingsIfNeeded(
        old_enableConsole, old_enableFileAll, old_enableFileAlerts,
        old_allPath, old_alertsPath,
        old_allMaxSize, old_allMaxFiles, old_alertMaxSize, old_alertMaxFiles);

    applySoftSettings();

    if (flushEverySec_ > 0) {
        spdlog::flush_every(std::chrono::seconds(flushEverySec_));
    } else {
        if (logger_) {
            logger_->warn("FLUSH_EVERY_SEC=0 detected. Disabling periodic flush at runtime is limited. Restart recommended.");
        }
    }

    checkDiskAndAct();
    return true;
}

// 디스크 감시: INI로 ON/OFF 가능
void LoggerManager::checkDiskAndAct() {
    // 감시가 꺼져 있으면, 혹시 이전에 분리했던 파일 싱크를 복구하고 종료
    if (!diskGuardEnable_) {
        if (fileSinksDetachedForDisk_) {
            if (enableFileAll_    && allSink_)    distSink_->add_sink(allSink_);
            if (enableFileAlerts_ && alertsSink_) distSink_->add_sink(alertsSink_);
            applySoftSettings();
            fileSinksDetachedForDisk_ = false;
            if (logger_) logger_->info("Disk guard disabled by config. File logging resumed.");
        }
        return;
    }

    if (diskRoot_.empty()) return;

    std::filesystem::space_info info{};
    try {
        info = std::filesystem::space(std::filesystem::path(diskRoot_));
    } catch (...) {
        if (logger_) logger_->warn("DISK_ROOT='{}' space() failed. Skip this round.", diskRoot_);
        return;
    }

    unsigned long long avail = static_cast<unsigned long long>(info.available);
    unsigned long long cap   = static_cast<unsigned long long>(info.capacity);
    long double ratio = cap > 0 ? (static_cast<long double>(avail) * 100.0L / static_cast<long double>(cap)) : 100.0L;

    bool low = (ratio < static_cast<long double>(diskMinFreeRatio_));

    if (low) {
        if (!fileSinksDetachedForDisk_) {
            if (allSink_)    { allSink_->flush();    distSink_->remove_sink(allSink_); }
            if (alertsSink_) { alertsSink_->flush(); distSink_->remove_sink(alertsSink_); }
            fileSinksDetachedForDisk_ = true;
            if (logger_) logger_->warn("Low disk space on '{}': {:.2f}% free. File logging suspended, console only.", diskRoot_, static_cast<double>(ratio));
        }

        auto now = std::chrono::steady_clock::now();
        bool due = (lastUdpSent_.time_since_epoch().count() == 0) ||
                   (now - lastUdpSent_ >= std::chrono::seconds(udpIntervalSec_));
        if (due && !udpIp_.empty() && udpPort_ > 0) {
            std::string payload = buildUdpMessage(udpMessageTmpl_, diskRoot_, avail, ratio);
            if (sendUdpAlert(payload)) {
                lastUdpSent_ = now;
            }
        }
    } else {
        if (fileSinksDetachedForDisk_) {
            if (enableFileAll_    && allSink_)    distSink_->add_sink(allSink_);
            if (enableFileAlerts_ && alertsSink_) distSink_->add_sink(alertsSink_);
            applySoftSettings();
            fileSinksDetachedForDisk_ = false;
            if (logger_) logger_->info("Disk space recovered on '{}': {:.2f}% free. File logging resumed.", diskRoot_, static_cast<double>(ratio));
        }
    }
}

std::string LoggerManager::buildUdpMessage(const std::string& tmpl,
                                           const std::string& path,
                                           unsigned long long availBytes,
                                           long double ratioPercent) const {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2) << static_cast<double>(ratioPercent);
    std::string ratio2 = oss.str();

    std::string msg = tmpl;
    replaceAll(msg, "{path}", path);
    replaceAll(msg, "{avail_bytes}", std::to_string(static_cast<long long>(availBytes)));
    replaceAll(msg, "{ratio}", ratio2);
    return msg;
}

void LoggerManager::replaceAll(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool LoggerManager::sendUdpAlert(const std::string& msg) {
    try {
        boost::asio::io_context io;
        boost::asio::ip::udp::endpoint ep(boost::asio::ip::make_address(udpIp_), static_cast<unsigned short>(udpPort_));
        boost::asio::ip::udp::socket sock(io);
        sock.open(boost::asio::ip::udp::v4());
        sock.send_to(boost::asio::buffer(msg), ep);
        sock.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool LoggerManager::startAutoReload(unsigned interval_sec) {
    std::lock_guard<std::mutex> lk(mu_);
    if (autoReloadRunning_) {
        autoReloadIntervalSec_ = interval_sec ? interval_sec : 60;
        return true;
    }
    if (interval_sec == 0) interval_sec = 60;
    autoReloadIntervalSec_ = interval_sec;

    autoReloadRunning_ = true;
    autoReloadThread_ = std::thread([this]() {
        while (autoReloadRunning_) {
            try {
                this->reloadIfChanged();
            } catch (...) {
            }
            std::this_thread::sleep_for(std::chrono::seconds(this->autoReloadIntervalSec_));
        }
    });
    return true;
}

void LoggerManager::stopAutoReload() {
    if (!autoReloadRunning_) return;
    autoReloadRunning_ = false;
    if (autoReloadThread_.joinable()) {
        autoReloadThread_.join();
    }
}

bool LoggerManager::loadConfig(bool readAutoReload) {
    if (ini_.LoadFile(iniPath_.c_str()) < 0) {
        return false;
    }

    std::string time_mode = toLower(ini_.GetValue(logSection_.c_str(), "TIME_MODE", "local"));
    utcMode_ = (time_mode == "utc");

    enableConsole_    = toBool(ini_.GetValue(logSection_.c_str(), "ENABLE_CONSOLE_LOG",    "true"), true);
    enableFileAll_    = toBool(ini_.GetValue(logSection_.c_str(), "ENABLE_FILE_LOG_ALL",   "true"), true);
    enableFileAlerts_ = toBool(ini_.GetValue(logSection_.c_str(), "ENABLE_FILE_LOG_ALERTS","true"), true);

    consoleMin_ = parseLevel(ini_.GetValue(logSection_.c_str(), "CONSOLE_LEVEL",     "trace"), spdlog::level::trace);
    allFileMin_ = parseLevel(ini_.GetValue(logSection_.c_str(), "ALL_FILE_LEVEL",    "trace"), spdlog::level::trace);
    alertsMin_  = parseLevel(ini_.GetValue(logSection_.c_str(), "ALERTS_FILE_LEVEL", "warn"),  spdlog::level::warn);
    loggerMin_  = parseLevel(ini_.GetValue(logSection_.c_str(), "LOGGER_LEVEL",      "trace"), spdlog::level::trace);
    flushOn_    = parseLevel(ini_.GetValue(logSection_.c_str(), "FLUSH_ON_LEVEL",    "warn"),  spdlog::level::warn);

    flushEverySec_ = static_cast<std::size_t>(
        ini_.GetLongValue(logSection_.c_str(), "FLUSH_EVERY_SEC", 1));

    patternConsole_ = ini_.GetValue(logSection_.c_str(), "PATTERN_CONSOLE",
                                    "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    patternFile_    = ini_.GetValue(logSection_.c_str(), "PATTERN_FILE",
                                 "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

    allPath_    = ini_.GetValue(logSection_.c_str(), "ALL_PATH",    "logs/all.log");
    alertsPath_ = ini_.GetValue(logSection_.c_str(), "ALERTS_PATH", "logs/alerts.log");

    allMaxSize_   = parseSizeBytes(ini_.GetValue(logSection_.c_str(), "ALL_MAX_SIZE",   "104857600"),
                                 100ull * 1024ull * 1024ull);
    allMaxFiles_  = static_cast<std::size_t>(ini_.GetLongValue(logSection_.c_str(), "ALL_MAX_FILES",  5));
    alertMaxSize_ = parseSizeBytes(ini_.GetValue(logSection_.c_str(), "ALERT_MAX_SIZE", "104857600"),
                                   100ull * 1024ull * 1024ull);
    alertMaxFiles_= static_cast<std::size_t>(ini_.GetLongValue(logSection_.c_str(), "ALERT_MAX_FILES",10));

    // 디스크 감시 ON/OFF 및 파라미터
    diskGuardEnable_ = toBool(ini_.GetValue(logSection_.c_str(), "DISK_GUARD_ENABLE", "true"), true);
    diskRoot_         = ini_.GetValue(logSection_.c_str(), "DISK_ROOT", "");
    diskMinFreeRatio_ = ini_.GetDoubleValue(logSection_.c_str(), "DISK_MIN_FREE_RATIO", 5.0);

    // UDP 알림(Boost.Asio)
    udpIp_            = ini_.GetValue(logSection_.c_str(), "UDP_ALERT_IP", "");
    udpPort_          = static_cast<unsigned>(ini_.GetLongValue(logSection_.c_str(), "UDP_ALERT_PORT", 0));
    udpIntervalSec_   = static_cast<unsigned>(ini_.GetLongValue(logSection_.c_str(), "UDP_ALERT_INTERVAL_SEC", 60));
    udpMessageTmpl_   = ini_.GetValue(logSection_.c_str(), "UDP_ALERT_MESSAGE",
                                    "DISK LOW: path={path} free={avail_bytes}B ({ratio}%)");

    if (readAutoReload) {
        autoReloadIntervalSec_ = static_cast<unsigned>(
            ini_.GetLongValue(logSection_.c_str(), "AUTO_RELOAD_SEC", 60));
    }

    return true;
}

void LoggerManager::ensureParentDir(const std::string& path) {
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    } catch (...) {
    }
}

bool LoggerManager::toBool(const std::string& val, bool default_val) const {
    std::string v = toLower(val);
    if (v == "true" || v == "1" || v == "yes" || v == "on")  return true;
    if (v == "false"|| v == "0" || v == "no"  || v == "off") return false;
    return default_val;
}

std::string LoggerManager::toLower(const std::string& s) const {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return res;
}

std::size_t LoggerManager::parseSizeBytes(const std::string& s, std::size_t default_val) const {
    if (s.empty()) return default_val;
    std::string v = s;
    v.erase(std::remove_if(v.begin(), v.end(), [](unsigned char c){ return std::isspace(c); }), v.end());
    std::string lower = toLower(v);

    std::size_t i = 0;
    while (i < lower.size() && (std::isdigit(static_cast<unsigned char>(lower[i])) || lower[i] == '.')) ++i;
    if (i == 0) return default_val;

    std::string num_str  = lower.substr(0, i);
    std::string unit_str = lower.substr(i);

    double num = 0.0;
    try { num = std::stod(num_str); } catch (...) { return default_val; }

    long double mul = 1.0L;
    if (unit_str.empty() || unit_str == "b") mul = 1.0L;
    else if (unit_str == "k" || unit_str == "kb") mul = 1024.0L;
    else if (unit_str == "m" || unit_str == "mb") mul = 1024.0L * 1024.0L;
    else if (unit_str == "g" || unit_str == "gb") mul = 1024.0L * 1024.0L * 1024.0L;
    else if (unit_str == "t" || unit_str == "tb") mul = 1024.0L * 1024.0L * 1024.0L * 1024.0L;
    else return default_val;

    unsigned long long bytes = static_cast<unsigned long long>(std::llround(num * mul));
    return static_cast<std::size_t>(bytes);
}

spdlog::level::level_enum LoggerManager::parseLevel(
    const std::string& s, spdlog::level::level_enum def) const {
    std::string v = toLower(s);
    if (v == "trace")                  return spdlog::level::trace;
    if (v == "debug")                  return spdlog::level::debug;
    if (v == "info")                   return spdlog::level::info;
    if (v == "warn" || v == "warning") return spdlog::level::warn;
    if (v == "error" || v == "err")    return spdlog::level::err;
    if (v == "critical" || v == "crit")return spdlog::level::critical;
    if (v == "off")                    return spdlog::level::off;
    return def;
}

} // namespace j2
