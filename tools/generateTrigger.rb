
def generateTrigger( ebIndex, dataDir )

  triggerOutput = String.new( "\
  # sspOutput : {
  #   module_type: RootOutput
  #   fileName: \"%{output_file}\"
  #   compressionLevel: 0
  #   SelectEvents: { SelectEvents: [ ssppath ] }
  # }
  SelectEvents: { SelectEvents: [ ssppath ] }
  ")

  triggerConfig = String.new( "\
  filters: {
    ssptrigger:{
      module_type: SSPTrigger
      SspModuleLabel: daq
      CutOnNTriggers: true
      MinNTriggers: 2
      Verbose: true
    }
  }

  producers: {

  }
  ")

  triggerPath = String.new( "\
  ssppath:   [ssptrigger]
  # sspoutput: [sspOutput]
  ")

  sspFileName = "lbne_ssp_eb%02d_" % ebIndex
  sspFileName += "r%06r_sr%02s_%to"
  sspFileName += ".root"
  sspOutputFile = File.join(dataDir, sspFileName)
  triggerOutput.gsub!(/\%\{output_file\}/, sspOutputFile)

  return triggerOutput, triggerConfig, triggerPath

end

