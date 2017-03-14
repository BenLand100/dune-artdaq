art_make_exec(NAME gallerytest
    SOURCE
    gallerytest.cc
  LIBRARIES
  artdaq-core_Data
  canvas_Utilities
  canvas_Persistency_Common
  canvas_Persistency_Provenance
  ${ROOT_CORE}
  ${CETLIB}
  ${GALLERY}
)
