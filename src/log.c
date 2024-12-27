#include "log.h"

#include <stdarg.h>
#include <stdio.h>

void log(const char* fmt, ...) {
    // Pass to printf for now
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void log_windows_error(const char* text, const DWORD code) {
    LPSTR error_text = get_error_text(code);
    if(error_text == NULL) {
        log(text);
    }
    else {
        log("%s with error: %s", text, error_text);
        LocalFree(error_text);
    }
}

LPTSTR get_error_text(const int code) {
    const LPTSTR message_buffer;

    const size_t size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&message_buffer,
        0,
        NULL
    );

    if(size == 0) {
        log("FormatMessage failed with error code: %d\n", GetLastError());
        return NULL;
    }

    return message_buffer;
}
