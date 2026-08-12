#include "nox/clipboard.hpp"
#include "nox/errors.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace nox {
void copy_to_clipboard(const std::string& value) {
#ifdef _WIN32
 if(!OpenClipboard(nullptr))throw NoxError("Unable to open the Windows clipboard");
 if(!EmptyClipboard()){CloseClipboard();throw NoxError("Unable to clear the Windows clipboard");}
 int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);if(length<=0){CloseClipboard();throw NoxError("Secret is not valid UTF-8");}
 auto memory=GlobalAlloc(GMEM_MOVEABLE,static_cast<SIZE_T>(length+1)*sizeof(wchar_t));if(!memory){CloseClipboard();throw NoxError("Unable to allocate clipboard memory");}
 auto*target=static_cast<wchar_t*>(GlobalLock(memory));MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),target,length);target[length]=L'\0';GlobalUnlock(memory);
 if(!SetClipboardData(CF_UNICODETEXT,memory)){GlobalFree(memory);CloseClipboard();throw NoxError("Unable to set clipboard data");}CloseClipboard();
#elif defined(__APPLE__)
 throw NoxError("Native macOS clipboard backend is unavailable in this build");
#else
 throw NoxError("No safe native clipboard backend is available on this Unix platform");
#endif
}
}
