cmake_minimum_required(VERSION 3.21)
project(nox_vault_client VERSION 0.2.8 LANGUAGES CXX)

set(NOX_PRODUCT_NAME "NOX Vault")
set(NOX_VENDOR "NOX Vault Team")
set(NOX_HOMEPAGE "https://github.com/Progerleonid/NOX-Vault")
set(NOX_PACKAGING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packaging")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    # Release dependencies use the x64-windows-static vcpkg triplet. Match its
    # /MT runtime so the executable is self-contained and ABI-compatible.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set(NOX_DEFAULT_SERVER_URL "https://api.noxvault.tech" CACHE STRING "Official NOX server URL")
option(NOX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

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
    if(NOX_ENABLE_SANITIZERS)
        target_compile_options(nox_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(nox_core PUBLIC -fsanitize=address,undefined)
    endif()
endif()

add_executable(nox src/main.cpp)
target_link_libraries(nox PRIVATE nox_core CLI11::CLI11)
if(WIN32)
    install(TARGETS nox RUNTIME DESTINATION bin)
    install(FILES $<TARGET_RUNTIME_DLLS:nox> DESTINATION bin)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET AND
       EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/"
                DESTINATION bin FILES_MATCHING PATTERN "*.dll")
    endif()
elseif(APPLE)
    install(TARGETS nox RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
            DESTINATION share/nox-vault)
    install(PROGRAMS "${NOX_PACKAGING_DIR}/macos/uninstall-nox-vault"
            DESTINATION share/nox-vault)
else()
    include(GNUInstallDirs)
    install(TARGETS nox RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/nox-vault")
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(nox_tests tests/test_client.cpp)
    target_link_libraries(nox_tests PRIVATE nox_core Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(nox_tests)
endif()

# Linux packages use the normal CMake install graph. Windows and macOS consume
# the same graph from their native WiX/productbuild scripts.
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "nox-vault")
    set(CPACK_PACKAGE_VENDOR "${NOX_VENDOR}")
    set(CPACK_PACKAGE_CONTACT "NOX Vault Team <Progerleonid@users.noreply.github.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Locally encrypted NOX Vault command-line client")
    set(CPACK_PACKAGE_HOMEPAGE_URL "${NOX_HOMEPAGE}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_CHECKSUM "SHA256")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    include(CPack)
endif()
