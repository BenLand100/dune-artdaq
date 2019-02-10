#ifndef FRAMEFILE_H
#define FRAMEFILE_H

#include <fstream>
#include <iostream>
#include <memory> // for unique_ptr
#include <array>
#include "dune-raw-data/Overlays/FelixFormat.hh"

// Quoth Milo (stitched together from a Slack DM on  2018-11-21):
//
// The file contains the 50 first fragments in run 5863. A
// fragment contains a readout window for a given fibre. In the
// run that this data is from, that means it contains 15024 frames
// coming from that same fibre, corresponding to a readout window
// of 7.5 ms. [The file] contains the same data volume as 5 APA
// readouts. The fragments are not strictly ordered within a root
// file, if I remember correctly, so you might have 6 fragments
// for one fibre and 4 for another. I just took 50 fragments
// because that way i could be quite sure that all fibres are
// represented in the data. The first 15024 are from one fibre and
// are in chronological order. Every timestamp within the fragment
// data should be 25 more than the one in the previous frame. If
// you notice any deviation in this or the identifier fields
// (slot, crate, fiber numbers), you know that the data is
// corrupt.  The same should be the case for subsequent sets of
// 15024 frames.

class FrameFile
{
public:
    // We could try to learn this value by parsing the file, but meh
    static constexpr size_t frames_per_fragment=15024;
    static constexpr size_t buffer_size=frames_per_fragment*sizeof(dune::FelixFrame);

    FrameFile(const char* filename)
        : m_file(filename, std::ifstream::binary),
          m_buffer(new char[buffer_size])
    {
        if(m_file.bad() || m_file.fail() || !m_file.is_open()){
            throw std::runtime_error(std::string("Bad file ")+std::string(filename));
        }
        // Calculate the length of the file
        m_file.seekg(0, m_file.end);
        m_length = m_file.tellg();
        m_file.seekg(0, m_file.beg);
        if(m_length==0){
            throw std::runtime_error("Empty file");
        }
    }

    ~FrameFile()
    {
        m_file.close();
        delete[] m_buffer;
    }

    // Length of the file in bytes
    size_t length() const {return m_length;}
    // Number of fragments in the file. One fragment is an entire
    // readout window for a given fibre, with size frames_per_fragment
    // frames
    size_t num_fragments() const {return m_length/buffer_size;}

    // Read the ith fragment into the buffer and return a pointer to
    // the first frame in the fragment. Subsequent calls will
    // overwrite the buffer with a different fragment
    dune::FelixFrame* fragment(size_t i)
    {
        if(i>=num_fragments()) return nullptr;
        // Seek to the right place in the file
        m_file.seekg(i*frames_per_fragment*sizeof(dune::FelixFrame));
        // Check we didn't go past the end
        if(m_file.bad() || m_file.eof()) return nullptr;
        // Actually read the fragment into the buffer
        m_file.read(m_buffer,buffer_size);
        if(m_file.bad() || m_file.eof()) return nullptr;
        return reinterpret_cast<dune::FelixFrame*>(m_buffer);
    }

protected:
    std::ifstream m_file;
    size_t m_length;
    char* m_buffer;
};


#endif
