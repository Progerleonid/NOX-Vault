cmake_minimum_required(VERSION 3.20)
project(nox_vault_client VERSION 0.2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
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
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/"
                DESTINATION bin FILES_MATCHING PATTERN "*.dll")
    endif()
else()
    install(TARGETS nox RUNTIME DESTINATION bin)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(nox_tests tests/test_client.cpp)
    target_link_libraries(nox_tests PRIVATE nox_core Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(nox_tests)
endif()

# Native release packages. Dependencies are linked statically by the release
# workflow, leaving the installed package with only the `nox` executable.
set(NOX_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Architecture used in release package names")
string(TOLOWER "${NOX_PACKAGE_ARCH}" NOX_PACKAGE_ARCH_LOWER)
set(CPACK_PACKAGE_NAME "nox-vault")
set(CPACK_PACKAGE_VENDOR "NOX Vault")
set(CPACK_PACKAGE_CONTACT "NOX Vault maintainers <noreply@noxvault.tech>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Zero-knowledge command-line secrets vault")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Progerleonid/NOX-Vault")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NOX Vault")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_PACKAGE_FILE_NAME "nox-vault-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${NOX_PACKAGE_ARCH_LOWER}")
set(CPACK_STRIP_FILES TRUE)

if(WIN32)
    # NSIS produces a familiar setup.exe and can add the installation's bin
    # directory to the system PATH. It is removed again on uninstall.
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "NOX Vault")
    set(CPACK_NSIS_PACKAGE_NAME "NOX Vault")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
elseif(APPLE)
    # The PKG installs /usr/local/bin/nox. TGZ is the no-root portable option.
    set(CPACK_GENERATOR "productbuild;TGZ")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
    set(CPACK_PRODUCTBUILD_IDENTIFIER "tech.noxvault.cli")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")
    set(CPACK_RPM_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.rpm")
    set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
endif()

include(CPack)
