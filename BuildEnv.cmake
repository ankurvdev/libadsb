cmake_minimum_required(VERSION 3.19)
include(GenerateExportHeader)
#set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if (MINGW)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES   OFF)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
endif()

set(BuildEnvCMAKE_LOCATION "${CMAKE_CURRENT_LIST_DIR}")
if (UNIX AND NOT ANDROID)
    set(LINUX 1)
endif()
if (IS_DIRECTORY ${BuildEnvCMAKE_LOCATION}/../Format.cmake AND NOT SKIP_FORMAT)
    if (NOT TARGET fix-clang-format)
        add_subdirectory(${BuildEnvCMAKE_LOCATION}/../Format.cmake Format.cmake)
    endif()
endif()
macro(_PrintFlags)
    message(STATUS "CMAKE_C_FLAGS_INIT             : ${CMAKE_C_FLAGS_INIT}")
    message(STATUS "CMAKE_CXX_FLAGS_INIT           : ${CMAKE_CXX_FLAGS_INIT}")
    message(STATUS "CMAKE_C_FLAGS                  : ${CMAKE_C_FLAGS}")
    message(STATUS "CMAKE_C_FLAGS_DEBUG            : ${CMAKE_C_FLAGS_DEBUG}")
    message(STATUS "CMAKE_C_FLAGS_MINSIZEREL       : ${CMAKE_C_FLAGS_MINSIZEREL}")
    message(STATUS "CMAKE_C_FLAGS_RELEASE          : ${CMAKE_C_FLAGS_RELEASE}")
    message(STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO   : ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    message(STATUS "CMAKE_CXX_FLAGS                : ${CMAKE_CXX_FLAGS}")
    message(STATUS "CMAKE_CXX_FLAGS_DEBUG          : ${CMAKE_CXX_FLAGS_DEBUG}")
    message(STATUS "CMAKE_CXX_FLAGS_MINSIZEREL     : ${CMAKE_CXX_FLAGS_MINSIZEREL}")
    message(STATUS "CMAKE_CXX_FLAGS_RELEASE        : ${CMAKE_CXX_FLAGS_RELEASE}")
    message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO : ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    message(STATUS "CMAKE_SYSTEM_PROCESSOR         : ${CMAKE_SYSTEM_PROCESSOR}")
endmacro()

function(_FixFlags name)
    cmake_parse_arguments("" "" "" "VALUE;EXCLUDE;APPEND" ${ARGN})
    if (NOT _VALUE)
        if (NOT ${name})
            set(_VALUE " ")
        else()
            set(_VALUE ${${name}})
        endif()
    endif()

    set(origlist ${_VALUE})

    string(REPLACE " " ";" origlist ${origlist})

    foreach (val ${origlist})
        list(APPEND cflags ${val})
    endforeach()
    foreach(excl ${_EXCLUDE})
        list(FILTER cflags EXCLUDE REGEX ${excl})
    endforeach()

    list(APPEND cflags ${_APPEND})
    list(REMOVE_DUPLICATES cflags)
    list(JOIN cflags " " cflagsstr)

    message(STATUS "${name}: ${_VALUE} ==> ${cflagsstr}")
    set(${name} "${cflagsstr}" PARENT_SCOPE)
endfunction()

macro(EnableStrictCompilation)
    find_package(Threads)
    file(TIMESTAMP ${CMAKE_CURRENT_LIST_FILE} filetime)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(extraflags
            /Wall   # Enable all errors
            /WX     # All warnings as errors
            # /await
            /permissive- # strict compilation
            /DWIN32
            /D_WINDOWS
            /DNOMINMAX
            /DWIN32_LEAN_AND_MEAN
            /bigobj
            /guard:cf
            /std:c++20
            /Zc:__cplusplus
            #suppression list
            /wd4619  # pragma warning: there is no warning number
            /wd4068  # unknown pragma
            /wd4514  # unreferenced inline function has been removed
            /wd4820  # bytes padding added after data member in struct
            /wd5039  #  pointer or reference to potentially throwing function passed to 'extern "C"' function under -EHc.
            /wd5045  # Spectre mitigation insertion
            # TODO : Revisit these with newer VS Releases
            /wd4710  # Function not inlined. VS2019 CRT throws this
            /wd4711  # Function selected for automatic inline. VS2019 CRT throws this
            /wd4738  # storing 32-bit float result in memory, possible loss of performance 10.0.19041.0\ucrt\corecrt_math.h(642)
            /wd4746  # volatile access of 'b' is subject to /volatile:<iso|ms>
            # TODO : Revisit with later cmake release. This causes cmake autodetect HAVE_STRUCT_TIMESPEC to fail
            /wd4255  # The compiler did not find an explicit list of arguments to a function. This warning is for the C compiler only.
	    /wd5246  # MSVC Bug VS2022 :  the initialization of a subobject should be wrapped in braces
        )

        set(exclusions "[-/]W[a-zA-Z1-9]+" "[-/]permissive?")
        link_libraries(WindowsApp.lib rpcrt4.lib onecoreuap.lib kernel32.lib)

        _FixFlags(CMAKE_C_FLAGS     EXCLUDE ${exclusions} APPEND ${extraflags})
        _FixFlags(CMAKE_CXX_FLAGS   EXCLUDE ${exclusions} APPEND ${extraflags})
        # /RTCc rejects code that conforms to the standard, it's not supported by the C++ Standard Library
        # RTCc Reports when a value is assigned to a smaller data type and results in a data loss.
        # RTCs Enables stack frame run-time error checking, as follows:
        # RTCu Reports when a variable is used without having been initialized
        _FixFlags(CMAKE_C_FLAGS_DEBUG APPEND /RTCsu)
    elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang) OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL GNU))
        set(extraflags
            -g
            -fPIC
            -Wl,--exclude-libs,ALL
            -fvisibility=hidden
            -Wall   # Enable all errors
            -Werror     # All warnings as errors
            -Wextra
            -pedantic
            -pedantic-errors
            -pthread
            # Remove unused code
            -ffunction-sections
            -fdata-sections
            -Wl,--gc-sections
        )
        set(extracxxflags
            -std=c++20
            -fvisibility-inlines-hidden
        )

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang)
            list(APPEND extraflags
                -Weverything
                -Wno-weak-vtables # Virtual Classes will actually be virtual
                -Wno-return-std-move-in-c++11 # Rely on guaranteed copy ellisioning
                -Wno-c++98-c++11-c++14-compat
                -Wno-documentation-unknown-command
                -Wno-covered-switch-default
                -Wno-documentation
                -Wno-unknown-warning-option
                -Wno-unknown-warning
                -Wno-unknown-argument
                -Wno-c99-extensions
                -Wno-unused-command-line-argument
                -Wno-c++98-compat # Dont care about c++98 compatibility
                -Wno-c++20-compat
                -Wno-c++98-compat-pedantic
                -Wno-reserved-identifier # Allow names starting with underscore
                -Wno-reserved-id-macro
                )
        endif()

        if (MINGW)
            # TODO GCC Bug: Compiling with -O1 can sometimes result errors
            # due to running out of string slots (file too big)
            list(APPEND extraflags -O1)

            list(APPEND extraflags -Wa,-mbig-obj)
            list(APPEND extraflags -DWIN32=1 -D_WINDOWS=1 -DWIN32_LEAN_AND_MEAN=1)
            list(APPEND extracxxflags -DNOMINMAX=1)
        endif()

        list(APPEND extracxxflags
            #suppression list
            -Wno-ctad-maybe-unsupported
            -Wno-unknown-pragmas
            -Wno-padded # Dont care about auto padding
        )

        set(exclusions "[-/]W[a-zA-Z1-9]+")
        _FixFlags(CMAKE_C_FLAGS     EXCLUDE ${exclusions} APPEND ${extraflags})
        _FixFlags(CMAKE_CXX_FLAGS   EXCLUDE ${exclusions} APPEND ${extraflags} ${extracxxflags})
        _PrintFlags()
    else()
        _PrintFlags()
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "")
            message("Unidentified compiler")
        else()
            message(FATAL_ERROR "Unknown compiler ::${CMAKE_CXX_COMPILER_ID}::")
        endif()
    endif()
    set(STRICT_COMPILATION_MODE ${filetime} CACHE INTERNAL "Is Strict Compilation mode enabled" FORCE)
    if (EXISTS ${BuildEnvCMAKE_LOCATION}/../include)
        include_directories(${BuildEnvCMAKE_LOCATION}/../include)
    endif()
endmacro()

macro (SupressWarningForFile f)
    message(STATUS "Suppressing Warnings for ${f}")
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set_source_files_properties("${f}" PROPERTIES COMPILE_FLAGS "/W3")
    elseif((${CMAKE_CXX_COMPILER_ID} STREQUAL Clang) OR (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU))
        set_source_files_properties("${f}" PROPERTIES COMPILE_FLAGS "-Wno-error -w")
    else()
        message(FATAL_ERROR "Unknown compiler : ${CMAKE_CXX_COMPILER_ID}")
    endif()
endmacro()


macro (SupressWarningForTarget targetName)
    message(STATUS "Suppressing Warnings for ${targetName}")
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${targetName} PRIVATE /W3 /WX-)
    elseif((${CMAKE_CXX_COMPILER_ID} STREQUAL Clang) OR (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU))
        target_compile_options(${targetName} PRIVATE -Wno-error -w)
    else()
        message(FATAL_ERROR "Unknown compiler : ${CMAKE_CXX_COMPILER_ID}")
    endif()
endmacro()

function(init_submodule path)
    cmake_parse_arguments("" "" "SUBDMODULE_DIRECTORY" "" ${ARGN})
    set(srcdir "${CMAKE_CURRENT_SOURCE_DIR}")
    if (DEFINED _SUBDMODULE_DIRECTORY)
        set(srcdir "${_SUBDMODULE_DIRECTORY}")
    elseif (DEFINED INIT_SUBMODULE_DIRECTORY)
        set(srcdir "${INIT_SUBMODULE_DIRECTORY}")
    endif()
    if ((IS_DIRECTORY "${srcdir}/${path}"))
        file(GLOB files "${srcdir}/${path}/*")
        if (NOT "${files}" STREQUAL "")
            return()
        endif()
    endif()
    find_package(Git QUIET REQUIRED)
    message(STATUS "Submodule Update: ${srcdir}/${path}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive --single-branch "${path}"
        WORKING_DIRECTORY "${srcdir}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()
