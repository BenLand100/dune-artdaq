#!/bin/env ruby

require 'fileutils'

if ARGV.length != 2
  puts "Usage: " + __FILE__ + " <Fragment generator token> <Fragment token>" 
  exit 1
end

fraggentoken = ARGV[0]
fragtoken = ARGV[1]

if ! ENV['LBNEARTDAQ_REPO'] 
  puts "Couldn\'t find LBNEARTDAQ_REPO variable; have you sourced setupLBNEARTDAQ?"
  exit 1
end

basedir = ENV['LBNEARTDAQ_REPO'] 
gendir = basedir + "/lbne-artdaq/Generators/"
overlaydir = basedir + "/lbne-artdaq/Overlays/"

Dir.chdir basedir

sourcefiles = []

# Copy the Toy* source files to the source files for the new code

[".hh", "_generator.cc"].each do |ext|
  #puts gendir + "ToySimulator" + ext + " " + gendir + fraggentoken + ext
  FileUtils.cp( gendir + "ToySimulator" + ext , gendir + fraggentoken + ext )
  sourcefiles << gendir + fraggentoken + ext

end

[".hh", ".cc", "Writer.hh"].each do |ext|
  #puts overlaydir + "ToyFragment" + ext + " " + overlaydir + fragtoken + "Fragment" + ext
  FileUtils.cp( overlaydir + "ToyFragment" + ext , overlaydir + fragtoken + "Fragment" + ext )
  sourcefiles << overlaydir + fragtoken + "Fragment" + ext
end


# Search-and-replace the fragment and fragment generator names, using
# the tokens passed at the command line

sourcefiles.each do |sourcefile|

  sourcetext = File.read( sourcefile )
  sourcetext = sourcetext.gsub('ToySimulator', fraggentoken)
  sourcetext = sourcetext.gsub('Toy', fragtoken)

  outf = File.open( sourcefile, 'w')
  outf.puts sourcetext
end

# Add in the new generator to the CMakeLists.txt file for compilation

sourcetext = File.read( gendir + "CMakeLists.txt" )
sourcetext += "\n\n" 
sourcetext += "simple_plugin(" + fraggentoken + " \"generator\"\n"
sourcetext += <<-'EOS'
  lbne-artdaq_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP} 
  ${CETLIB} 
  )
EOS

outf = File.open( gendir + "CMakeLists.txt", 'w')
outf.puts sourcetext

