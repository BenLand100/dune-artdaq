art_make_exec(NAME gallerytest
    SOURCE
    gallerytest.cc
  LIBRARIES
  artdaq-core_Data
  ${CANVAS}
  ${ROOT_CORE}
  ${CETLIB}
  ${GALLERY}
)
