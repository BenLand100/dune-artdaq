
# Generate the FHiCL document which configures the lbne::ToySimulator class

# Note that if "nADCcounts" is set to nil, its FHiCL
# setting is defined in a separate file called ToySimulator.fcl,
# searched for via "read_fcl"

require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateToy(startingFragmentId, boardId, fragmentsPerBoard, 
                fragmentType, throttleUsecs = nil, nADCcounts = nil)

  toyConfig = String.new( "\
    generator: ToySimulator
    fragment_type: %{fragment_type}
    fragments_per_board: %{fragments_per_board}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    random_seed: %{random_seed}
    sleep_on_stop_us: 500000 " \
                          + read_fcl("ToySimulator.fcl") )
  
  toyConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  toyConfig.gsub!(/\%\{fragments_per_board\}/, String(fragmentsPerBoard))
  toyConfig.gsub!(/\%\{board_id\}/, String(boardId))
  toyConfig.gsub!(/\%\{random_seed\}/, String(rand(10000))) 
  toyConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  if ! nADCcounts.nil?
    toyConfig.gsub!(/.*nADCcounts.*\:.*/, "nADCcounts: %d" % [nADCcounts])
  end

  if ! throttleUsecs.nil?
    toyConfig.gsub!(/.*throttle_usecs.*\:.*/, "throttle_usecs: %d" % [throttleUsecs])
  end

  return toyConfig

end
