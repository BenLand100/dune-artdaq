simple_plugin(RootApplication "module")
simple_plugin(WFViewer "module" lbne-raw-data_Overlays ${ROOT_GPAD} ${ROOT_HIST} ${ROOT_GUI})
simple_plugin(TriggerAna "module" lbne-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)