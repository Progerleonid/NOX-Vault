#include "nox/clipboard.hpp"
#include "nox/errors.hpp"
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace nox {
namespace {
#ifdef _WIN32
std::wstring utf8_to_wide(const std::string &value) {
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        throw NoxError("Secret is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), length);
    return result;
}
#endif
} // namespace

void copy_to_clipboard(const std::string &value) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr))
        throw NoxError("Unable to open the Windows clipboard");
    if (!EmptyClipboard()) {
        CloseClipboard();
        throw NoxError("Unable to clear the Windows clipboard");
    }
    std::wstring wide;
    try {
        wide = utf8_to_wide(value);
    } catch (...) {
        CloseClipboard();
        throw;
    }
    const auto length = static_cast<int>(wide.size());
    auto memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(length + 1) * sizeof(wchar_t));
    if (!memory) {
        CloseClipboard();
        throw NoxError("Unable to allocate clipboard memory");
    }
    auto *target = static_cast<wchar_t *>(GlobalLock(memory));
    std::copy(wide.begin(), wide.end(), target);
    target[length] = L'\0';
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        throw NoxError("Unable to set clipboard data");
    }
    CloseClipboard();
#elif defined(__APPLE__)
    (void)value;
    throw NoxError("Native macOS clipboard backend is unavailable in this build");
#else
    (void)value;
    throw NoxError("No safe native clipboard backend is available on this Unix platform");
#endif
}

bool clear_clipboard_if_matches(const std::string &value) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr))
        return false;
    bool cleared = false;
    const auto handle = GetClipboardData(CF_UNICODETEXT);
    if (handle) {
        const auto *current = static_cast<const wchar_t *>(GlobalLock(handle));
        if (current) {
            bool matches = false;
            try {
                matches = utf8_to_wide(value) == current;
            } catch (...) {
            }
            GlobalUnlock(handle);
            if (matches)
                cleared = EmptyClipboard() != FALSE;
        }
    }
    CloseClipboard();
    return cleared;
#else
    (void)value;
    return false;
#endif
}
} // namespace nox
