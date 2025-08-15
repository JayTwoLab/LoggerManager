#include "j2/LoggerManager.hpp"

#ifndef hname
#define hname "hello_logger"
#endif
#include "macro.hpp"

#include <thread>
#include <chrono>

int main() {
    // 기본 설정 파일명을 j2_logger_manager_config.ini 로 변경
    std::string defaultConfig = "j2_logger_manager_config.ini";
    std::string sectionName   = "Log";
    std::string loggerName    = hname;
    std::string envName       = "LOG_MANAGER_CONFIG_PATH"; // 필요 없으면 "" 사용

    j2::LoggerManager logMgr;
    if (!logMgr.init(defaultConfig, sectionName, loggerName, envName)) {
        return 1;
    }

    while (true) {
        ht("trace message");
        hd("debug message");
        hi("info message");
        hw("warn message");
        he("error message");
        hc("critical message");
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}

