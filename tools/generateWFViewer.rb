
# Generate the FHiCL document which configures the lbne::WFViewer class

# Note that in the case of the "prescale" and "digital_sum_only"
# parameters, if one or both of these parameters are "nil", the
# function, their values will be taken from a separate file called
# WFViewer.fcl, searched for via "read_fcl"



require File.join( File.dirname(__FILE__), 'demo_utilities' )

def generateWFViewer(fragmentIDList, fragmentTypeList, prescale = nil, digital_sum_only = nil)

  wfViewerConfig = String.new( "\
    app: {
      module_type: RootApplication
      force_new: true
    }
    wf: {
      module_type: WFViewer
      fragment_ids: %{fragment_ids}
      fragment_type_labels: %{fragment_type_labels} " \
      + read_fcl("WFViewer.fcl") \
      + "    }" )


    # John F., 1/21/14 -- before sending FHiCL configurations to the
    # EventBuilderMain and AggregatorMain processes, construct the
    # strings listing fragment ids and fragment types which will be
    # used by the WFViewer

    # John F., 3/8/14 -- adding a feature whereby the fragments are
    # sorted in ascending order of ID

    fragmentIDListString, fragmentTypeListString = "[ ", "[ "

    typemap = Hash.new

    0.upto(fragmentIDList.length-1) do |i|
      typemap[ fragmentIDList[i]  ] = fragmentTypeList[i]
    end

    fragmentIDList.sort.each { |id| fragmentIDListString += " %d," % [ id ] }
    fragmentIDList.sort.each { |id| fragmentTypeListString += "%s," % [ typemap[ id ] ] }

    fragmentIDListString[-1], fragmentTypeListString[-1] = "]", "]" 

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

