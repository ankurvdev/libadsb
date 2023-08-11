# - Try to find LibRTLSDR
# Once done this will define
#
#  LIBRTLSDR_FOUND - System has librtlsdr
#  LIBRTLSDR_INCLUDE_DIRS - The librtlsdr include directories
#  LIBRTLSDR_LIBRARIES - The libraries needed to use librtlsdr
#  LIBRTLSDR_DEFINITIONS - Compiler switches required for using librtlsdr
#  LIBRTLSDR_VERSION - The librtlsdr version
#

find_package(PkgConfig)
pkg_check_modules(PC_LibRTLSDR QUIET librtlsdr)
set(LIBRTLSDR_DEFINITIONS ${PC_LibRTLSDR_CFLAGS_OTHER})

find_path(LIBRTLSDR_INCLUDE_DIR NAMES rtl-sdr.h
          HINTS ${PC_LibRTLSDR_INCLUDE_DIRS}
          PATHS
          /usr/include
          /usr/local/include )

find_library(LIBRTLSDR_LIBRARY NAMES rtlsdr
             HINTS ${PC_LibRTLSDR_LIBRARY_DIRS}
             PATHS
             /usr/lib
             /usr/local/lib )

set(LIBRTLSDR_VERSION ${PC_LibRTLSDR_VERSION})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LibRTLSDR_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibRTLSDR
	                          REQUIRED_VARS LIBRTLSDR_LIBRARY LIBRTLSDR_INCLUDE_DIR
				  VERSION_VAR LIBRTLSDR_VERSION)

mark_as_advanced(LIBRTLSDR_LIBRARY LIBRTLSDR_INCLUDE_DIR LIBRTLSDR_VERSION)

set(LIBRTLSDR_LIBRARIES ${LIBRTLSDR_LIBRARY} )
set(LIBRTLSDR_INCLUDE_DIRS ${LIBRTLSDR_INCLUDE_DIR} )
