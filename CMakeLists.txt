cmake_minimum_required(VERSION 3.19)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/BuildEnv.cmake)
project(libadsb)

include(CTest)
include(GenerateExportHeader)
set(CMAKE_CXX_STANDARD 26)

EnableStrictCompilation()
InitClangDIntellisense()
include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/fmt.cmake)

option(libadsb_BUILD_TESTING "Build Tests" ON)
option(libadsb_PREFER_SYSTEM_LIB "Find packages and use system libs for dependencies" ON)

if (libadsb_PREFER_SYSTEM_LIB)
find_package(libusb CONFIG)
endif()

if (DEFINED LIBUSB_LIBRARIES AND DEFINED LIBUSB_INCLUDE_DIRS)
    if (NOT TARGET usb-1.0)
        add_library(usb-1.0 INTERFACE)
        target_include_directories(usb-1.0 INTERFACE ${LIBUSB_INCLUDE_DIR})
        target_link_libraries(usb-1.0 INTERFACE ${LIBUSB_LIBRARIES})
    endif()
endif()
if (NOT TARGET usb-1.0)
    init_submodule(libusb-cmake)
    set(libusb_BUILD_TESTING OFF CACHE BOOL "")
    set(libusb_BUILD_SHARED_LIBS OFF CACHE BOOL "libusb build static libs")
    set(LIBUSB_ROOT ${CMAKE_CURRENT_LIST_DIR}/libusb CACHE PATH "Libusb Root")
    add_subdirectory(libusb-cmake)
    SupressWarningForTarget(usb-1.0)
endif()
if (NOT TARGET usb-1.0)
    message(FATAL_ERROR "Libusb not found")
endif()
if (NOT TARGET rtlsdr::rtlsdr)
    if (libadsb_PREFER_SYSTEM_LIB)
        find_package(rtlsdr QUIET)
    endif()
    if (NOT rtlsdr_FOUND)
        init_submodule(rtl-sdr SUBDMODULE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
        set(rtlsdr_ROOT rtl-sdr/src)
        add_library(rtlsdr_static STATIC
            ${rtlsdr_ROOT}/librtlsdr.c
            ${rtlsdr_ROOT}/tuner_e4k.c
            ${rtlsdr_ROOT}/tuner_fc0012.c
            ${rtlsdr_ROOT}/tuner_fc0013.c
            ${rtlsdr_ROOT}/tuner_fc2580.c
            ${rtlsdr_ROOT}/tuner_r82xx.c)
        target_include_directories(rtlsdr_static PRIVATE ${rtlsdr_ROOT})
        target_compile_definitions(rtlsdr_static PUBLIC rtlsdr_STATIC)
        target_link_libraries(rtlsdr_static PUBLIC usb-1.0)
        target_include_directories(rtlsdr_static PUBLIC rtl-sdr/include)
        SupressWarningForTarget(rtlsdr_static)
        add_library(rtlsdr::rtlsdr ALIAS rtlsdr_static)
    endif()
endif()

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)

init_submodule(dump978 SUBDMODULE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
init_submodule(dump1090 SUBDMODULE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

file(GLOB dump1090sources dump1090/*.c)

set(excludes
    interactive.c #curses
    sdr_limesdr.c #device not needed
    sdr_bladerf.c #device not needed
    sdr_hackrf.c #device not needed
    dump1090.c # contains main
    view1090.c # contains main
    cprtests.c # tests
    faup1090.c # contains main
    net_io.c   # highjacking callbacks
)
foreach(ex ${excludes})
    list(FILTER dump1090sources EXCLUDE REGEX "dump1090/${ex}")
endforeach()

check_include_files("unistd.h" HAVE_UNISTD_H)

set(dump978_ROOT dump978)
add_library(dump978 STATIC
    ${dump978_ROOT}/legacy/uat_decode.c
    ${dump978_ROOT}/legacy/dump978.c
    ${dump978_ROOT}/legacy/fec.c
    ${dump978_ROOT}/libs/fec/decode_rs_char.c
    ${dump978_ROOT}/libs/fec/init_rs_char.c
    uat2json-wrapper.cpp
)
target_include_directories(dump978 PRIVATE ${dump978_ROOT}/libs .)
set_source_files_properties(${dump978_ROOT}/dump978.c PROPERTIES SKIP_LINTING ON)
if (HAVE_UNISTD_H)
    target_compile_definitions(dump978 PRIVATE HAVE_UNISTD_H=1)
endif()

SupressWarningForTarget(dump978)
target_compile_definitions(dump978 PRIVATE main=dump978_main1 _USE_MATH_DEFINES)
if (MSVC)
    target_compile_options(dump978 PRIVATE /wd4242 /wd4334 /wd4013 /wd4996 /wd4701)
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL Clang)
    target_compile_options(dump978 PRIVATE -Wno-implicit-function-declaration)
endif()
target_link_libraries(dump978 PRIVATE rtlsdr::rtlsdr)

add_library(adsb STATIC
    ADSB1090.h
    ADSBListener.h
    AircraftImpl.h
    CommonMacros.h
    SetThreadName.h
    UAT978.cpp
    ADSB1090.cpp
    ADSBListener.cpp
    )

target_link_libraries(adsb PUBLIC dump978 rtlsdr::rtlsdr Threads::Threads)
target_include_directories(adsb PUBLIC .)

if (libadsb_BUILD_TESTING AND BUILD_TESTING)
    add_executable(testadsb main.cpp)
    target_link_libraries(testadsb PRIVATE adsb)

    if (DEFINED ANDROID)
        add_library(testadsb_app SHARED android_main.cpp)
        target_link_libraries(testadsb_app PRIVATE adsb usb-1.0)
        generate_export_header(testadsb_app)
        target_include_directories(testadsb_app PRIVATE ${PROJECT_BINARY_DIR})
    endif()

    include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/EmbedResource.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/Catch2.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/dtl.cmake)

    add_executable(unittest_1090 tests/test_1090.cpp)
    file(DOWNLOAD https://github.com/antirez/dump1090/raw/refs/heads/master/testfiles/modes1.bin ${CMAKE_CURRENT_BINARY_DIR}/modes1.bin)
    find_package(EmbedResource REQUIRED)
    target_add_resource(unittest_1090 RESOURCE_COLLECTION_NAME traces RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/modes1.bin")
    target_include_directories(unittest_1090 PRIVATE .)
    target_link_libraries(unittest_1090 PRIVATE Catch2::Catch2WithMain dtl::dtl fmt::fmt adsb)
    target_compile_definitions(unittest_1090 PRIVATE HAVE_EMBEDRESOURCE=1)
    add_test(NAME unittest_1090 COMMAND unittest_1090)
endif()
