
# Generate the FHiCL document which configures the lbne::PennReceiver class


require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generatePenn(startingFragmentId, boardId, fragmentType)

  fclFileName = "PennReceiver" + String(boardId) + ".fcl"
  puts "***generatePenn boardId %d fclFileName %s" % [boardId, fclFileName]
 
  pennConfig = String.new( "\
    generator: PennReceiver
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    sleep_on_stop_us: 500000 
    " \
     + read_fcl(fclFileName) )
  
  pennConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  pennConfig.gsub!(/\%\{board_id\}/, String(boardId))
  #pennConfig.gsub!(/\%\{random_seed\}/, String(rand(10000))) 
  pennConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  return pennConfig

end
