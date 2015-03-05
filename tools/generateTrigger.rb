
def generateTrigger( )

  triggerOutput = String.new( "SelectEvents: { SelectEvents: [ ssploose,ssptight,random ] }")

  triggerConfig = String.new( "\
  filters: {
    ssploosehypo:{
      module_type: SSPTrigger
      SspModuleLabel: daq
      CutOnNTriggers: true
      MinNTriggers: 1
      Verbose: false
    }
    ssplooseprescale:{
      module_type: PreScaleTrigger
      PreScale: 10
      UseRndmPreScale: false
    }

    ssptighthypo:{
      module_type: SSPTrigger
      SspModuleLabel: daq
      CutOnNTriggers: true
      MinNTriggers: 2
      Verbose: false
    }
    ssptightprescale:{
      module_type: PreScaleTrigger
      PreScale: 1
      UseRndmPreScale: false
    }

    prescale100:{
      module_type: PreScaleTrigger
      PreScale: 100
      UseRndmPreScale: true
    }
  }

  producers: {

  }
  ")

  triggerPath = String.new( "\
  ssploose:   [ssploosehypo,ssplooseprescale]
  ssptight:   [ssptighthypo,ssptightprescale]
  random:     [prescale100]
  ")

  return triggerOutput, triggerConfig, triggerPath

end

