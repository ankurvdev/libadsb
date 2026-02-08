include_guard()
include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.10.0
    SOURCE_SUBDIR .
    GIT_PROGRESS TRUE
    GIT_SHALLOW 1
    SYSTEM
    FIND_PACKAGE_ARGS NAMES Catch2
)
if (COMMAND vcpkg_install)
    vcpkg_install(catch2)
endif()

FetchContent_MakeAvailable(catch2)
if (TARGET Catch2)
if (COMMAND SupressWarningForTarget)
    SupressWarningForTarget(Catch2)
endif()
if(MSVC)
target_compile_options(Catch2 PUBLIC /wd4868)
endif()
endif()

if (COMMAND SupressWarningForTarget AND TARGET Catch2WithMain)
    SupressWarningForTarget(Catch2WithMain)
endif()
