#include "nox/input.hpp"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace nox {
std::string read_hidden(const std::string &prompt) {
    std::cerr << prompt;
    std::string value;
#ifdef _WIN32
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool terminal = GetConsoleMode(input, &mode) != 0;
    if (terminal)
        SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT);
    std::getline(std::cin, value);
    if (terminal)
        SetConsoleMode(input, mode);
#else
    termios old{};
    const bool terminal = tcgetattr(STDIN_FILENO, &old) == 0;
    auto hidden = old;
    if (terminal) {
        hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &hidden);
    }
    std::getline(std::cin, value);
    if (terminal)
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
#endif
    std::cerr << '\n';
    return value;
}
} // namespace nox
