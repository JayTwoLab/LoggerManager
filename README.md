# j2 LoggerManager

[Korean](README.ko.md)

---

## Overview

**LoggerManager** is a tiny C++ wrapper around **spdlog** that gives you:

- **Single place** to configure logging via an INI file (`j2_logger_manager_config.ini`)
- **Soft-reload** (apply at runtime without restart): levels, patterns, time mode (UTC/Local), `flush_on`, periodic flush interval
- **Hard-reload** (recreate sinks): enable/disable sinks, file paths, rotation sizes & backups
- **Dual file routing**: one rotating file for **all** logs, another rotating file for **alerts** (`warn` and above)
- **Console mirroring**: everything that goes to `all.log` also shows in the console (configurable)
- **Disk guard**: if free space on a specified disk falls below a threshold, file sinks are **detached** and logging continues to **console only**
- **UDP alerts**: while file logging is suspended, a **Boost.Asio** UDP message is sent to a configured IP/port at a fixed interval
- **Ultra-short macros** for a specific logger name (default: `hello_logger`): `ht/hd/hi/hw/he/hc`

The design is “drop-in”: add the files, point to the INI, and log.

---

## Features

- **Soft-reload** (no restart):  
  Levels (`LOGGER_LEVEL`, `CONSOLE_LEVEL`, `ALL_FILE_LEVEL`, `ALERTS_FILE_LEVEL`), `FLUSH_ON_LEVEL`, `FLUSH_EVERY_SEC`, `TIME_MODE` (`local|utc`), patterns (`PATTERN_*`).
- **Hard-reload** (sink re-creation):  
  `ENABLE_*`, `ALL_PATH`, `ALERTS_PATH`, `ALL_MAX_SIZE`, `ALL_MAX_FILES`, `ALERT_MAX_SIZE`, `ALERT_MAX_FILES`.
- **Rotating files**: capacity-bounded with backup counts.
- **Disk monitoring** (single disk root): when `DISK_MIN_FREE_RATIO` is exceeded (i.e., free < threshold), detach file sinks and send UDP alerts every `UDP_ALERT_INTERVAL_SEC`.
- **Boost.Asio UDP**: format message with placeholders `{path}`, `{avail_bytes}`, `{ratio}`.
- **Macros**: tiny logging macros targeting one named logger.

---

## Folder Layout

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

> `SimpleIni.h` uses **ConvertUTF.c/h** when built with `SI_CONVERT_GENERIC` for Unicode conversion.

---

## Requirements

- CMake ≥ 3.16
- C++17 compiler
- **spdlog** (package provides `spdlog::spdlog` target)
- **Boost.System** (for Boost.Asio UDP)
- Threads
- Windows only: link `ws2_32`

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### CMake key parts (already included)

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

## Configure

`j2_logger_manager_config.ini` (excerpt — see comments inline):

```ini
[Log]

; ===== [init-only] read once at startup =====
AUTO_RELOAD_SEC=60

; ===== [hard-reload] requires sink recreation =====
ENABLE_CONSOLE_LOG=true
ENABLE_FILE_LOG_ALL=true
ENABLE_FILE_LOG_ALERTS=true
ALL_PATH=logs/all.log
ALERTS_PATH=logs/alerts.log
ALL_MAX_SIZE=100MB
ALL_MAX_FILES=5
ALERT_MAX_SIZE=100MB
ALERT_MAX_FILES=10

; ===== [soft-reload] applied immediately =====
TIME_MODE=local                 ; local | utc

; Levels (minimum):
;   trace, debug, info, warn, error, critical, off
CONSOLE_LEVEL=trace
ALL_FILE_LEVEL=trace
ALERTS_FILE_LEVEL=warn
LOGGER_LEVEL=trace
FLUSH_ON_LEVEL=warn

FLUSH_EVERY_SEC=1               ; periodic flush

; Patterns (placeholders):
;   %Y %m %d %H %M %S %e = time parts, %l=level, %t=thread id, %v=message, %^ %$=color on/off
PATTERN_CONSOLE=[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v
PATTERN_FILE=[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v

; Disk guard (single disk root) — soft-reload
DISK_ROOT=/hello
DISK_MIN_FREE_RATIO=5

; UDP alert (Boost.Asio). Placeholders: {path}, {avail_bytes}, {ratio}
UDP_ALERT_IP=127.0.0.1
UDP_ALERT_PORT=10514
UDP_ALERT_INTERVAL_SEC=60
UDP_ALERT_MESSAGE=DISK LOW: path={path} free={avail_bytes}B ({ratio}%)
```

---

## Usage

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

Environment variable `LOG_MANAGER_CONFIG_PATH` (if set) overrides the config path.

---

## Soft-reload vs Hard-reload

- **Soft-reload**: in-place update of formatter/levels/flush/time mode.
- **Hard-reload**: sinks are recreated when on/off, paths, or rotation policy changes; files may switch immediately to new locations/names.

> Caution: shrinking rotation limits does not shrink existing files; it applies on next rotation.

---

## Disk Guard & UDP

- When free space on `DISK_ROOT` < `DISK_MIN_FREE_RATIO` (%), file sinks are **detached** → console-only logging.  
- Every `UDP_ALERT_INTERVAL_SEC`, a UDP datagram is sent to `UDP_ALERT_IP:UDP_ALERT_PORT` using Boost.Asio with the formatted `UDP_ALERT_MESSAGE`.  
- When space recovers, file sinks are re-attached automatically.

---

## IDE tip (Qt Creator)

To make the INI visible in the project tree:
```cmake
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini)
set_source_files_properties(${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("Config" FILES ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini)
```

Optionally copy next to the binary after build:
```cmake
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini
          $<TARGET_FILE_DIR:${PROJECT_NAME}>/j2_logger_manager_config.ini)
```

---

## License

- Project code: see `LICENSE` (insert your license here).  
- `SimpleIni.h`: see upstream license.  
- `ConvertUTF.c/h`: Unicode, Inc. reference implementation — see included license.
