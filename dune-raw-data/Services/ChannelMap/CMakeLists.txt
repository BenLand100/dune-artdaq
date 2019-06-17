
# Jingbo Wang, John Freeman, with mods from Tom Junk
# 2017

install_fhicl()
install_headers()
install_source()


set(${product}_file_dir "${product}/${version}/files")
file(GLOB channel_map_file *ChannelMap*.txt)
install(FILES ${channel_map_file} DESTINATION ${${product}_file_dir})

# Add plugin for each service

if( DEFINED ENV{CANVAS_DIR} )
  set( persistency_lib art_Utilities canvas)
else()
  set( persistency_lib art_Utilities)
endif()

simple_plugin(PdspChannelMapService "service"
              ${ART_PERSISTENCY_ROOTDB}
              ${ART_FRAMEWORK_SERVICES_REGISTRY}
              ${persistency_lib}
              ${SQLITE3}
              ${MF_MESSAGELOGGER}
              ${MF_UTILITIES}
              ${CETLIB_LIBS}
              ${ROOT_BASIC_LIB_LIST}
)

