#pragma once
#include <string>
namespace nox {
void copy_to_clipboard(const std::string &value);
bool clear_clipboard_if_matches(const std::string &value);
}
