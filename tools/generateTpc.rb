
# Generate the FHiCL document which configures the lbne::TpcRceReceiver class


require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateTpc(startingFragmentId, boardId, fragmentType)

  tpcConfig = String.new( "\
    generator: TpcRceReceiver
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    sleep_on_stop_us: 500000 
    " \
     + read_fcl("TpcRceReceiver.fcl") )
  
  tpcConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  tpcConfig.gsub!(/\%\{board_id\}/, String(boardId))
  tpcConfig.gsub!(/\%\{random_seed\}/, String(rand(10000))) 
  tpcConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  return tpcConfig

end
