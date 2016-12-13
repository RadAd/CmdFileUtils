#ifndef CONSOLEUTILS_H
#define CONSOLEUTILS_H

#include <Windows.h>

inline WORD GetConsoleTextAttribute(HANDLE Console)
{
    CONSOLE_SCREEN_BUFFER_INFO	ConsoleInfo = { sizeof(CONSOLE_SCREEN_BUFFER_INFO) };
    GetConsoleScreenBufferInfo(Console, &ConsoleInfo);
    return ConsoleInfo.wAttributes;
}

inline int GetConsoleWidth(HANDLE Console)
{
    CONSOLE_SCREEN_BUFFER_INFO	ConsoleInfo = { sizeof(CONSOLE_SCREEN_BUFFER_INFO) };
    GetConsoleScreenBufferInfo(Console, &ConsoleInfo);
    return ConsoleInfo.srWindow.Right - ConsoleInfo.srWindow.Left + 1;
}

#endif
