#include "nox/gui/password_generator.hpp"
#include "nox/errors.hpp"
#include <sodium.h>
#include <array>
#include <vector>

namespace nox::gui {
QString generate_password(int length, bool upper, bool lower, bool numbers, bool symbols) {
    if (length < 8 || length > 256)
        throw NoxError("Password length must be between 8 and 256");
    const std::array<QByteArray, 4> classes = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz",
                                               "0123456789", "!@#$%^&*()-_=+[]{}:,.?"};
    const std::array<bool, 4> enabled = {upper, lower, numbers, symbols};
    QByteArray alphabet;
    std::vector<QByteArray> selected;
    for (std::size_t i = 0; i < classes.size(); ++i) {
        if (enabled[i]) {
            alphabet += classes[i];
            selected.push_back(classes[i]);
        }
    }
    if (selected.empty())
        throw NoxError("Select at least one character class");
    if (length < static_cast<int>(selected.size()))
        throw NoxError("Password is too short for the selected character classes");
    QByteArray output;
    output.reserve(length);
    for (const auto &characters : selected)
        output += characters.at(static_cast<int>(randombytes_uniform(static_cast<std::uint32_t>(characters.size()))));
    while (output.size() < length)
        output += alphabet.at(static_cast<int>(randombytes_uniform(static_cast<std::uint32_t>(alphabet.size()))));
    for (int i = output.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(randombytes_uniform(static_cast<std::uint32_t>(i + 1)));
        std::swap(output[i], output[j]);
    }
    return QString::fromLatin1(output);
}
} // namespace nox::gui
