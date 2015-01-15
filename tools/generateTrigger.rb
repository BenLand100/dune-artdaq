
def generateTrigger( )

triggerConfig = String.new( "\
  filters: {
    exampletrigger:{
      # module_type: PreScaleTrigger
      # PreScale:0
      # UseRndmPreScale: true
      module_type: SSPTrigger
      SspModuleLabel: daq
      CutOnNTriggers: true
      MinNTriggers: 2
      Verbose: true
    }
  }

  producers: {

  }

  triggerpath: [exampletrigger]

"
)

  return triggerConfig

end

