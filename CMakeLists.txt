cmake_minimum_required(VERSION 3.19)
project(libadsb)
set(CMAKE_CXX_STANDARD 20)
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

add_library(dump1090 OBJECT ${dump1090sources})
target_include_directories(dump1090 PRIVATE dump1090 dump1090/dsp/generated)
target_compile_definitions(dump1090 PRIVATE STARCH_MIX_X86)
add_library(adsb OBJECT ADSBListener.h ADSBListener.cpp dump1090-wrapper.c)
target_include_directories(adsb PRIVATE dump1090 dump1090/dsp/generated)

add_executable(test main.cpp)
find_package(Threads)
add_subdirectory(libusb/cmake)
add_subdirectory(rtl-sdr)
target_link_libraries(test PRIVATE adsb dump1090 rtlsdr Threads::Threads)
target_include_directories(test PRIVATE "${hdr}")
