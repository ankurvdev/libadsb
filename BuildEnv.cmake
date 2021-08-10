cmake_minimum_required(VERSION 3.19)
include(GenerateExportHeader)
#set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(BuildEnvCMAKE_LOCATION "${CMAKE_CURRENT_LIST_DIR}")
if (UNIX AND NOT ANDROID)
    set(LINUX 1)
endif()


macro(_FixFlags name)
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
    set(${name} "${cflagsstr}")
endmacro()

macro(EnableStrictCompilation)
    find_package(Threads)
    file(TIMESTAMP ${CMAKE_CURRENT_LIST_FILE} filetime)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(extraflags
            /Wall   # Enable all errors
            /WX     # All warnings as errors
            /await
            /permissive- # strict compilation
            /DWIN32
            /D_WINDOWS
            /DNOMINMAX
            /DWIN32_LEAN_AND_MEAN
            /bigobj
            /guard:cf
            /std:c++latest
            /Zc:__cplusplus
            #suppression list
            /wd4068  # unknown pragma
            /wd4514  # unreferenced inline function has been removed
            /wd4820  # bytes padding added after data member in struct
            /wd5045  # Spectre mitigation insertion
            # TODO : Revisit these with newer VS Releases
            /wd4710  # Function not inlined. VS2019 CRT throws this
            /wd4711  # Function selected for automatic inline. VS2019 CRT throws this
            /wd4738  # storing 32-bit float result in memory, possible loss of performance 10.0.19041.0\ucrt\corecrt_math.h(642)
        )

        set(exclusions "[-/]W[a-zA-Z1-9]+" "[-/]permissive?")
        link_libraries(WindowsApp.lib rpcrt4.lib onecoreuap.lib kernel32.lib)

        _FixFlags(CMAKE_C_FLAGS     EXCLUDE ${exclusions} APPEND ${extraflags})
        _FixFlags(CMAKE_CXX_FLAGS   EXCLUDE ${exclusions} APPEND ${extraflags})
        _FixFlags(CMAKE_C_FLAGS_DEBUG APPEND /RTCcsu)

    elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang) OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL GNU))
        set(extraflags
            -g
            -fPIC
            -Wl,--exclude-libs,ALL
            -Wno-unused-command-line-argument
            -fvisibility=hidden
            -Wall   # Enable all errors
            -Werror     # All warnings as errors
            -Wextra
            -pedantic
            -pedantic-errors
            -pthread
        )
        set(extracxxflags
            -std=c++20
            -fvisibility-inlines-hidden
        )
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang)
            list(APPEND extraflags -Weverything)
        endif()
        list(APPEND extracxxflags
            #suppression list
            -Wno-ctad-maybe-unsupported
            -Wno-unknown-argument
            -Wno-unknown-pragmas
            -Wno-unknown-warning
            -Wno-unknown-warning-option
            -Wno-documentation
            -Wno-covered-switch-default
            -Wno-documentation-unknown-command
            -Wno-c++98-compat # Dont care about c++98 compatibility
            -Wno-c++98-compat-pedantic
            -Wno-c++98-c++11-c++14-compat
            -Wno-padded # Dont care about auto padding
            -Wno-return-std-move-in-c++11 # Rely on guaranteed copy ellisioning
            -Wno-weak-vtables # Virtual Classes will actually be virtual
        )

        set(exclusions "[-/]W[a-zA-Z1-9]+")
        _FixFlags(CMAKE_C_FLAGS     EXCLUDE ${exclusions} APPEND ${extraflags})
        _FixFlags(CMAKE_CXX_FLAGS   EXCLUDE ${exclusions} APPEND ${extraflags} ${extracxxflags})
        #message(FATAL_ERROR "Unknown compiler : ${CMAKE_CXX_COMPILER_ID}")
    else()
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
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "")
            message("Unidentified compiler")
        else()
            message(FATAL_ERROR "Unknown compiler ::${CMAKE_CXX_COMPILER_ID}::")
        endif()
    endif()
    set(STRICT_COMPILATION_MODE ${filetime} CACHE INTERNAL "Is Strict Compilation mode enabled" FORCE)
    if (EXISTS ${BuildEnvCMAKE_LOCATION}/CommonMacros.h)
        include_directories(${BuildEnvCMAKE_LOCATION})
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
    if (EXISTS  "${path}/.git")
        return()
    endif()
    find_package(Git QUIET REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive --single-branch "${path}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()
