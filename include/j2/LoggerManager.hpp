#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <optional>
#include <sstream>
#include <iomanip>
#include <boost/asio.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include "SimpleIni.h"

// LoggerManager (namespace j2)
// - INI 기반 spdlog 구성
// - soft-reload: 레벨/패턴/UTC/flush_on/주기적 플러시 즉시 반영
// - hard-reload: on/off, 파일 경로, 회전 정책 변경 시 해당 싱크 재생성
// - AUTO_RELOAD_SEC: init-only
// - 디스크 감시: DISK_ROOT 기준 잔여 비율이 임계값 미만이면 파일 로깅 중지 + UDP 알림 전송(Boost.Asio)

namespace j2 {

class LoggerManager {
public:
    LoggerManager();
    ~LoggerManager();

    bool init(const std::string& defaultConfigPath,
              const std::string& sectionName,
              const std::string& loggerName,
              const std::string& envName = "");

    std::shared_ptr<spdlog::logger> getLogger() const;

    bool reloadIfChanged();
    bool startAutoReload(unsigned interval_sec = 60);
    void stopAutoReload();

private:
    bool loadConfig(bool readAutoReload);
    void applySoftSettings();
    void applyHardSettingsIfNeeded(
        bool old_enableConsole, bool old_enableFileAll, bool old_enableFileAlerts,
        const std::string& old_allPath, const std::string& old_alertsPath,
        std::size_t old_allMaxSize, std::size_t old_allMaxFiles,
        std::size_t old_alertMaxSize, std::size_t old_alertMaxFiles);
    static void ensureParentDir(const std::string& path);
    bool toBool(const std::string& val, bool default_val) const;
    std::string toLower(const std::string& s) const;
    std::size_t parseSizeBytes(const std::string& s, std::size_t default_val) const;
    spdlog::level::level_enum parseLevel(const std::string& s,
                                         spdlog::level::level_enum def) const;

    // 디스크 감시 + UDP 알림
    void checkDiskAndAct();
    bool sendUdpAlert(const std::string& msg);
    std::string buildUdpMessage(const std::string& tmpl,
                                const std::string& path,
                                unsigned long long availBytes,
                                long double ratioPercent) const;
    static void replaceAll(std::string& s, const std::string& from, const std::string& to);

private:
    // INI/상태
    std::string iniPath_;
    std::string logSection_ = "Log";
    std::string loggerName_;
    CSimpleIniA ini_;

    bool utcMode_ = false;

    bool enableConsole_    = true;
    bool enableFileAll_    = true;
    bool enableFileAlerts_ = true;

    spdlog::level::level_enum consoleMin_ = spdlog::level::trace;
    spdlog::level::level_enum allFileMin_ = spdlog::level::trace;
    spdlog::level::level_enum alertsMin_  = spdlog::level::warn;
    spdlog::level::level_enum loggerMin_  = spdlog::level::trace;
    spdlog::level::level_enum flushOn_    = spdlog::level::warn;

    std::size_t flushEverySec_ = 1;

    std::string patternConsole_ = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v";
    std::string patternFile_    = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v";

    std::string allPath_    = "logs/all.log";
    std::string alertsPath_ = "logs/alerts.log";

    std::size_t allMaxSize_    = 100 * 1024 * 1024;
    std::size_t allMaxFiles_   = 5;
    std::size_t alertMaxSize_  = 100 * 1024 * 1024;
    std::size_t alertMaxFiles_ = 10;

    // 디스크 감시(단일)
    std::string diskRoot_;
    double      diskMinFreeRatio_ = 5.0; // %

    // UDP 알림(Boost.Asio)
    std::string udpIp_;
    unsigned    udpPort_ = 0;
    unsigned    udpIntervalSec_ = 60;
    std::string udpMessageTmpl_ = "DISK LOW: path={path} free={avail_bytes}B ({ratio}%)";
    std::chrono::steady_clock::time_point lastUdpSent_{};

    // 파일 싱크 분리 상태
    bool fileSinksDetachedForDisk_ = false;

    // 로거/싱크
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> consoleSink_;
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> allSink_;
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> alertsSink_;
    std::shared_ptr<spdlog::sinks::dist_sink_mt> distSink_;

    // 공통 상태
    std::filesystem::file_time_type lastWriteTime_{};
    std::atomic<bool> autoReloadRunning_{false};
    std::thread autoReloadThread_;
    unsigned autoReloadIntervalSec_{60};
    mutable std::mutex mu_;
};

} // namespace j2

