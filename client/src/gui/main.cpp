#include "nox/gui/app_controller.hpp"
#include "nox/agent.hpp"
#include "nox/config_manager.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <cstdio>
#endif

namespace {
void enable_console_output() {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *stream = nullptr;
        (void)freopen_s(&stream, "CONOUT$", "w", stdout);
        (void)freopen_s(&stream, "CONOUT$", "w", stderr);
    }
#endif
}
bool has_argument(int argc, char **argv, const std::string &value) {
    for (int index = 1; index < argc; ++index)
        if (argv[index] == value)
            return true;
    return false;
}
} // namespace

int main(int argc, char **argv) {
    if (argc >= 3 && std::string(argv[1]) == "agent" && std::string(argv[2]) == "--serve") {
        QCoreApplication app(argc, argv);
        const nox::ConfigManager config;
        const auto settings = config.load();
        return nox::run_agent(settings.unlock_timeout_seconds, 8 * 60 * 60);
    }
    if (has_argument(argc, argv, "--version") || has_argument(argc, argv, "--help")) {
        enable_console_output();
        if (has_argument(argc, argv, "--version"))
            std::cout << NOX_VERSION << '\n';
        else
            std::cout << "NOX Vault desktop client\n\nOptions:\n  --version\n  --help\n  --smoke-test\n";
        return 0;
    }
    const bool smoke = has_argument(argc, argv, "--smoke-test");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("NOX Vault");
    QGuiApplication::setOrganizationName("NOX Vault Team");
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/nox-vault.ico")));
#ifdef _WIN32
    (void)SetCurrentProcessExplicitAppUserModelID(L"NOXVault.Desktop");
#endif
    nox::gui::AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);
    engine.rootContext()->setContextProperty("smokeTest", smoke);
    engine.loadFromModule("NoxVault", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
#ifdef _WIN32
    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().front())) {
        const HWND handle = reinterpret_cast<HWND>(window->winId());
        const BOOL dark = TRUE;
        const COLORREF caption = RGB(16, 16, 20);
        const COLORREF border = RGB(41, 41, 50);
        (void)DwmSetWindowAttribute(handle, 20, &dark, sizeof(dark));
        (void)DwmSetWindowAttribute(handle, 35, &caption, sizeof(caption));
        (void)DwmSetWindowAttribute(handle, 34, &border, sizeof(border));
    }
#endif
    return smoke ? 0 : app.exec();
}
