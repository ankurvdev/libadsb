include_guard(GLOBAL)
cmake_minimum_required(VERSION 3.26)
include(GenerateExportHeader)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)
    set(LINUX 1)
endif()


# We directly enable flto=full on android
# using this causes build failures
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
if (ANDROID OR EMSCRIPTEN)
    # Flto=thin has issues with emscripten. We want to use full anyway
    set(CMAKE_CXX_COMPILE_OPTIONS_IPO "-flto=full")
endif()

set(BuildEnvCMAKE_LOCATION "${CMAKE_CURRENT_LIST_DIR}")

if (EMSCRIPTEN)
    set(Threads_FOUND 1)
    # set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_CXX_COMPILE_OPTIONS_IPO "-flto=full")
endif()

if (IS_DIRECTORY ${BuildEnvCMAKE_LOCATION}/../Format.cmake AND NOT SKIP_FORMAT)
    if (NOT TARGET fix-clang-format)
        add_subdirectory(${BuildEnvCMAKE_LOCATION}/../Format.cmake Format.cmake)
    endif()
endif()

macro(_PrintFlags)
    foreach (flagname
            CMAKE_C_FLAGS CMAKE_CXX_FLAGS
            CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS
            CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        foreach(variantstr _INIT
                    "               "
                    "_DEBUG         "
                    "_RELEASE       "
                    "_RELWITHDEBINFO"
                    "_MINSIZEREL    ")
            set(varname ${flagname}${variantstr})
            message(STATUS "${varname}:${${varname}}")
        endforeach()
    endforeach()

    foreach (varname
		    CMAKE_SYSTEM_PROCESSOR CMAKE_HOST_SYSTEM CMAKE_SYSTEM_NAME
		    CMAKE_CXX_COMPILER_ID CMAKE_C_COMPILER_ID
		    CMAKE_CXX_COMPILER CMAKE_C_COMPILER
		    CMAKE_CXX_COMPILER_AR CMAKE_C_COMPILER_AR
		    CMAKE_CXX_COMPILER_RANLIB CMAKE_C_COMPILER_RANLIB)
         message(STATUS "${varname}: ${${varname}}")
    endforeach()
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
    if (NOT "${_VALUE}" STREQUAL "${cflagsstr}")
        message(STATUS "${name}: \n\t<== ${_VALUE}\n\t==> ${cflagsstr}")
        set(${name} "${cflagsstr}" PARENT_SCOPE)
    endif()
endfunction()

macro(InitClangDIntellisense)
    file(GENERATE OUTPUT "${PROJECT_SOURCE_DIR}/.clangd"
        CONTENT "CompileFlags:\n\tCompilationDatabase: \"${CMAKE_CURRENT_BINARY_DIR}\"")
endmacro()

macro(EnableStrictCompilation)
    find_package(Threads)
    file(TIMESTAMP "${BuildEnvCMAKE_LOCATION}/BuildEnv.cmake" filetime)
    if ((NOT DEFINED STRICT_COMPILATION_MODE) OR (NOT "${STRICT_COMPILATION_MODE}" STREQUAL "${filetime}"))
        if (MINGW)
            # set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES   OFF)
            # set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

            # TODO : stencil : x64-mingw-static fails linking with the error
            # Disable LTO on mingw for now
            # Investigate if no-lto in vcpkg toolchain could be causing this

            # `_ZThn8_N5boost10wrapexceptINS_6system12system_errorEED1Ev' referenced in section `.rdata$_ZTVN5boost10wrapexceptINS_6system12system_errorEEE' of C:\Users\VSSADM~1\AppData\Local\Temp\cc6jZeRp.ltrans0.ltrans.o: defined in discarded section `.gnu.linkonce.t._ZN5boost10wrapexceptINS_6system12system_errorEED1Ev[_ZThn8_N5boost10wrapexceptINS_6system12system_errorEED1Ev]' of CodegenRuntime/CMakeFiles/codegen_runtime_tests.dir/Test_Interfaces.cpp.obj (symbol from plugin)
            # `_ZThn48_N5boost10wrapexceptINS_6system12system_errorEED0Ev' referenced in section `.rdata$_ZTVN5boost10wrapexceptINS_6system12system_errorEEE' of C:\Users\VSSADM~1\AppData\Local\Temp\cc6jZeRp.ltrans0.ltrans.o: defined in discarded section `.gnu.linkonce.t._ZN5boost10wrapexceptINS_6system12system_errorEED0Ev[_ZThn48_N5boost10wrapexceptINS_6system12system_errorEED0Ev]' of CodegenRuntime/CMakeFiles/codegen_runtime_tests.dir/Test_Interfaces.cpp.obj (symbol from plugin)
            # `_ZThn8_N5boost10wrapexceptISt12out_of_rangeED1Ev' referenced in section `.rdata$_ZTVN5boost10wrapexceptISt12out_of_rangeEE' of C:\Users\VSSADM~1\AppData\Local\Temp\cc6jZeRp.ltrans0.ltrans.o: defined in discarded section `.gnu.linkonce.t._ZN5boost10wrapexceptISt12out_of_rangeED1Ev[_ZThn8_N5boost10wrapexceptISt12out_of_rangeED1Ev]' of CodegenRuntime/CMakeFiles/codegen_runtime_tests.dir/Test_Interfaces.cpp.obj (symbol from plugin)
            # `_ZThn24_N5boost10wrapexceptISt12out_of_rangeED0Ev' referenced in section `.rdata$_ZTVN5boost10wrapexceptISt12out_of_rangeEE' of C:\Users\VSSADM~1\AppData\Local\Temp\cc6jZeRp.ltrans0.ltrans.o: defined in discarded section `.gnu.linkonce.t._ZN5boost10wrapexceptISt12out_of_rangeED0Ev[_ZThn24_N5boost10wrapexceptISt12out_of_rangeED0Ev]' of CodegenRuntime/CMakeFiles/codegen_runtime_tests.dir/Test_Interfaces.cpp.obj (symbol from plugin)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
        endif()
        if (WIN32)
            if (DEFINED ENV{INCLUDE})
                include_directories(SYSTEM $ENV{INCLUDE})
            endif()
        endif()
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(extraflags
                -external:W3 -external:anglebrackets -external:templates-
                -Wall   # Enable all errors
                -WX     # All warnings as errors
                # /await
                -permissive- # strict compilation
                -EHsc   # C++ Exceptions
                -DWIN32
                -D_WINDOWS
                -DNOMINMAX
                -DWIN32_LEAN_AND_MEAN
                -bigobj
                -guard:cf
                -Zc:__cplusplus

                #suppression list
                /wd4619  # pragma warning: there is no warning number
                /wd4514  # unreferenced inline function has been removed
                /wd4820  # bytes padding added after data member in struct
                /wd4868  # compiler may not enforce left-to-right evaluation order in braced initializer list
                /wd5039  #  pointer or reference to potentially throwing function passed to 'extern "C"' function under -EHc.
                /wd5045  # Spectre mitigation insertion
                /wd5264  # const variable is not used

                # TODO : Revisit these with newer VS Releases
                /wd4710  # Function not inlined. VS2019 CRT throws this
                /wd4711  # Function selected for automatic inline. VS2019 CRT throws this
                /wd4738  # storing 32-bit float result in memory, possible loss of performance 10.0.19041.0\ucrt\corecrt_math.h(642)
                /wd4746  # volatile access of 'b' is subject to /volatile:<iso|ms>
                # TODO : Revisit with later cmake release. This causes cmake autodetect HAVE_STRUCT_TIMESPEC to fail
                /wd4255  # The compiler did not find an explicit list of arguments to a function. This warning is for the C compiler only.
                /wd5246  # MSVC Bug VS2022 :  the initialization of a subobject should be wrapped in braces
            )

            set(exclusions "[-/]W[a-zA-Z1-9]+" "[-/]permissive?" "[-/]external:W?" "[-/]external:anglebrackets?" "[-/]external:templates?")
            link_libraries(WindowsApp.lib rpcrt4.lib onecoreuap.lib kernel32.lib)
            _FixFlags(CMAKE_C_FLAGS     EXCLUDE ${exclusions} APPEND ${extraflags})
            _FixFlags(CMAKE_CXX_FLAGS   EXCLUDE ${exclusions} APPEND ${extraflags})
            # /RTCc rejects code that conforms to the standard, it's not supported by the C++ Standard Library
            # RTCc Reports when a value is assigned to a smaller data type and results in a data loss.
            # RTCs Enables stack frame run-time error checking, as follows:
            # RTCu Reports when a variable is used without having been initialized
            _FixFlags(CMAKE_C_FLAGS_DEBUG APPEND /RTCsu)
            if (DEFINED VCPKG_TARGET_TRIPLET AND DEFINED VCPKG_ROOT AND NOT DEFINED VCPKG_CRT_LINKAGE)
                if (EXISTS "${VCPKG_ROOT}/triplets/${VCPKG_TARGET_TRIPLET}.cmake")
                    include("${VCPKG_ROOT}/triplets/${VCPKG_TARGET_TRIPLET}.cmake")
                elseif (EXISTS "${VCPKG_ROOT}/triplets/community/${VCPKG_TARGET_TRIPLET}.cmake")
                    include("${VCPKG_ROOT}/triplets/community/${VCPKG_TARGET_TRIPLET}.cmake")
                endif()
            endif()
            if (DEFINED VCPKG_CRT_LINKAGE)
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${VCPKG_CRT_LINKAGE},dynamic>:DLL>")
            endif()
        elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang) OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL GNU))
            set(extraflags
                # -fPIC via cmake CMAKE_POSITION_INDEPENDENT_CODE
                # -fvisibility=hidden Done via cmake CMAKE_CXX_VISIBILITY_PRESET
                -Wall   # Enable all errors
                -Wextra
                -pedantic
                -pedantic-errors
                -pthread
                # Remove unused code
                -ffunction-sections
                -fdata-sections
                -fexceptions
            )
            set(extracxxflags
                # -std=c++20 via CMAKE_CXX_STANDARD
                # -fvisibility-inlines-hidden via CMAKE_VISIBILITY_INLINES_HIDDEN
            )

            if (NOT EMSCRIPTEN)
                list(APPEND extraflags
                        -Wl,--exclude-libs,ALL
                        -Werror     # All warnings as errors
                        -Wl,--gc-sections
            )
            endif()

            if (EMSCRIPTEN)
                list(APPEND extraflags -pthread -Wno-limited-postlink-optimizations -sASYNCIFY)
                #TODO https://github.com/emscripten-core/emscripten/issues/16836
                list(APPEND extraflags -Wl,-u,htonl -Wl,-u,htons ) 
            endif()
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang)
                set(CMAKE_CXX_CLANG_TIDY clang-tidy -fix)
                list(APPEND extraflags
                    -fcxx-exceptions
                    -Weverything
                    -Wno-weak-vtables # Virtual Classes will actually be virtual
                    -Wno-return-std-move-in-c++11 # Rely on guaranteed copy ellisioning
                    -Wno-c++98-c++11-c++14-c++17-compat
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
                    -Wno-c++20-extensions
                    -Wno-c++98-compat-pedantic
                    -Wno-reserved-identifier # Allow names starting with underscore
                    -Wno-reserved-id-macro
                    -Wno-unsafe-buffer-usage
                    )
            else()
                list(APPEND extracxxflags -Wno-error=stringop-overflow)
            endif()

            if (MINGW OR WIN32)
                # list(APPEND extraflags -O1)
                # list(APPEND extraflags -Wa,-mbig-obj)
                # list(APPEND extraflags -mconsole  -Wl,-subsystem,console)
                list(APPEND extraflags -DWIN32=1 -D_WINDOWS=1 -DWIN32_LEAN_AND_MEAN=1)
                list(APPEND extracxxflags -DNOMINMAX=1)
            endif()

            list(APPEND extracxxflags
                #suppression list
                -Wno-ctad-maybe-unsupported
                -Wno-padded # Dont care about auto padding
            )

            set(exclusions "[-/]W[a-zA-Z1-9]+")
            _FixFlags(CMAKE_C_FLAGS     EXCLUDE ${exclusions} APPEND ${extraflags})
            _FixFlags(CMAKE_CXX_FLAGS   EXCLUDE ${exclusions} APPEND ${extraflags} ${extracxxflags})

            _FixFlags(CMAKE_CXX_FLAGS_RELEASE EXCLUDE "-O[^3]+" APPEND "-O3")
            _FixFlags(CMAKE_CXX_FLAGS_RELWITHDEBINFO EXCLUDE "-O[^3]+" APPEND "-O3")
            _FixFlags(CMAKE_C_FLAGS_RELEASE EXCLUDE "-O[^3]+" APPEND "-O3")
            _FixFlags(CMAKE_C_FLAGS_RELWITHDEBINFO EXCLUDE "-O[^3]+" APPEND "-O3")

            if (MINGW)
                # TODO GCC Bug: Compiling with -O1 can sometimes result errors
                # due to running out of string slots (file too big)
                _FixFlags(CMAKE_CXX_FLAGS_DEBUG EXCLUDE "-O." APPEND "-O1")
            endif()

            if (ANDROID)
                _FixFlags(CMAKE_CXX_LINK_OPTIONS_IPO EXCLUDE "-fuse-ld=gold")
                _FixFlags(CMAKE_C_LINK_OPTIONS_IPO EXCLUDE "-fuse-ld=gold")
            endif()
        else()
            _PrintFlags()
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "")
                message("Unidentified compiler")
            else()
                message(FATAL_ERROR "Unknown compiler ::${CMAKE_CXX_COMPILER_ID}::")
            endif()
        endif()
        _PrintFlags()
    endif()
    set(STRICT_COMPILATION_MODE "${filetime}")
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
    if (TARGET ${targetName})
        message(STATUS "Suppressing Warnings for ${targetName}")
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            target_compile_options(${targetName} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W3 /WX->)
        elseif((${CMAKE_CXX_COMPILER_ID} STREQUAL Clang) OR (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU))
            target_compile_options(${targetName} PRIVATE -Wno-error -w)
        else()
            message(FATAL_ERROR "Unknown compiler : ${CMAKE_CXX_COMPILER_ID}")
        endif()
    endif()
endmacro()

function(init_submodule path)
    cmake_parse_arguments("" "" "SUBMODULE_DIRECTORY" "" ${ARGN})
    set(srcdir "${CMAKE_CURRENT_SOURCE_DIR}")
    if (DEFINED _SUBMODULE_DIRECTORY)
        set(srcdir "${_SUBMODULE_DIRECTORY}")
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
