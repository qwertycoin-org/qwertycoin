# Refresh version.cpp at build time so binaries cannot retain the revision from
# an earlier commit merely because an existing CMake build tree was reused.

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "SOURCE_DIR and BINARY_DIR are required")
endif()

set(VERSION_IS_RELEASE "false")
set(VERSIONTAG "unknown")

if(DEFINED GIT_EXECUTABLE AND NOT GIT_EXECUTABLE STREQUAL "")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short=9 HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE GIT_RESULT
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(GIT_RESULT EQUAL 0)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" tag -l --points-at HEAD
      WORKING_DIRECTORY "${SOURCE_DIR}"
      RESULT_VARIABLE TAG_RESULT
      OUTPUT_VARIABLE GIT_TAG
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(TAG_RESULT EQUAL 0 AND NOT GIT_TAG STREQUAL "")
      set(VERSIONTAG "release")
      set(VERSION_IS_RELEASE "true")
    else()
      set(VERSIONTAG "${GIT_COMMIT}")
    endif()
  endif()
endif()

configure_file(
  "${SOURCE_DIR}/src/version.cpp.in"
  "${BINARY_DIR}/version.cpp"
  @ONLY)
