
# Generate the FHiCL document which configures the lbne::WFViewer class

# Note that in the case of the "prescale" and "digital_sum_only"
# parameters, if one or both of these parameters are "nil", the
# function, their values will be taken from a separate file called
# WFViewer.fcl, searched for via "read_fcl"



require File.join( File.dirname(__FILE__), 'demo_utilities' )

def generateWFViewer(totalFRs, fragmentsPerBoard, fragmentIDList, fragmentTypeList, prescale = nil, digital_sum_only = nil)

  wfViewerConfig = String.new( "\
    app: {
      module_type: RootApplication
      force_new: true
    }
    wf: {
      module_type: WFViewer
      fragments_per_board: %{fragments_per_board}
      fragment_receiver_count: %{total_frs}
      fragment_ids: %{fragment_ids}
      fragment_type_labels: %{fragment_type_labels} " \
      + read_fcl("WFViewer.fcl") \
      + "    }" )


    # John F., 1/21/14 -- before sending FHiCL configurations to the
    # EventBuilderMain and AggregatorMain processes, construct the
    # strings listing fragment ids and fragment types which will be
    # used by the WFViewer

    fragmentIDListString, fragmentTypeListString = "[ ", "[ "

    fragmentIDList.each { |id| fragmentIDListString += " %d," % [ id ] }
    fragmentTypeList.each { |type| fragmentTypeListString += "%s," % [type ] }

    fragmentIDListString[-1], fragmentTypeListString[-1] = "]", "]" 

  wfViewerConfig.gsub!(/\%\{total_frs\}/, String(totalFRs))
  wfViewerConfig.gsub!(/\%\{fragments_per_board\}/, String(fragmentsPerBoard))
  wfViewerConfig.gsub!(/\%\{fragment_ids\}/, String(fragmentIDListString))
  wfViewerConfig.gsub!(/\%\{fragment_type_labels\}/, String(fragmentTypeListString))

if ! prescale.nil?
  wfViewerConfig.gsub!(/.*prescale.*\:.*/, "prescale: %d" % [prescale])
end

if ! digital_sum_only.nil?
  wfViewerConfig.gsub!(/.*digital_sum_only.*\:.*/, "digital_sum_only: %s" % 
                       [String(digital_sum_only) ] )
end

  return wfViewerConfig
end

