# This function will generate the FHiCL code used to control the
# EventBuilderMain application by configuring its
# artdaq::EventBuilderCore object

require File.join( File.dirname(__FILE__), 'generateEventBuilder' )
require File.join( File.dirname(__FILE__), 'generateTrigger' )


def generateEventBuilderMain(ebIndex, totalFRs, totalEBs, totalAGs, 
                         dataDir, onmonEnable, triggerEnable,
                         diskWritingEnable, fragSizeWords, totalFragments,
                         fclWFViewer )
  # Do the substitutions in the event builder configuration given the options
  # that were passed in from the command line.  

  ebConfig = String.new( "\

services: {
  scheduler: {
    fileMode: NOMERGE
  }
  user: {
    NetMonTransportServiceInterface: {
      service_provider: NetMonTransportService
      first_data_receiver_rank: %{ag_rank}
      mpi_buffer_count: %{netmonout_buffer_count}
      max_fragment_size_words: %{size_words}
      data_receiver_count: 1 # %{ag_count}
      #broadcast_sends: true
    }
  }
  Timing: { summaryOnly: true }
  #SimpleMemoryCheck: { }
}

%{event_builder_code}

outputs: {
  %{netmon_output}rootMPIOutput: {
  %{netmon_output}  module_type: RootMPIOutput
  %{netmon_output}%{trigger_output}
  %{netmon_output}}
  %{root_output}normalOutput: {
  %{root_output}  module_type: RootOutput
  %{root_output}  fileName: \"%{output_file}\"
  %{root_output}  compressionLevel: 0
  %{root_output}}
  
}

physics: {
  analyzers: {
%{phys_anal_onmon_cfg}
  }
  
  %{trigger_code}
  
  %{enable_onmon}a1: [ app, wf ]

  %{netmon_output}my_output_modules: [ rootMPIOutput ]
  %{root_output}my_output_modules: [ normalOutput ]
  %{trigger_path}
}
source: {
  module_type: RawInput
  waiting_time: 900
  resume_after_timeout: true
  fragment_type_map: [[1, \"missed\"], [2, \"TPC\"], [3, \"PHOTON\"], [4, \"TRIGGER\"], [5, \"TOY1\"], [6, \"TOY2\"]]
}
process_name: DAQ" )

verbose = "true"

if Integer(totalAGs) >= 1
  verbose = "false"
end


event_builder_code = generateEventBuilder( fragSizeWords, totalFRs, totalAGs, totalFragments, verbose)

ebConfig.gsub!(/\%\{event_builder_code\}/, event_builder_code)

if Integer(triggerEnable) != 0
  trigger_output,trigger_code,trigger_path =  generateTrigger()
else
  trigger_output = ""
  trigger_code   = ""
  trigger_path   = ""
end
ebConfig.gsub!(/\%\{trigger_output\}/, trigger_output)
ebConfig.gsub!(/\%\{trigger_code\}/,   trigger_code)
ebConfig.gsub!(/\%\{trigger_path\}/,   trigger_path)
# puts ebConfig
# raise "Fail"

ebConfig.gsub!(/\%\{ag_rank\}/, String(totalFRs + totalEBs))
ebConfig.gsub!(/\%\{ag_count\}/, String(totalAGs))
ebConfig.gsub!(/\%\{size_words\}/, String(fragSizeWords))
ebConfig.gsub!(/\%\{netmonout_buffer_count\}/, String(totalAGs*4))

if Integer(totalAGs) >= 1
  ebConfig.gsub!(/\%\{netmon_output\}/, "")
  ebConfig.gsub!(/\%\{root_output\}/, "#")
  ebConfig.gsub!(/\%\{enable_onmon\}/, "#")
  ebConfig.gsub!(/\%\{phys_anal_onmon_cfg\}/, "")
else
  ebConfig.gsub!(/\%\{netmon_output\}/, "#")
  if Integer(diskWritingEnable) != 0
    ebConfig.gsub!(/\%\{root_output\}/, "")
  else
    ebConfig.gsub!(/\%\{root_output\}/, "#")
  end
  if Integer(onmonEnable) != 0
    ebConfig.gsub!(/\%\{phys_anal_onmon_cfg\}/, fclWFViewer )
    ebConfig.gsub!(/\%\{enable_onmon\}/, "")
  else
    ebConfig.gsub!(/\%\{phys_anal_onmon_cfg\}/, "")
    ebConfig.gsub!(/\%\{enable_onmon\}/, "#")
  end
end


currentTime = Time.now
fileName = "lbne_eb%02d_" % ebIndex
fileName += "r%06r_sr%02s_%to"
fileName += ".root"
outputFile = File.join(dataDir, fileName)
ebConfig.gsub!(/\%\{output_file\}/, outputFile)

return ebConfig

end


