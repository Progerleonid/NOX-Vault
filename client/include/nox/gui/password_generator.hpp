#pragma once

#include <QString>

namespace nox::gui {
QString generate_password(int length, bool upper, bool lower, bool numbers, bool symbols);
}
