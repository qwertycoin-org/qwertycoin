// Copyright 2018-2019 (c) The Qwertycoin Group.
// Licensed under the GNU General Public License, Version 3.
// See the file LICENSE from this package for details.

#include <iostream>
#include <Breakpad/Breakpad.h>

#if defined(__linux__) && !defined(__ANDROID__) // Linux
#include <client/linux/handler/exception_handler.h>
#elif defined(__APPLE__) // macOS
#include <client/mac/handler/exception_handler.h>
#elif defined(_WIN32) || defined(WIN32) // Windows
#include <client/windows/handler/exception_handler.h>
#else
#define QWERTYCOIN_BREAKPAD_UNAVAILABLE 1 // Silently disable Google Breakpad if not supported.
#endif

#if defined(__linux__) && !defined(__ANDROID__) // Linux
static bool exceptionHandlerCallback(
    const google_breakpad::MinidumpDescriptor &descriptor,
    void *context,
    bool succeeded)
{
    std::cout << "ERROR: Process crashed! Minidump created." << std::endl;
    std::cout << "Minidump dir: " << descriptor.path() << std::endl;

    return true;
}
#elif defined(__APPLE__) // macOS
static bool exceptionHandlerCallback(
    const char *minidumpDir,
    const char *minidumpId,
    void *context,
    bool succeeded)
{
    std::cout << "ERROR: Process crashed! Minidump created." << std::endl;
    std::cout << "Minidump dir: " << minidumpDir << ", id: " << minidumpId << std::endl;

    return true;
}
#elif defined(_WIN32) || defined(WIN32) // Windows
static bool exceptionHandlerCallback(
    const wchar_t *minidumpPath,
    const wchar_t *minidumpId,
    void *context,
    EXCEPTION_POINTERS *exinfo,
    MDRawAssertionInfo *assertion,
    bool succeeded)
{
    std::cout << "ERROR: Process crashed! Minidump created." << std::endl;
    std::cout << "Minidump path: " << minidumpPath << ", id: " << minidumpId << std::endl;

    return true;
}
#endif

Qwertycoin::Breakpad::ExceptionHandler::ExceptionHandler(const std::string &dumpPath)
{
    std::string validDumpPath;
    if (!dumpPath.empty()) {
        validDumpPath = dumpPath;
    } else {
#if defined(_WIN32) || defined(WIN32) // Windows
        validDumpPath = std::string("C:\\Windows\\Temp");
#else // Linux, macOS, etc.
        validDumpPath = std::string("/tmp");
#endif
    }

#ifdef QWERTYCOIN_BREAKPAD_UNAVAILABLE
    m_exceptionHandler = nullptr;
#else // in case breakpad is available
    m_exceptionHandler = new google_breakpad::ExceptionHandler{
#   if defined(__linux__) && !defined(__ANDROID__) // Linux
        google_breakpad::MinidumpDescriptor(validDumpPath),
        nullptr,
        exceptionHandlerCallback,
        nullptr,
        true,
        -1
#   elif defined(__APPLE__) // macOS
        validDumpPath,
        nullptr,
        exceptionHandlerCallback,
        nullptr,
        true,
        nullptr
#   elif defined(_WIN32) || defined(WIN32) // Windows
        std::wstring(validDumpPath.begin(), validDumpPath.end())
        NULL,
        exceptionHandlerCallback,
        NULL,
        google_breakpad::ExceptionHandler::HANDLER_ALL
#   endif
    };
#endif // QWERTYCOIN_BREAKPAD_UNAVAILABLE
}

Qwertycoin::Breakpad::ExceptionHandler::~ExceptionHandler()
{
    if (m_exceptionHandler) {
        delete m_exceptionHandler;
        m_exceptionHandler = nullptr;
    }
}

/*!
    WARNING: This function will crash running process! Use only for testing purposes.
*/
void Qwertycoin::Breakpad::ExceptionHandler::dummyCrash()
{
    int *a = (int *)0x42;
    fprintf(stdout, "Going to crash...\n");
    fprintf(stdout, "A = %d", *a);
}
