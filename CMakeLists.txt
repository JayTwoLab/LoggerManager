cmake_minimum_required(VERSION 3.28)

project(j2_logger_demo LANGUAGES C CXX)

# C++ 표준 설정
# set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake 정책
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# --- Windows + MinGW 환경 전용 설정 ---
if (WIN32 AND MINGW)
  message(STATUS "Detected Windows (MinGW)")

  set(VCPKG_ROOT $ENV{USERPROFILE})

  # vcpkg toolchain 설정 (MingW64 static)
  # ⚠ 이 위치에서 설정하면 project() 이후라서 CMake 초기 툴체인엔 적용 안 됨
  #    => Qt Creator CMake 추가 인자로 -DCMAKE_TOOLCHAIN_FILE=... 을 넘겨주세요.
  set(CMAKE_PREFIX_PATH    "${VCPKG_ROOT}/vcpkg/installed/x64-mingw-static")
  set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/vcpkg/scripts/buildsystems/vcpkg.cmake")

  # 선택: 정적 트리플릿 고정
  # set(VCPKG_TARGET_TRIPLET "x64-mingw-static")

  # 응급처치(직접 지정) - 필요 시 주석 해제:
  # set(spdlog_DIR "${VCPKG_ROOT}/vcpkg/installed/x64-mingw-static/share/spdlog")
endif()

# spdlog
find_package(spdlog CONFIG REQUIRED)

# Threads
find_package(Threads REQUIRED)

# Boost(system)
find_package(Boost CONFIG REQUIRED COMPONENTS system)

# ConvertUTF.c 빌드
add_library(convertutf STATIC third_party/ConvertUTF.c)
set_source_files_properties(third_party/ConvertUTF.c PROPERTIES LANGUAGE C)
target_include_directories(convertutf PUBLIC ${CMAKE_SOURCE_DIR}/third_party)
target_compile_definitions(convertutf PUBLIC SI_CONVERT_GENERIC SI_SUPPORT_IOSTREAMS)

# 실행 파일
add_executable(${PROJECT_NAME}
    # LoggerManager
    include/j2/LoggerManager.hpp
    src/LoggerManager.cpp
    # 사용자가 작성할 파일
    src/main.cpp
    src/macro.hpp
)

add_custom_target(config_files
    SOURCES ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini
)
source_group("Config" FILES
  ${CMAKE_SOURCE_DIR}/j2_logger_manager_config.ini
)

# include 경로
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${Boost_INCLUDE_DIRS}
)

# 링크
target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
    Threads::Threads
    Boost::system
    convertutf
)

# Windows UDP 소켓 심볼
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# spdlog 로그 레벨
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
