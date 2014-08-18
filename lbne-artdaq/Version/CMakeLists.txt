# project name
project(lbne-artdaq_Version)

# the utcDateTime variable is used by the GetReleaseVersion code
# to report the build datetime
execute_process( COMMAND "date" "-u" "+%d-%b-%Y %H:%M:%S %Z" OUTPUT_VARIABLE rawUTCDate )
string( STRIP ${rawUTCDate} utcDateTime )

configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/GetReleaseVersion.cc.in
  ${CMAKE_CURRENT_BINARY_DIR}/GetReleaseVersion.cc @ONLY )

art_make_library( LIBRARY_NAME lbne-artdaq_Version
                  SOURCE ${CMAKE_CURRENT_BINARY_DIR}/GetReleaseVersion.cc
                 )

install_headers()
install_source()
