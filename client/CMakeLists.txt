cmake_minimum_required(VERSION 3.20)
project(nox_vault_client VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(NOX_DEFAULT_SERVER_URL "http://localhost:8000" CACHE STRING "Official NOX server URL")

find_package(CLI11 CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG QUIET)
if(TARGET unofficial-sodium::sodium)
    set(NOX_SODIUM_TARGET unofficial-sodium::sodium)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    set(NOX_SODIUM_TARGET PkgConfig::SODIUM)
endif()

add_library(nox_core
    src/api_client.cpp
    src/agent.cpp
    src/auth_manager.cpp
    src/backup_service.cpp
    src/clipboard.cpp
    src/config_manager.cpp
    src/crypto_service.cpp
    src/input.cpp
    src/models.cpp
    src/vault_service.cpp)
target_include_directories(nox_core PUBLIC include)
target_compile_definitions(nox_core PUBLIC
    NOX_DEFAULT_SERVER_URL="${NOX_DEFAULT_SERVER_URL}"
    NOX_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_definitions(nox_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(nox_core PUBLIC
    CURL::libcurl nlohmann_json::nlohmann_json ${NOX_SODIUM_TARGET})
if(WIN32)
    target_link_libraries(nox_core PUBLIC advapi32)
endif()

if(MSVC)
    target_compile_options(nox_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(nox_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(nox src/main.cpp)
target_link_libraries(nox PRIVATE nox_core CLI11::CLI11)
install(TARGETS nox RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(nox_tests tests/test_client.cpp)
    target_link_libraries(nox_tests PRIVATE nox_core Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(nox_tests)
endif()
