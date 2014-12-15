
def generateTrigger( )

triggerConfig = String.new( "\
  filters: {
    exampletrigger:{
      module_type: PreScaleTrigger
      PreScale:0
      UseRndmPreScale: true
    }
  }

  producers: {

  }

  triggerpath: [exampletrigger]

"
)

  return triggerConfig

end

