# ======================================================================
#  darkart main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/dune-artdaq/ups/setup_for_development
#  buildtool
# ======================================================================


# use cmake 2.8 or later
cmake_minimum_required (VERSION 2.8)

project(dune-artdaq)

# cetbuildtools contains our cmake modules
SET ( CETBUILDTOOLS_VERSION $ENV{CETBUILDTOOLS_VERSION} )
IF (NOT CETBUILDTOOLS_VERSION)
    MESSAGE (FATAL_ERROR "ERROR: setup cetbuildtools to get the cmake modules")
ENDIF()

if( DEFINED ENV{CANVAS_DIR} )
set(CMAKE_MODULE_PATH
  $ENV{ART_DIR}/Modules
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


# JCF, Aug-19-2017

# Since the inhibit functionality requires 0MQ, leave ZEROMQ_IGNORED
# set to "FALSE"

set (ZEROMQ_IGNORED "TRUE")

if (NOT ZEROMQ_IGNORED) 
message(STATUS "Requiring 0MQ libraries be available for build" )

cet_set_compiler_flags(DIAGS VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic
 EXTRA_CXX_FLAGS -D_GLIBCXX_USE_NANOSLEEP -Wno-ignored-qualifiers
 )
else()
message(STATUS "Not requiring 0MQ libraries be available for build" )

cet_set_compiler_flags(DIAGS VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic
 EXTRA_CXX_FLAGS -D_GLIBCXX_USE_NANOSLEEP -DZEROMQ_IGNORED -Wno-ignored-qualifiers
 )
endif()

cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_ups_product( cetbuildtools v5_06_06 )
find_ups_product( artdaq v2_01_00 )
find_ups_product( artdaq_utilities v1_00_00)
find_ups_product( artdaq_core v1_06_01 )
find_ups_product( dune_raw_data v0_00_02 )
find_ups_product( art v2_03_00 )
find_ups_product( fhiclcpp v2_16_00 )
find_ups_product( messagefacility v1_10_00 )
find_ups_product( cetlib v1_03_00 )
find_ups_root( v5_34_00 )
find_ups_boost( v1_50_0 filesystem
                        program_options
                        unit_test_framework
 )
find_ups_product( libxml2 v2_9_1a )
find_ups_product( TRACE v3_07_00 )
find_ups_product( protodune_pdt_core v0_1_0 )

# JM-A 2017/06/23 ###
find_ups_product( uhal v2_4_2 ) 
include_directories($ENV{UHAL_INC}/uhal)
find_library( UHAL_UHAL NAMES cactus_uhal_uhal PATHS $ENV{UHAL_LIB})
find_library( UHAL_LOG  NAMES cactus_uhal_log  PATHS $ENV{UHAL_LIB})
find_library( PDT_CORE  NAMES protodune_pdt_core PATHS $ENV{PROTODUNE_PDT_CORE_LIB})
#####################

# have to make sure cmake doesn't select the system libraries
# define libsigc
cet_find_library( SIGC    NAMES sigc-2.0  PATHS ENV LIBSIGCPP_LIB NO_DEFAULT_PATH )


# macros for art_dictionary and simple_plugin
include(ArtDictionary)
include(ArtMake)
include(BuildPlugins)
include(CetTest)

# source
add_subdirectory(dune-artdaq)

# testing
add_subdirectory(test)

# tools
add_subdirectory(tools)

# ups - table and config files
add_subdirectory(ups)

# packaging utility
include(UseCPack)
