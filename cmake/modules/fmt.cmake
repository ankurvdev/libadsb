include_guard()
include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG        11.2.0
    SOURCE_SUBDIR  .
    GIT_PROGRESS TRUE
    GIT_SHALLOW 1
    SYSTEM
    FIND_PACKAGE_ARGS NAMES fmt
)

if (COMMAND vcpkg_install)
    vcpkg_install(fmt)
endif()

FetchContent_MakeAvailable(fmt)
if (COMMAND SupressWarningForTarget)
    SupressWarningForTarget(fmt)
endif()
