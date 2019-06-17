# ======================================================================
#  darkart main build file
#
#  cd .../path/to/build/directory
#  source ../path/to/dune-raw-data/ups/setup_for_development
#  buildtool
# ======================================================================


# use cmake 2.8 or later
cmake_minimum_required (VERSION 2.8)

project(dune_raw_data)

# cetbuildtools contains our cmake modules
SET ( CETBUILDTOOLS_VERSION $ENV{CETBUILDTOOLS_VERSION} )
IF (NOT CETBUILDTOOLS_VERSION)
    MESSAGE (FATAL_ERROR "ERROR: setup cetbuildtools to get the cmake modules")
ENDIF()

if( DEFINED ENV{CANVAS_DIR} )
set(CMAKE_MODULE_PATH 
	$ENV{CANVAS_DIR}/Modules
	 $ENV{CETBUILDTOOLS_DIR}/Modules
	  ${CMAKE_MODULE_PATH})
else()
set(CMAKE_MODULE_PATH
        $ENV{ART_DIR}/Modules
	$ENV{CETBUILDTOOLS_DIR}/Modules
	${CMAKE_MODULE_PATH})
endif()

include(CetCMakeEnv)
cet_cmake_env()


if( DEFINED ENV{CANVAS_DIR} )
cet_set_compiler_flags(DIAGS VIGILANT
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic
  EXTRA_DEFINITIONS -DHAVE_CANVAS -D_GLIBCXX_USE_NANOSLEEP
)
else()
cet_set_compiler_flags(DIAGS VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic
 EXTRA_CXX_FLAGS -D_GLIBCXX_USE_NANOSLEEP
 )
endif()

cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_ups_product( cetbuildtools v5_06_06 )

# JCF, May-9-2019
# Removing ups product minimums for the artdaq packages so that
# dev-versioning can be used on ProtoDUNE
find_ups_product( artdaq_core ) # v1_06_01 )
find_ups_product( art v2_03_00 )
find_ups_product( cetlib v1_03_00 )
find_ups_boost( v1_50_0 filesystem
                        program_options
                        unit_test_framework
 )
find_ups_product( TRACE v3_07_00 )
# have to make sure cmake doesn't select the system libraries
# define libsigc
cet_find_library( SIGC    NAMES sigc-2.0  PATHS ENV LIBSIGCPP_LIB NO_DEFAULT_PATH )

# macros for art_dictionary and simple_plugin
include(ArtDictionary)
include(ArtMake)
include(BuildPlugins)
include(CetTest)

# source
add_subdirectory(dune-raw-data)

# testing
add_subdirectory(test)

# # tools
# add_subdirectory(tools)

# ups - table and config files
add_subdirectory(ups)

# packaging utility
include(UseCPack)
