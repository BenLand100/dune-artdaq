
def generateAggregator(totalFRs, totalEBs, bunchSize, fragSizeWords,
                       xmlrpcClientList, fileSizeThreshold, fileDuration,
                       fileEventCount, queueDepth, queueTimeout, onmonEventPrescale)

agConfig = String.new( "\
daq: {
  max_fragment_size_words: %{size_words}
  aggregator: {
    mpi_buffer_count: %{buffer_count}
    first_event_builder_rank: %{total_frs}
    event_builder_count: %{total_ebs}
    expected_events_per_bunch: %{bunch_size}
    print_event_store_stats: true
    event_queue_depth: %{queue_depth}
    event_queue_wait_time: %{queue_timeout}
    onmon_event_prescale: %{onmon_event_prescale}
    xmlrpc_client_list: \"%{xmlrpc_client_list}\"
    file_size_MB: %{file_size}
    file_duration: %{file_duration}
    file_event_count: %{file_event_count}
  }
}" )

  agConfig.gsub!(/\%\{size_words\}/, String(fragSizeWords))
  agConfig.gsub!(/\%\{buffer_count\}/, String(totalEBs*4))
  agConfig.gsub!(/\%\{total_ebs\}/, String(totalEBs))
  agConfig.gsub!(/\%\{bunch_size\}/, String(bunchSize))  
  agConfig.gsub!(/\%\{queue_depth\}/, String(queueDepth))  
  agConfig.gsub!(/\%\{queue_timeout\}/, String(queueTimeout))  
  agConfig.gsub!(/\%\{onmon_event_prescale\}/, String(onmonEventPrescale))
  agConfig.gsub!(/\%\{xmlrpc_client_list\}/, String(xmlrpcClientList))
  agConfig.gsub!(/\%\{file_size\}/, String(fileSizeThreshold))
  agConfig.gsub!(/\%\{file_duration\}/, String(fileDuration))
  agConfig.gsub!(/\%\{file_event_count\}/, String(fileEventCount))

  return agConfig
end
