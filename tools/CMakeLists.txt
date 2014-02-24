cet_script(setupDemoEnvironment.sh DemoControl.rb start2x2System.sh manage2x2System.sh start2x2x2System.sh manage2x2x2System.sh CreateNewFragmentGenerator.rb  generateEventBuilderMain.rb  generateWFViewer.rb generateEventBuilder.rb demo_utilities.rb generateToy.rb generateAggregator.rb generateAggregatorMain.rb generateBoardReaderMain.rb RunDriver.rb)


art_make_exec(driver
  LIBRARIES
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQRATE}
  ${ARTDAQ_UTILITIES}
  ${ART_FRAMEWORK_ART}
  ${FHICLCPP}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${MPI_C_LIBRARIES}
  )
  

# Is this necessary?
#install_source()
