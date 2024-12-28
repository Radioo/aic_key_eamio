#ifndef LOG_H
#define LOG_H

#include <windows.h>

void my_log(const char* fmt, ...);
void log_windows_error(const char* text, DWORD code);
LPTSTR get_error_text(int code);

#endif