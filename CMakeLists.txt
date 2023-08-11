cmake_minimum_required(VERSION 3.19)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/BuildEnv.cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/Modules)

project(libadsb)

include(CTest)
include(GenerateExportHeader)
set(CMAKE_CXX_STANDARD 20)

EnableStrictCompilation()

option(libadsb_BUILD_TESTING "Build Tests" ON)
option(libadsb_PREFER_SYSTEM_LIB "Find packages and use system libs for dependencies" ON)

#if (libadsb_PREFER_SYSTEM_LIB)
find_package(LibUSB REQUIRED)
#endif()
find_package(LibRTLSDR REQUIRED)

if (DEFINED LIBUSB_LIBRARIES AND DEFINED LIBUSB_INCLUDE_DIRS)
    if (NOT TARGET usb-1.0)
        add_library(usb-1.0 INTERFACE)
        target_include_directories(usb-1.0 INTERFACE ${LIBUSB_INCLUDE_DIR})
        target_link_libraries(usb-1.0 INTERFACE ${LIBUSB_LIBRARIES})
    endif()
endif()
if (NOT TARGET usb-1.0)
    message(FATAL_ERROR "Libusb not found")
endif()
if (DEFINED LIBRTLSDR_LIBRARIES AND DEFINED LIBRTLSDR_INCLUDE_DIRS)
    if (NOT TARGET rtlsdr::rtlsdr)
        add_library(rtlsdr_static INTERFACE)
	target_include_directories(rtlsdr_static INTERFACE ${LIBRTLSDR_INCLUDE_DIR})
	target_link_libraries(rtlsdr_static INTERFACE ${LIBRTLSDR_LIBRARIES})
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

set(dump978_ROOT dump978)
add_library(dump978 STATIC
    ${dump978_ROOT}/uat_decode.c
    ${dump978_ROOT}/dump978.c
    ${dump978_ROOT}/fec.c
    ${dump978_ROOT}/fec/decode_rs_char.c
    ${dump978_ROOT}/fec/init_rs_char.c
    uat2json-wrapper.cpp
)
SupressWarningForTarget(dump978)
target_compile_definitions(dump978 PRIVATE main=dump978_main1 _USE_MATH_DEFINES)
if (MSVC)
    target_compile_options(dump978 PRIVATE /wd4242 /wd4334 /wd4013 /wd4996 /wd4701)
endif()
target_link_libraries(dump978 PRIVATE rtlsdr::rtlsdr)

add_library(adsb STATIC
    ADSB1090.h
    ADSBListener.h
    AircraftImpl.h
    CommonMacros.h
    SetThreadName.h
    UAT978.h

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
endif()
