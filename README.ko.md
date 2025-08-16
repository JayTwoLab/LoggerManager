# j2 LoggerManager 한국어

[English](README.md)

<br />

---

## 개요

- **LoggerManager**는 **spdlog** 기반의 경량 `C++` 로깅 래퍼입니다.
- `INI` 파일 하나(`j2_logger_manager_config.ini`)로 설정하고,
- 실행 중에도 즉시 반영되는 **soft-reload**와 sink 재생성이 필요한 **hard-reload**를 지원합니다.
- 또한 디스크 잔여 용량에 따라 파일 로깅을 자동 중단/복귀하고,
- 중단 중에는 **Boost.Asio UDP 알림**을 보냅니다.
- 기본 로거 이름(`hello_logger`)용 **초단축 매크로**도 제공합니다.

<br />

---

## 특징

- **soft-reload**(재시작 없이 즉시 반영):  
  레벨, 패턴, 시간 모드(UTC/Local), `flush_on`, `FLUSH_EVERY_SEC`
- **hard-reload**(sink 재생성):  
  on/off, 파일 경로, 회전 용량/백업 개수
- **이중 파일 로깅**: 전체 로그용(all) + 경고 이상(alerts)용 회전 파일
- **콘솔 미러링**: all.log 내용 콘솔에도 표시(패턴/레벨 별도 설정 가능)
- **디스크 감시**: 특정 디스크의 잔여 비율이 임계값 미만이면 파일 싱크 분리 → 콘솔만 출력
- **UDP 알림(Boost.Asio)**: 파일 로깅 중단 동안 일정 간격으로 알림 전송
- **짧은 매크로**: `ht/hd/hi/hw/he/hc`

<br />

---

## 폴더 구성

```
.
├─ CMakeLists.txt
├─ j2_logger_manager_config.ini
├─ include/
│  └─ j2/
│     ├─ LoggerManager.hpp
│     └─ macro.hpp
├─ src/
│  ├─ LoggerManager.cpp
│  └─ main.cpp
└─ third_party/
   ├─ SimpleIni.h
   ├─ ConvertUTF.h
   └─ ConvertUTF.c
```

> `SimpleIni.h`는 유니코드 변환을 위해 `SI_CONVERT_GENERIC` 구성 시 **ConvertUTF.c/h**를 사용합니다.

---

## 요구 사항

- CMake ≥ 3.16
- C++17 컴파일러
- **spdlog** (CMake 패키지: `spdlog::spdlog`)
- **Boost.System** (Boost.Asio UDP)
- Threads
- Windows: `ws2_32` 링크

---

## 빌드

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 주요 CMake 구성(이미 포함됨)

```cmake
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

add_library(convertutf STATIC third_party/ConvertUTF.c)
set_source_files_properties(third_party/ConvertUTF.c PROPERTIES LANGUAGE C)
target_include_directories(convertutf PUBLIC ${CMAKE_SOURCE_DIR}/third_party)
target_compile_definitions(convertutf PUBLIC SI_CONVERT_GENERIC SI_SUPPORT_IOSTREAMS)

add_executable(${PROJECT_NAME} src/main.cpp src/LoggerManager.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE include third_party ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog Threads::Threads Boost::system convertutf)
if (WIN32) target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32) endif()

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
```

---

## 설정

`j2_logger_manager_config.ini` (발췌 — 주석에 상세 설명 포함):

```ini
; =======================
; j2 / spdlog 설정 파일
; =======================
; 리로드 구분 안내
; - [soft-reload] : 재시작 없이 즉시 반영(레벨/패턴/시간/flush_on/주기적 플러시/디스크 감시 ON/OFF 등)
; - [hard-reload] : sink 재생성 필요(on/off, 경로, 회전 용량/백업 개수)
; - [init-only]   : 최초 초기화에서만 읽음(AUTO_RELOAD_SEC)
;
; 하드 리로드 주의
; - 파일 경로 변경 시 새 파일로 즉시 전환(기존 파일 보존), 권한/네트워크 경로 주의
; - 회전 용량/백업 개수 축소는 즉시 파일을 줄이지 않으며 다음 회전부터 적용
; - 매우 큰 용량(GB~TB)은 64비트 권장, 파일시스템 한계·ulimit에 유의

[Log]

; ===== [init-only] 최초 1회만 읽음 =====
AUTO_RELOAD_SEC=60

; ===== [hard-reload] sink 재생성 필요 =====
ENABLE_CONSOLE_LOG=true
ENABLE_FILE_LOG_ALL=true
ENABLE_FILE_LOG_ALERTS=true

ALL_PATH=logs/all.log
ALERTS_PATH=logs/alerts.log

; 회전 정책
; 예시> 일반 로그 최대 500MB = 100MB * 5개
ALL_MAX_SIZE=100MB
ALL_MAX_FILES=5
; 예시> 경고 로그 최대 약 1GB = 100MB * 10개
ALERT_MAX_SIZE=100MB
ALERT_MAX_FILES=10

; ===== [soft-reload] 즉시 반영 =====
TIME_MODE=local

; 최소 레벨(이상)
;   trace, debug, info, warn, error, critical, off

CONSOLE_LEVEL=trace
; CONSOLE_LEVEL=critical

ALL_FILE_LEVEL=trace
ALERTS_FILE_LEVEL=warn
LOGGER_LEVEL=trace
FLUSH_ON_LEVEL=warn

; 주기적 플러시(초)
FLUSH_EVERY_SEC=1

; 패턴
;   %Y %m %d %H %M %S %e = 시간
;   %l=레벨  %t=스레드ID  %v=메시지  %^/%$=컬러 on/off
PATTERN_CONSOLE=[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v
PATTERN_FILE=[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v

; ===== 디스크 감시(단일, soft-reload) =====
; 디스크 감시 ON/OFF
DISK_GUARD_ENABLE=false
; 감시할 디스크(또는 마운트/드라이브) 상의 경로. 예) /hello 또는 C:\hello
; DISK_ROOT=/hello
DISK_ROOT=C:\
; 잔여 비율(%)이 임계값 미만이면 파일 로깅 중지(콘솔만 출력)
DISK_MIN_FREE_RATIO=5
; 파일 로깅 중지 동안 UDP 알림 전송
UDP_ALERT_IP=127.0.0.1
UDP_ALERT_PORT=10514
UDP_ALERT_INTERVAL_SEC=60
; 플레이스홀더: {path}={DISK_ROOT}, {avail_bytes}=가용 바이트, {ratio}=잔여 비율(%)
UDP_ALERT_MESSAGE=DISK LOW: path={path} free={avail_bytes}B ({ratio}%)
```

<br />

---

## 사용법

```cpp
#include "j2/LoggerManager.hpp"
#define hname "hello_logger"
#include "j2/macro.hpp"

int main() {
    std::string defaultConfig = "j2_logger_manager_config.ini";
    j2::LoggerManager mgr;
    if (!mgr.init(defaultConfig, "Log", hname, "LOG_MANAGER_CONFIG_PATH")) return 1;

    while (true) {
        ht("trace message");
        hd("debug message");
        hi("info message");
        hw("warn message");
        he("error message");
        hc("critical message");
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}
```

환경 변수 `LOG_MANAGER_CONFIG_PATH`를 지정하면 INI 경로를 우선 사용합니다.

---

## soft-reload vs hard-reload

- **soft-reload**: 포맷터/레벨/플러시/시간 모드 등의 **즉시 반영**  
- **hard-reload**: on/off, 경로, 회전 정책 변경 시 **sink 재생성** 필요. 파일 경로 변경 시 새 파일로 전환

> 주의: 회전 용량/백업 개수를 줄이면 기존 파일은 즉시 줄어들지 않으며, 다음 회전부터 적용됩니다.

---

## IDE 팁 (Qt Creator)

프로젝트 트리에 INI를 보이게 하려면:
```cmake
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini)
set_source_files_properties(${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("Config" FILES ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini)
```

빌드 후 실행 파일 옆으로 복사(선택):
```cmake
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini
          $<TARGET_FILE_DIR:${PROJECT_NAME}>/j2_logger_manager_config.ini)
```

---

## 라이선스

- 프로젝트 코드: `LICENSE` 참조(`MIT License`)  
- `SimpleIni.h`: 상위 프로젝트 라이선스 참조  
- `ConvertUTF.c/h`: Unicode, Inc. 레퍼런스 구현 — 포함된 라이선스 참조
