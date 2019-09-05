# Breakpad

FetchContent_Declare(
    Breakpad
    GIT_REPOSITORY "https://github.com/google/breakpad.git"
    GIT_TAG master # TODO: Add more specific tag (like v1.0.0)
    GIT_SHALLOW TRUE
    GIT_PROGRESS FALSE

    UPDATE_COMMAND ""
    PATCH_COMMAND ""
)
FetchContent_Populate(Breakpad)

FetchContent_Declare(
    LinuxSyscallSupport
    GIT_REPOSITORY "https://chromium.googlesource.com/linux-syscall-support.git"
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS FALSE

    UPDATE_COMMAND ""
    PATCH_COMMAND ""
)
FetchContent_Populate(LinuxSyscallSupport)

set(Breakpad_INCLUDE_DIRS "${breakpad_SOURCE_DIR}/src")

if(NOT ANDROID)
    set(Breakpad_SOURCES
        "${breakpad_SOURCE_DIR}/src/common/string_conversion.cc"
        "${breakpad_SOURCE_DIR}/src/common/convert_UTF.cc"
        "${breakpad_SOURCE_DIR}/src/common/md5.cc"
    )
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    message(STATUS "Configuring Google Breakpad sources for Linux...")
    set(Breakpad_SOURCES
        ${Breakpad_SOURCES}
        "${breakpad_SOURCE_DIR}/src/client/linux/crash_generation/crash_generation_client.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/dump_writer_common/thread_info.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/dump_writer_common/ucontext_reader.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/handler/exception_handler.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/handler/minidump_descriptor.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/log/log.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/microdump_writer/microdump_writer.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/minidump_writer/linux_dumper.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/minidump_writer/linux_ptrace_dumper.cc"
        "${breakpad_SOURCE_DIR}/src/client/linux/minidump_writer/minidump_writer.cc"
        "${breakpad_SOURCE_DIR}/src/client/minidump_file_writer.cc"
        "${breakpad_SOURCE_DIR}/src/common/linux/elfutils.cc"
        "${breakpad_SOURCE_DIR}/src/common/linux/file_id.cc"
        "${breakpad_SOURCE_DIR}/src/common/linux/guid_creator.cc"
        "${breakpad_SOURCE_DIR}/src/common/linux/linux_libc_support.cc"
        "${breakpad_SOURCE_DIR}/src/common/linux/memory_mapped_file.cc"
        "${breakpad_SOURCE_DIR}/src/common/linux/safe_readlink.cc"
    )

    set(LINUX_SYSCALL_SUPPORT_HEADER_IN "${linuxsyscallsupport_SOURCE_DIR}/linux_syscall_support.h")
    set(LINUX_SYSCALL_SUPPORT_HEADER_OUT "${breakpad_SOURCE_DIR}/src/third_party/lss/linux_syscall_support.h")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E echo "Copied linux_syscall_support.h to ${breakpad_SOURCE_DIR}/src/third_party/lss"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${breakpad_SOURCE_DIR}/src/third_party/lss"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LINUX_SYSCALL_SUPPORT_HEADER_IN}" "${LINUX_SYSCALL_SUPPORT_HEADER_OUT}"
    )
    unset(LINUX_SYSCALL_SUPPORT_HEADER_IN)
    unset(LINUX_SYSCALL_SUPPORT_HEADER_OUT)
elseif(APPLE)
    message(STATUS "Configuring Google Breakpad sources for macOS...")
    set(Breakpad_SOURCES
        ${Breakpad_SOURCES}
        "${breakpad_SOURCE_DIR}/src/client/mac/crash_generation/crash_generation_client.cc"
        "${breakpad_SOURCE_DIR}/src/client/mac/handler/breakpad_nlist_64.cc"
        "${breakpad_SOURCE_DIR}/src/client/mac/handler/dynamic_images.cc"
        "${breakpad_SOURCE_DIR}/src/client/mac/handler/exception_handler.cc"
        "${breakpad_SOURCE_DIR}/src/client/mac/handler/minidump_generator.cc"
        "${breakpad_SOURCE_DIR}/src/client/mac/handler/protected_memory_allocator.cc"
        "${breakpad_SOURCE_DIR}/src/client/minidump_file_writer.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/bootstrap_compat.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/file_id.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/MachIPC.mm"
        "${breakpad_SOURCE_DIR}/src/common/mac/macho_id.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/macho_reader.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/macho_utilities.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/macho_walker.cc"
        "${breakpad_SOURCE_DIR}/src/common/mac/string_utilities.cc"
    )
elseif(WIN32)
    message(STATUS "Configuring Google Breakpad sources for Windows...")
    set(Breakpad_SOURCES
        ${Breakpad_SOURCES}
        "${breakpad_SOURCE_DIR}/src/client/windows/crash_generation/client_info.cc"
        "${breakpad_SOURCE_DIR}/src/client/windows/crash_generation/crash_generation_client.cc"
        "${breakpad_SOURCE_DIR}/src/client/windows/crash_generation/minidump_generator.cc"
        "${breakpad_SOURCE_DIR}/src/client/windows/handler/exception_handler.cc"
        "${breakpad_SOURCE_DIR}/src/common/windows/guid_string.cc"
    )
else()
    message(WARNING "Google Breakpad is not configured for current platform!")
endif()
