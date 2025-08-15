cmake_minimum_required(VERSION 3.16)
project(j2_logger_demo CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# spdlog(필수), Threads
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Boost.Asio 사용(UDP 알림)
find_package(Boost REQUIRED COMPONENTS system)
if (NOT Boost_FOUND)
  message(FATAL_ERROR "Boost (system) not found")
endif()

# ConvertUTF.c는 C 소스입니다. 정적 라이브러리로 빌드
add_library(convertutf STATIC third_party/ConvertUTF.c)
set_source_files_properties(third_party/ConvertUTF.c PROPERTIES LANGUAGE C)
target_include_directories(convertutf PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

# SimpleIni 설정:
#  - SI_CONVERT_GENERIC  : ConvertUTF.c/h 기반 generic 변환 사용
#  - SI_SUPPORT_IOSTREAMS: iostream 지원
target_compile_definitions(convertutf PUBLIC SI_CONVERT_GENERIC SI_SUPPORT_IOSTREAMS)

# 실행 파일
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/LoggerManager.cpp
)

# include 경로
target_include_directories(${PROJECT_NAME} PRIVATE
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

# spdlog 컴파일 레벨(선택) - TRACE까지 활성화
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

