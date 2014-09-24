#!/bin/env ruby

# John F., 2/4/14

# This script is designed to make copies of the toy fragment generator
# / overlay source files, using user-selected tokens passed at the
# command line to replace the variable names, and then adding info
# about the source files to the CMakeLists.txt build script

require 'fileutils'

puts "JCF, 7/25/14: now that packages have been split up, this script is deprecated until further notice"
exit 0

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
  FileUtils.cp( gendir + "ToySimulator" + ext , gendir + fraggentoken + ext )
  sourcefiles << gendir + fraggentoken + ext

end

[".hh", ".cc", "Writer.hh"].each do |ext|
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
  ${ARTDAQ-CORE_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP} 
  ${CETLIB} 
  )
EOS

outf = File.open( gendir + "CMakeLists.txt", 'w')
outf.puts sourcetext

# Announce what's been done, and describe the next steps to the users

puts
puts "The following files have been created: "
sourcefiles.each { |sourcefile| puts "%s" % [sourcefile] }

puts
puts "The following file has been modified: "
puts gendir + "CMakeLists.txt"

instructions = <<-'EOS'
In order to use your new fragment generator, please recompile
lbne-artdaq. However, before doing so, consider (A) editing the source
code, in particular changing the fragment types generated by your new
fragment generator (currently still TOY1 and TOY2), and (B) creating a
copy of lbne-artdaq/tools/fcl/driver.fcl which will use your fragment
generator and not the ToySimulator, and then testing it via "driver -c
<yourfile>"
EOS

puts instructions
