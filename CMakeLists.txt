# ======================================================================
#  darkart main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/dune-artdaq/ups/setup_for_development
#  buildtool
# ======================================================================


cmake_minimum_required (VERSION 3.0)

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

# JCF, May-9-2019
# Removing ups product minimums for the artdaq packages so that
# dev-versioning can be used on ProtoDUNE
find_ups_product( artdaq ) # v2_01_00 )
find_ups_product( artdaq_utilities ) # v1_00_00)
find_ups_product( artdaq_core ) # v1_06_01 )
find_ups_product( dune_raw_data ) # v0_00_02 )
find_ups_product( art v2_03_00 )
find_ups_product( fhiclcpp v2_16_00 )
find_ups_product( messagefacility v1_10_00 )
find_ups_product( cetlib v1_03_00 )
find_ups_product( netio v0_8_0 )
find_ups_product( ftd2xx v1_2_7 )
# add a newer version of ZMQ for PTMP
find_ups_product( zmq v4_3_1 )
find_ups_product( czmq v4_2_0 -q e15)
# add protobuf for PTMP
find_ups_product( protobuf v3_3_1a )
find_ups_product( ptmp v0_3_0 )
find_ups_product( ptmp_tcs v0_0_3 )
find_ups_product( jsoncpp v1_8_0 )
find_ups_root( v5_34_00 )
find_ups_boost( v1_50_0 filesystem
                        program_options
                        unit_test_framework
 )
find_ups_product( libxml2 v2_9_1a )
find_ups_product( TRACE v3_07_00 )
find_ups_product( protodune_timing v4_1_3 )
find_ups_product( zmq v4_3_1)
find_ups_product( czmq v4_2_0)
find_ups_product( protobuf v3_3_1a)
find_ups_product( protodune_felix_deps v0_0_1 )


find_ups_product( dune_artdaq_InhibitMaster v0_01_00 )

set(netio_INCLUDE_DIR $ENV{NETIO_INC})
set(netio_LIBRARY_DIR $ENV{NETIO_LIB})
set(ftd2xx_INCLUDE_DIR $ENV{FTD2XX_INC})
set(ftd2xx_LIBRARY_DIR $ENV{FTD2XX_LIB})

# JM-A 2017/06/23 ###
find_ups_product( uhal v2_6_0 ) 
include_directories($ENV{UHAL_INC}/uhal)
find_library( UHAL_UHAL NAMES cactus_uhal_uhal PATHS $ENV{UHAL_LIB})
find_library( UHAL_LOG  NAMES cactus_uhal_log  PATHS $ENV{UHAL_LIB})
find_library( PDT_CORE  NAMES protodune_pdt_core PATHS $ENV{PROTODUNE_TIMING_LIB})
find_library( PROTOBUF  NAMES protobuf      PATHS $ENV{PROTOBUF_LIB})
find_library( LIBPTMP_TCS NAMES ptmp-tcs PATHS $ENV{PTMP_TCS_LIB})
find_library( LIBPTMP NAMES ptmp PATHS $ENV{PTMP_LIB})
find_library( LIBZMQ NAMES zmq PATHS $ENV{ZMQ_LIB} NO_DEFAULT_PATH)
find_library( LIBCZMQ NAMES czmq PATHS $ENV{CZMQ_LIB} NO_DEFAULT_PATH)

set(PTMP_LIBRARIES ${LIBPTMP} ${LIBZMQ} ${LIBCZMQ} ${PROTOBUF} dl)
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

# JCF, Jul-5-2019

# Logic behind the grep statement in the string of commands is that
# the DAQInterface version in common use on the ProtoDUNE cluster will
# be the newest version of the form
# v<int>_<int><int>_<int><int><letter> - e.g., v3_00_06v

if ( NOT DEFINED ENV{HOSTNAME} )

MESSAGE(FATAL_ERROR "You need to have the HOSTNAME environment variable defined for the build to work")

elseif ( $ENV{HOSTNAME} MATCHES "^np04-srv-.*" )

execute_process(COMMAND bash "-c" "daqinterface_version=$( \
ls /nfs/sw/artdaq/products/artdaq_daqinterface | grep -E \"^v[0-9]_[0-9]{2}_[0-9]{2}.$\" | sort -n | tail -1 ) ; \
echo DAQInterface version used to save commit info is $daqinterface_version ; \
. /nfs/sw/artdaq/products/setup; \
setup artdaq_daqinterface $daqinterface_version; \
python $ARTDAQ_DAQINTERFACE_DIR/rc/control/utilities.py get_commit_info dune-artdaq $MRB_SOURCE/dune_artdaq $MRB_INSTALL/.." )
endif()
