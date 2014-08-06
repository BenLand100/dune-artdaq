
# Generate the FHiCL document which configures the lbne::TpcRceReceiver class


require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateSSP(startingFragmentId, boardId, fragmentType)

  sspConfig = String.new( "\
    generator: SSP
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    interface_type: 1
    " )

  
  sspConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  sspConfig.gsub!(/\%\{board_id\}/, String(boardId))
  sspConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  return sspConfig

end
