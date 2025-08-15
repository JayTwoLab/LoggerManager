#pragma once

// 빌드 시스템에서 안 내려오면 TRACE로 올린다(매크로 컴파일 아웃 방지)
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

// hello_logger 전용 초단축 로깅 매크로
#ifndef hname
#define hname "hello_logger"
#endif

#define ht(...) SPDLOG_LOGGER_TRACE   (spdlog::get(hname), __VA_ARGS__)  // trace
#define hd(...) SPDLOG_LOGGER_DEBUG   (spdlog::get(hname), __VA_ARGS__)  // debug
#define hi(...) SPDLOG_LOGGER_INFO    (spdlog::get(hname), __VA_ARGS__)  // info
#define hw(...) SPDLOG_LOGGER_WARN    (spdlog::get(hname), __VA_ARGS__)  // warn
#define he(...) SPDLOG_LOGGER_ERROR   (spdlog::get(hname), __VA_ARGS__)  // error
#define hc(...) SPDLOG_LOGGER_CRITICAL(spdlog::get(hname), __VA_ARGS__)  // critical

