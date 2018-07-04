art_make( LIB_LIBRARIES ${LARRECO_LIB}
          ${LARDATA_LIB}
          ${ART_FRAMEWORK_CORE}
          ${ART_FRAMEWORK_PRINCIPAL}
          ${ART_FRAMEWORK_SERVICES_REGISTRY}
          ${ART_FRAMEWORK_SERVICES_OPTIONAL}
          ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
          ${ART_FRAMEWORK_SERVICES_SYSTEM_FILECATALOGMETADATA_SERVICE}
          art_Framework_IO_Root_RootDB
          art_Persistency_Common canvas
          art_Persistency_Provenance canvas
          art_Utilities canvas
          ${MF_MESSAGELOGGER}
          ${MF_UTILITIES}
          ${SQLITE3}
          ${FHICLCPP}
          cetlib cetlib_except
          ${ROOT_GEOM}
          ${ROOT_XMLIO}
          ${ROOT_GDML}
          ${ROOT_BASIC_LIB_LIST}
          ${BOOST_LIB}
          ${Boost_PROGRAM_OPTIONS_LIBRARY}
          ${Boost_FILESYSTEM_LIBRARY}
          ${Boost_SYSTEM_LIBRARY}
          )

art_make_exec( extract_dune_metadata
          SOURCE
          extract_dune_metadata.cc
          LIBRARIES
          ${ART_FRAMEWORK_CORE}
          art_Framework_IO_Root_RootDB
          ${ART_FRAMEWORK_IO_ROOT_ROOTDB}
          ${ART_PERSISTENCY_COMMON}
          ${ART_PERSISTENCY_ROOTDB}
          ${SQLITE3}
          ${FHICLCPP}
          ${CETLIB}
          ${ROOT_BASIC_LIB_LIST}
          ${BOOST_LIB}
          ${Boost_PROGRAM_OPTIONS_LIBRARY}
          )

simple_plugin( DuneMetadata "plugin"
          dune-artdaq_Metadata
          ${LARRECO_LIB}
          ${LARDATA_LIB}
          ${ART_FRAMEWORK_CORE}
          ${ART_FRAMEWORK_PRINCIPAL}
          ${ART_FRAMEWORK_SERVICES_REGISTRY}
          ${ART_FRAMEWORK_SERVICES_OPTIONAL}
          ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
          ${ART_FRAMEWORK_SERVICES_SYSTEM_FILECATALOGMETADATA_SERVICE}
          art_Persistency_Common canvas
          art_Persistency_Provenance canvas
          art_Utilities canvas
          ${MF_MESSAGELOGGER}
          ${MF_UTILITIES}
          ${FHICLCPP}
          cetlib cetlib_except
          ${ROOT_GEOM}
          ${ROOT_XMLIO}
          ${ROOT_GDML}
          ${ROOT_BASIC_LIB_LIST}
          ${BOOST_LIB}
          ${Boost_FILESYSTEM_LIBRARY}
          ${Boost_SYSTEM_LIBRARY}
          )

install_headers()
install_fhicl()
install_source()
install_scripts()

