# This function will generate the FHiCL code used to control the
# AggregatorMain application by configuring its
# artdaq::AggregatorCore object

require File.join( File.dirname(__FILE__), 'generateAggregator' )

def generateAggregatorMain(dataDir, runNumber, totalFRs, totalEBs, bunchSize,
                           onmonEnable,
                           diskWritingEnable, agIndex, totalAGs, fragSizeWords,
                           xmlrpcClientList, fileSizeThreshold, fileDuration,
                           fileEventCount, fclWFViewer, onmonEventPrescale)

agConfig = String.new( "\
services: {
  scheduler: {
    fileMode: NOMERGE
    errorOnFailureToPut: true
  }
  user: {
    NetMonTransportServiceInterface: {
      service_provider: NetMonTransportService
      max_fragment_size_words: %{size_words}
    }
  }
  Timing: { summaryOnly: true }
}

%{aggregator_code}

source: {
  module_type: NetMonInput
}
outputs: {
  %{root_output}normalOutput: {
  %{root_output}  module_type: RootOutput
  %{root_output}  fileName: \"%{output_file}\"
  %{root_output}    fileSwitch: {
  %{root_output}      boundary: Run
  %{root_output}      force: true
  %{root_output}    }
  %{root_output}}
}
physics: {
  analyzers: {
%{phys_anal_onmon_cfg}
  }

  producers: {

    duneArtdaqBuildInfo: {
    module_type: DuneArtdaqBuildInfo
    }
  }

  p: [ duneArtdaqBuildInfo ]

  %{enable_onmon}a1: %{onmon_modules}

  %{root_output}my_output_modules: [ normalOutput ]
}
process_name: DAQAG"
)

  queueDepth, queueTimeout = -999, -999

  if agIndex == 0
    if totalAGs > 1
      onmonEnable = 0
    end
    queueDepth = 20
    queueTimeout = 5
    aggDescriptionString = "is_data_logger: true"
  else
    diskWritingEnable = 0
    queueDepth = 2
    queueTimeout = 1
    aggDescriptionString = "is_dispatcher: true"
  end

  aggregator_code = generateAggregator( totalFRs, totalEBs, bunchSize, fragSizeWords,
                                        xmlrpcClientList, fileSizeThreshold, fileDuration,
                                        fileEventCount, queueDepth, queueTimeout, onmonEventPrescale, aggDescriptionString )
  agConfig.gsub!(/\%\{aggregator_code\}/, aggregator_code)

  puts "Initial aggregator " + String(agIndex) + " disk writing setting = " +
  String(diskWritingEnable)
  # Do the substitutions in the aggregator configuration given the options
  # that were passed in from the command line.  Assure that files written out
  # by each AG are unique by including a timestamp in the file name.
  currentTime = Time.now
  fileName = "dune_"
  fileName += "r%06r_sr%02s_%to"
  fileName += ".root"
  outputFile = File.join(dataDir, fileName)

  agConfig.gsub!(/\%\{output_file\}/, outputFile)
  agConfig.gsub!(/\%\{total_frs\}/, String(totalFRs))
  agConfig.gsub!(/\%\{size_words\}/, String(fragSizeWords))

  agConfig.gsub!(/\%\{onmon_modules\}/, String(ONMON_MODULES))

  puts "agIndex = %d, totalAGs = %d, onmonEnable = %d" % [agIndex, totalAGs, onmonEnable]

  puts "Final aggregator " + String(agIndex) + " disk writing setting = " +
  String(diskWritingEnable)
  if Integer(diskWritingEnable) != 0
    agConfig.gsub!(/\%\{root_output\}/, "")
  else
    agConfig.gsub!(/\%\{root_output\}/, "#")
  end
  if Integer(onmonEnable) != 0
    agConfig.gsub!(/\%\{phys_anal_onmon_cfg\}/, fclWFViewer )
    agConfig.gsub!(/\%\{enable_onmon\}/, "")
  else
    agConfig.gsub!(/\%\{phys_anal_onmon_cfg\}/, "")
    agConfig.gsub!(/\%\{enable_onmon\}/, "#")
  end

  return agConfig  
end
