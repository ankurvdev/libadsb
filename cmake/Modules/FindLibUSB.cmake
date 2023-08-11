# - Try to find LibUSB-1.0
# Once done this will define
#
#  LIBUSB_FOUND - System has libusb
#  LIBUSB_INCLUDE_DIRS - The libusb include directories
#  LIBUSB_LIBRARIES - The libraries needed to use libusb
#  LIBUSB_DEFINITIONS - Compiler switches required for using libusb
#  LIBUSB_VERSION - the libusb version
#

find_package(PkgConfig)
pkg_check_modules(PC_LibUSB QUIET libusb-1.0)
set(LIBUSB_DEFINITIONS ${PC_LibUSB_CFLAGS_OTHER})

find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h
          HINTS ${PC_LibUSB_INCLUDE_DIRS}
          PATH_SUFFIXES libusb-1.0
          PATHS
          /usr/include
          /usr/local/include )

#standard library name for libusb-1.0
set(libusb1_library_names usb-1.0)

#libusb-1.0 compatible library on freebsd
if((CMAKE_SYSTEM_NAME STREQUAL "FreeBSD") OR (CMAKE_SYSTEM_NAME STREQUAL "kFreeBSD"))
    list(APPEND libusb1_library_names usb)
endif()

#libusb-1.0 name on Windows (from PothosSDR distribution)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND libusb1_library_names libusb-1.0)
endif()

find_library(LIBUSB_LIBRARY
             NAMES ${libusb1_library_names}
             HINTS ${PC_LibUSB_LIBRARY_DIRS}
             PATHS
             /usr/lib
             /usr/local/lib )

set(LIBUSB_VERSION ${PC_LibUSB_VERSION})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LibUSB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibUSB
	                          REQUIRED_VARS LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR
				  VERSION_VAR LIBUSB_VERSION)

mark_as_advanced(LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR LIBUSB_VERSION)

set(LIBUSB_LIBRARIES ${LIBUSB_LIBRARY} )
set(LIBUSB_INCLUDE_DIRS ${LIBUSB_INCLUDE_DIR} )
