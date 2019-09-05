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
