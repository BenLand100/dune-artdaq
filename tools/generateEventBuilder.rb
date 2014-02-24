
def generateEventBuilder( fragSizeWords, totalFRs, totalAGs, totalFragments, verbose)

ebConfig = String.new( "\
daq: {
  max_fragment_size_words: %{size_words}
  event_builder: {
    mpi_buffer_count: %{buffer_count}
    first_fragment_receiver_rank: 0
    fragment_receiver_count: %{total_frs}
    expected_fragments_per_event: %{total_fragments}
    use_art: true
    print_event_store_stats: true
    verbose: %{verbose}
  }
} "
)

  ebConfig.gsub!(/\%\{size_words\}/, String(fragSizeWords))
  ebConfig.gsub!(/\%\{buffer_count\}/, String(totalFRs*8))
  ebConfig.gsub!(/\%\{total_frs\}/, String(totalFRs))
  ebConfig.gsub!(/\%\{total_fragments\}/, String(totalFragments))
  ebConfig.gsub!(/\%\{verbose\}/, String(verbose))

  return ebConfig

end

