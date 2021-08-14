cmake_minimum_required(VERSION 3.19)
project(libadsb)
include(./BuildEnv.cmake)
include(CTest)

option(libadsb_BUILD_TESTING "Build Tests" ON)

if (NOT TARGET rtlsdr::rtlsdr_static)
    find_package(rtlsdr QUIET)
    if (NOT rtlsdr_FOUND)
        find_package(libusb QUIET)
        if (NOT libusb_FOUND)
            init_submodule(libusb)
            set(libusb_BUILD_TESTING OFF CACHE BOOL "")
            add_subdirectory(libusb/cmake)
            SupressWarningForTarget(usb-1.0)
        endif()
        init_submodule(rtl-sdr)
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
        add_library(rtlsdr::rtlsdr_static ALIAS rtlsdr_static)
    endif()
endif()
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)

init_submodule(dump978)

if (FALSE)
    init_submodule(dump1090)
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
endif()

set(dump978_ROOT dump978)
add_library(dump978 STATIC
    ${dump978_ROOT}/uat_decode.c
    ${dump978_ROOT}/dump978.c
    ${dump978_ROOT}/fec.c
    ${dump978_ROOT}/fec/decode_rs_char.c
    ${dump978_ROOT}/fec/init_rs_char.c
)
SupressWarningForTarget(dump978)
target_compile_definitions(dump978 PRIVATE main=dump978_main1 _USE_MATH_DEFINES)
if (MSVC)
    target_compile_options(dump978 PRIVATE /wd4242 /wd4334 /wd4013 /wd4996 /wd4701)
endif()
target_link_libraries(dump978 PRIVATE rtlsdr::rtlsdr_static)

add_library(adsb STATIC ADSBListener.h ADSBListener.cpp ADSB1090.h uat2json-wrapper.cpp)
target_link_libraries(adsb PUBLIC dump978 rtlsdr::rtlsdr_static Threads::Threads)
target_include_directories(adsb PUBLIC .)

if (libadsb_BUILD_TESTING AND BUILD_TESTING)
    add_executable(testadsb main.cpp)
    target_link_libraries(testadsb PRIVATE adsb)
endif()