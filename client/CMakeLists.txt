cmake_minimum_required(VERSION 3.21)
project(nox_vault_client VERSION 0.3.0 LANGUAGES CXX)

set(NOX_PRODUCT_NAME "NOX Vault")
set(NOX_VENDOR "NOX Vault Team")
set(NOX_HOMEPAGE "https://github.com/Progerleonid/NOX-Vault")
set(NOX_PACKAGING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packaging")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(NOX_BUILD_CLI "Build the NOX Vault command-line client" ON)
option(NOX_BUILD_GUI "Build the NOX Vault Qt desktop GUI" OFF)
option(NOX_MSVC_STATIC_RUNTIME "Use the static MSVC runtime for NOX targets" ON)
if(MSVC AND NOX_MSVC_STATIC_RUNTIME)
    # Release dependencies use the x64-windows-static vcpkg triplet. Match its
    # /MT runtime so the executable is self-contained and ABI-compatible.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set(NOX_DEFAULT_SERVER_URL "https://api.noxvault.tech" CACHE STRING "Official NOX server URL")
option(NOX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(NOX_BUILD_CLI)
    find_package(CLI11 CONFIG REQUIRED)
endif()
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

if(NOX_BUILD_CLI)
    add_executable(nox src/main.cpp)
    target_link_libraries(nox PRIVATE nox_core CLI11::CLI11)
endif()

if(NOX_BUILD_GUI)
    if(NOT WIN32)
        message(FATAL_ERROR "NOX_BUILD_GUI currently supports Windows only")
    endif()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent)
    qt_standard_project_setup(REQUIRES 6.8)
    qt_add_executable(nox_gui WIN32
        include/nox/gui/app_controller.hpp include/nox/gui/secrets_model.hpp
        src/gui/main.cpp src/gui/app_controller.cpp src/gui/password_generator.cpp
        src/gui/secrets_model.cpp src/gui/nox_gui.rc)
    qt_add_qml_module(nox_gui
        URI NoxVault VERSION 1.0
        QML_FILES
            qml/Main.qml
            qml/components/AppButton.qml qml/components/AppCheckBox.qml
            qml/components/AppDialog.qml qml/components/FormField.qml
            qml/components/SidebarButton.qml qml/components/StatusPill.qml
            qml/pages/AuthPage.qml qml/pages/BackupPage.qml
            qml/pages/DiagnosticsPage.qml qml/pages/GeneratorPage.qml
            qml/pages/SecurityPage.qml qml/pages/SecretsPage.qml
            qml/pages/SettingsPage.qml qml/pages/UnlockPage.qml
            qml/pages/VaultSetupPage.qml)
    target_include_directories(nox_gui PRIVATE include)
    target_link_libraries(nox_gui PRIVATE nox_core Qt6::Core Qt6::Gui Qt6::Qml
        Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent)
    set(nox_gui_icon "${NOX_PACKAGING_DIR}/assets/nox-vault.ico")
    set_source_files_properties("${nox_gui_icon}" PROPERTIES QT_RESOURCE_ALIAS "nox-vault.ico")
    qt_add_resources(nox_gui "nox_gui_assets" PREFIX "/assets" FILES "${nox_gui_icon}")
    if(WIN32)
        target_link_libraries(nox_gui PRIVATE dwmapi)
    endif()
    add_custom_command(TARGET nox_gui POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --$<LOWER_CASE:$<CONFIG>>
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" --no-translations
                --skip-plugin-types qmltooling,generic
                --dir "$<TARGET_FILE_DIR:nox_gui>" "$<TARGET_FILE:nox_gui>"
        COMMENT "Deploying Qt runtime for local GUI launches")
    # Keep the build-tree name free of spaces because Qt 6.8's generated
    # Windows deployment script does not quote the EXECUTABLE argument. The
    # installed product is renamed after deployment.
    set_target_properties(nox_gui PROPERTIES OUTPUT_NAME "nox_gui")
    if(MSVC)
        target_compile_options(nox_gui PRIVATE /W4 /permissive- /EHsc)
    endif()
endif()

if(WIN32)
    if(NOX_BUILD_CLI)
        install(TARGETS nox RUNTIME DESTINATION bin)
        install(FILES $<TARGET_RUNTIME_DLLS:nox> DESTINATION bin)
    endif()
    if(NOX_BUILD_GUI)
        install(TARGETS nox_gui RUNTIME DESTINATION .)
        install(FILES $<TARGET_RUNTIME_DLLS:nox_gui> DESTINATION .)
        # windeployqt is used directly because Qt 6.8's generated deployment
        # script does not quote executable paths whose parent directories
        # contain spaces (a common Windows user-profile layout).
        install(CODE "execute_process(COMMAND \"$<TARGET_FILE:Qt6::windeployqt>\"
            --$<LOWER_CASE:$<CONFIG>> --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/qml\"
            --no-translations --skip-plugin-types qmltooling,generic
            --dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\" \"$<TARGET_FILE:nox_gui>\"
            COMMAND_ERROR_IS_FATAL ANY)")
        install(CODE [[
            file(RENAME
                "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/nox_gui.exe"
                "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/NOX Vault.exe")
        ]])
        if(MSVC)
            # setup-msbuild does not keep VCINSTALLDIR in later CI steps, so
            # locate the app-local VC runtime relative to the selected cl.exe.
            set(nox_vc_root "${CMAKE_CXX_COMPILER}")
            foreach(_unused RANGE 1 7)
                cmake_path(GET nox_vc_root PARENT_PATH nox_vc_root)
            endforeach()
            file(GLOB nox_vc_redist_dirs LIST_DIRECTORIES TRUE
                "${nox_vc_root}/Redist/MSVC/*/x64/Microsoft.VC143.CRT")
            if(NOT nox_vc_redist_dirs)
                message(FATAL_ERROR "Unable to locate the Visual C++ x64 app-local runtime")
            endif()
            list(SORT nox_vc_redist_dirs COMPARE NATURAL ORDER DESCENDING)
            list(GET nox_vc_redist_dirs 0 nox_vc_redist_dir)
            install(DIRECTORY "${nox_vc_redist_dir}/" DESTINATION .
                    FILES_MATCHING PATTERN "*.dll")
        endif()
    endif()
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET AND
       EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        if(NOX_BUILD_CLI)
            install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/"
                    DESTINATION bin FILES_MATCHING PATTERN "*.dll")
        endif()
        if(NOX_BUILD_GUI)
            install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/"
                    DESTINATION . FILES_MATCHING PATTERN "*.dll"
                    PATTERN "pkgconf*.dll" EXCLUDE)
        endif()
    endif()
elseif(APPLE AND NOX_BUILD_CLI)
    install(TARGETS nox RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
            DESTINATION share/nox-vault)
    install(PROGRAMS "${NOX_PACKAGING_DIR}/macos/uninstall-nox-vault"
            DESTINATION share/nox-vault)
elseif(NOX_BUILD_CLI)
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
    add_executable(nox_agent_entrypoint_tests tests/test_agent_entrypoint.cpp)
    target_link_libraries(nox_agent_entrypoint_tests PRIVATE nox_core)
    if(NOX_BUILD_GUI)
        add_test(NAME agent_entrypoint_lifecycle COMMAND nox_agent_entrypoint_tests
            $<TARGET_FILE:nox_gui> $<TARGET_FILE:nox_gui>)
        set_tests_properties(agent_entrypoint_lifecycle PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    elseif(NOX_BUILD_CLI)
        add_test(NAME agent_entrypoint_lifecycle COMMAND nox_agent_entrypoint_tests
            $<TARGET_FILE:nox> $<TARGET_FILE:nox>)
    endif()
    if(NOX_BUILD_GUI)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_executable(nox_gui_tests tests/test_gui.cpp include/nox/gui/app_controller.hpp
            include/nox/gui/secrets_model.hpp src/gui/app_controller.cpp
            src/gui/password_generator.cpp src/gui/secrets_model.cpp)
        target_include_directories(nox_gui_tests PRIVATE include)
        target_link_libraries(nox_gui_tests PRIVATE nox_core Qt6::Core Qt6::Concurrent Qt6::Test)
        add_custom_command(TARGET nox_gui_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:nox_gui_tests> $<TARGET_FILE_DIR:nox_gui_tests>
            COMMAND_EXPAND_LISTS)
        add_test(NAME nox_gui_tests COMMAND nox_gui_tests)
    endif()
endif()

# Linux packages use the normal CMake install graph. Windows and macOS consume
# the same graph from their native WiX/productbuild scripts.
if(UNIX AND NOT APPLE AND NOX_BUILD_CLI)
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
