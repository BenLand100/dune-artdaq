// FelixCompress.hh
// Milo Vermeulen 2018
// Compression studies in software, primarily focussing on speed.

#ifndef artdaq_dune_Overlays_FelixComp_hh
#define artdaq_dune_Overlays_FelixComp_hh

// Option for previous value subtraction.
#define PREV

#include <bitset>  // testing
#include <cmath>   // log
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono>

#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixFormat.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"

namespace dune {

// A Huffman tree is literally a bunch of nodes that are connected in a certain
// way and may or may not have values assigned to them.
struct HuffTree {
  struct Node {
    Node* left = NULL;
    Node* right = NULL;
    uint16_t value = 0;
    size_t huffcode = 0;
    uint8_t hufflength = 0;
    unsigned frequency = 0;
    bool hasParent = false;

    void operator=(const Node& other) {
      if (this != &other) {
        left = other.left;
        right = other.right;
        value = other.value;
        huffcode = other.huffcode;
        huffcode = other.hufflength;
        frequency = other.frequency;
        hasParent = other.hasParent;
      }
    }

    // Operator< overloading for ordering by frequency. If the frequency is the
    // same, the values in the nodes are taken into account.
    bool operator<(const Node& other) const {
      return (frequency < other.frequency) ||
             ((frequency == other.frequency) && (value < other.value));
    }
  };

  bool valuecomp(const Node& one, const Node& other) const {
    return one.value < other.value;
  }

  Node* root;
  Node* nodelist;
  std::unordered_map<uint16_t, Node*> nodes;

  // Function to get a node from a value.
  Node* operator()(const uint16_t value) { return nodes[value]; }

  // Function to print all leaves from bottom to top.
  void print(Node* loc) {
    if (loc->left == NULL) {
      std::cout << "Node with value " << loc->value << ", frequency "
                << loc->frequency << " and huffcode "
                << std::bitset<32>(loc->huffcode) << " length "
                << (unsigned)loc->hufflength << '\n';
    } else {
      print(loc->left);
      print(loc->right);
    }
    return;
  }
  void print() { return print(root); }

  // Function to print a Huffman tree to a file.
  void printFile(std::string& filename) {
    std::ofstream ofile(filename);

    ofile << "Value\tFrequency\tCode\tLength\n";
    std::vector<Node*> local_nlist;

    for(auto p : nodes) {
      Node* n = p.second;
      if(n->frequency == 0) continue;
      local_nlist.push_back(n);
    }

    std::sort(local_nlist.begin(), local_nlist.end());

    for(unsigned i = 0; i < local_nlist.size(); ++i) {
      Node* n = local_nlist[i];
      if(n->value > (1<<16)/2) {
        ofile << (int)n->value-(1<<16) << '\t' << n->frequency << '\t' << n->huffcode << '\t' << (unsigned)n->hufflength << '\n';
      } else {
        if(n->value > 1000) continue; // Disregard start values.
        ofile << n->value << '\t' << n->frequency << '\t' << n->huffcode << '\t' << (unsigned)n->hufflength << '\n';
      }
    }

    ofile.close();
  }

  // Function to generate codes for a Huffman tree.
  void generate_codes(Node* loc, size_t buff = 0, uint8_t len = 0) {
    // Assign a string when a leaf is reached. Otherwise go one layer deeper.
    if (loc->left == NULL) {
      loc->huffcode = buff;
      loc->hufflength = len;
      nodes[loc->value] = loc;
    } else {
      // Move one node down.
      generate_codes(loc->left, buff, len + 1);
      generate_codes(loc->right, buff | (1 << len), len + 1);
    }

    // Move one node up.
    return;
  }
  void generate_codes() { return generate_codes(root); }

  // Function to create a tree from a buffer of nodes.
  std::unordered_map<uint16_t, Node*> make_tree(std::vector<Node> nodevec) {
    // Order vector to get the same results every time.
    std::sort(nodevec.begin(), nodevec.end());
    // Create a new buffer to hold the leaves and N-1 new nodes.
    const unsigned totlen = 2 * nodevec.size() - 1;
    Node* begin = &nodevec[0];
    nodelist = new Node[totlen];
    std::copy(begin, begin + nodevec.size(), nodelist);

    // Continue until the buffer is filled.
    for (unsigned i = 0; i < totlen - nodevec.size(); ++i) {
      // Find the lowest-frequency non-parented nodes.
      Node* lowest = nodelist;
      while (lowest->hasParent) {
        ++lowest;
      }
      Node* sec_lowest = lowest + 1;
      while (sec_lowest->hasParent) {
        ++sec_lowest;
      }
      for (Node* n = nodelist; n != nodelist + nodevec.size() + i; ++n) {
        // Disregard parented nodes.
        if (n->hasParent) {
          continue;
        }
        if (n->frequency < lowest->frequency) {
          sec_lowest = lowest;
          lowest = n;
        } else if (n->frequency < sec_lowest->frequency && n != lowest) {
          sec_lowest = n;
        }
      }

      // Make lowest value the left node in case of equal frequency.
      if (lowest->frequency == sec_lowest->frequency &&
          lowest->value > sec_lowest->value) {
        Node* tmp = lowest;
        lowest = sec_lowest;
        sec_lowest = tmp;
      }

      // Link the lowest frequency nodes to a new one in the buffer.
      Node* newNode = &nodelist[nodevec.size() + i];
      newNode->left = lowest;
      newNode->right = sec_lowest;
      newNode->frequency = lowest->frequency + sec_lowest->frequency;
      lowest->hasParent = true;
      sec_lowest->hasParent = true;
    }

    // Assign root to the last unparented node and generate codes.
    root = &nodelist[totlen - 1];
    generate_codes();

    // Output the generated map.
    return nodes;
  }

  ~HuffTree() { delete[] nodelist; }
};

struct MetaData {
  uint32_t comp_method : 2, unique_values : 14, num_frames : 16;
};

//========================
// FELIX compressor class
//========================
class FelixCompressor {
 private:
  // Input and output buffers.
  const void* input;
  const size_t input_length;
  const size_t num_frames = input_length / sizeof(FelixFrame);

  // Frame access to data.
  FelixFrame const* frame_(const size_t& frame_num = 0) const {
    return static_cast<FelixFrame const*>(input) + frame_num;
  }

  // Huffman tree storage for easy access.
  HuffTree hufftree;

 public:
  FelixCompressor(const uint8_t* data, const size_t num_frames = 10000)
      : input(data), input_length(num_frames * sizeof(dune::FelixFrame)) {}
  FelixCompressor(const dune::FelixFragment& frag)
      : input(frag.dataBeginBytes()), input_length(frag.dataSizeBytes()) {}

  // Function to store metadata in a recognisable format.
  void store_metadata(std::vector<char>& out) {
    // Clear the vector as a first action to make sure there is a clean slate to
    // work with.
    out.clear();

    MetaData meta = {1, 0, (uint32_t)num_frames};
    out.resize(sizeof(meta));
    memcpy(&out[0], &meta, sizeof(meta));
  }

  // Function to reduce headers and error fields upon error encounter.
  void header_reduce(std::vector<char>& out) {
    // Fields for storing which frames have faulty headers.
    std::vector<uint8_t> bad_headers(num_frames/8 + 1);
    // Check all error fields and increments of timestamp and CCC.
    for (unsigned i = 1; i < num_frames; ++i) {
      bool check_failed = false;

      // WIB header checks.
      check_failed |= frame_()->sof() ^ frame_(i)->sof();
      check_failed |= frame_()->version() ^ frame_(i)->version();
      check_failed |= frame_()->fiber_no() ^ frame_(i)->fiber_no();
      check_failed |= frame_()->crate_no() ^ frame_(i)->crate_no();
      check_failed |= frame_()->slot_no() ^ frame_(i)->slot_no();
      check_failed |= frame_()->mm() ^ frame_(i)->mm();
      check_failed |= frame_()->oos() ^ frame_(i)->oos();
      check_failed |= frame_()->wib_errors() ^ frame_(i)->wib_errors();
      check_failed |= frame_()->z() ^ frame_(i)->z();

      check_failed |=
          (uint64_t)(frame_()->timestamp() + 25 * i) ^ frame_(i)->timestamp();

      // COLDATA header checks.
      for (unsigned j = 0; j < 4; ++j) {
        check_failed |= frame_()->s1_error(j) ^ frame_(i)->s1_error(j);
        check_failed |= frame_()->s2_error(j) ^ frame_(i)->s2_error(j);
        check_failed |= frame_()->checksum_a(j) ^ frame_(i)->checksum_a(j);
        check_failed |= frame_()->checksum_b(j) ^ frame_(i)->checksum_b(j);
        check_failed |=
            frame_()->error_register(j) ^ frame_(i)->error_register(j);
        for (unsigned h = 0; h < 8; ++h) {
          check_failed |= frame_()->hdr(j, h) ^ frame_(i)->hdr(j, h);
        }

        check_failed |=
            (uint16_t)(frame_()->coldata_convert_count(j) + 25 * i) ^
            frame_(i)->coldata_convert_count(j);
      }

      // Set a bit if anything failed.
      if (check_failed) {
        bad_headers[i/8] |= 1 << (i%8);

        // std::cout << "Check failed at the " << i << "th frame.\n";

        // std::cout << "First frame:\n";
        // frame_(i - 1)->print();
        // std::cout << "Second frame:\n";
        // frame_(i)->print();

        // exit(1);
      }
    }  // Loop over all frames.

    // Record faulty frame field.
    size_t tail = out.size();
    out.resize(tail + num_frames/8+1);
    memcpy(&out[tail], &bad_headers[0], bad_headers.size());

    // Record first headers.
    tail = out.size();
    out.resize(tail + sizeof(WIBHeader) + 4*sizeof(ColdataHeader));

    memcpy(&out[tail], frame_()->wib_header(), sizeof(WIBHeader));
    memcpy(&out[tail + sizeof(WIBHeader)], frame_()->coldata_header(0),
           sizeof(ColdataHeader));
    memcpy(&out[tail + sizeof(WIBHeader) + sizeof(ColdataHeader)], frame_()->coldata_header(1),
           sizeof(ColdataHeader));
    memcpy(&out[tail + sizeof(WIBHeader) + 2*sizeof(ColdataHeader)], frame_()->coldata_header(2),
           sizeof(ColdataHeader));
    memcpy(&out[tail + sizeof(WIBHeader) + 3*sizeof(ColdataHeader)], frame_()->coldata_header(3),
           sizeof(ColdataHeader));

    // Loop over frames again to record faulty headers.
    for(unsigned i = 1; i < num_frames; ++i) {
      bool check_failed = (bad_headers[i / 8] >> (7 - (i % 8))) & 1;
      if(!check_failed) { continue; }

      tail = out.size();
      out.resize(tail + sizeof(WIBHeader) + 4*sizeof(ColdataHeader));

      memcpy(&out[tail], frame_(i)->wib_header(), sizeof(WIBHeader));
      for(unsigned j = 0; j < 4; ++j) {
        memcpy(&out[tail + sizeof(WIBHeader)], frame_(i)->coldata_header(j),
              sizeof(ColdataHeader));
      }
    }
  }

  // Function to generate a Huffman table and tree.
  void generate_Huff_tree(std::vector<char>& out) {
    // Build a frequency table.
    std::unordered_map<adc_t, uint32_t> freq_table;
    for (unsigned vi = 0; vi < frame_()->num_ch_per_frame; ++vi) {
      freq_table[frame_(0)->channel(vi)]++;
      for (unsigned fri = 1; fri < num_frames; ++fri) {
#ifdef PREV
        adc_t curr_val = frame_(fri)->channel(vi) - frame_(fri - 1)->channel(vi);
#else
        adc_t curr_val = frame_(fri)->channel(vi);
#endif
        freq_table[curr_val]++;
      }
    }

    // Save the number of unique values to the metadata.
    MetaData* meta = reinterpret_cast<MetaData*>(&out[0]);
    meta->unique_values = freq_table.size();

    // Save the frequency table to the outgoing buffer so that the decompressor
    // can make a Huffman tree of its own.
    size_t tail = out.size();
    out.resize(tail + freq_table.size() * (sizeof(adc_t) + 4));
    for (auto p : freq_table) {
      memcpy(&out[tail], &p.first, sizeof(p.first));
      memcpy(&out[tail + sizeof(p.first)], &p.second, sizeof(p.second));
      tail += sizeof(p.first) + sizeof(p.second);
    }

    // Insert the frequency table into Huffman tree nodes and calculate the
    // information entropy according to Shannon.
    std::vector<HuffTree::Node> nodes;
    double entropy = 0;
    const unsigned num_vals = num_frames * frame_()->num_ch_per_frame;
    for (auto p : freq_table) {
      HuffTree::Node curr_node;
      curr_node.value = p.first;
      curr_node.frequency = p.second;
      nodes.push_back(curr_node);

      entropy += (double)p.second / num_vals * log((double)num_vals / p.second);
    }
    // std::cout << "ADC value entropy: " << entropy << " bits.\n";

    // Connect the nodes in the tree according to Huffman's method.
    hufftree.make_tree(nodes);
  }

  // Function to write ADC values using the Huffman table.
  void ADC_compress(std::vector<char>& out) {
    // Resize the output buffer appropriately for the worst case.
    const size_t tail = out.size();
    out.resize(tail + sizeof(adc_t) * num_frames * 256);
    // Record the encoded ADC values into the buffer.
    unsigned rec_bits = 0;
    const char* dest = &out[0] + tail;

    for(unsigned j = 0; j < 256; ++j) {
      adc_t curr_val = frame_(0)->channel(j);
      *(unsigned long*)(dest + (rec_bits/8)) |= hufftree(curr_val)->huffcode << (rec_bits%8);
      rec_bits += hufftree(curr_val)->hufflength;

      for (unsigned i = 1; i < num_frames; ++i) {
#ifdef PREV
        adc_t curr_val = frame_(i)->channel(j) - frame_(i - 1)->channel(j);
#else
        adc_t curr_val = frame_(i)->channel(j);
#endif

        // No code is longer than 64 bits.
        *(uint64_t*)(dest + (rec_bits/8)) |= hufftree(curr_val)->huffcode << (rec_bits%8);
        rec_bits += hufftree(curr_val)->hufflength;
      }
    }
    // Resize output buffer to actual data length.
    out.resize(tail + rec_bits / 8 + 1);
  }

  // Function that calls all others relevant for compression.
  void compress_copy(std::vector<char>& out) {
    // auto comp_begin = std::chrono::high_resolution_clock::now();
    store_metadata(out);
    // auto meta_end = std::chrono::high_resolution_clock::now();
    // std::cout << "Meta time taken: "
    //           << std::chrono::duration_cast<std::chrono::microseconds>(
    //                  meta_end - comp_begin)
    //                  .count()
    //           << " us.\n";
    header_reduce(out);
    // auto header_end = std::chrono::high_resolution_clock::now();
    // std::cout << "Header time taken: "
    //           << std::chrono::duration_cast<std::chrono::microseconds>(
    //                  header_end - meta_end)
    //                  .count()
    //           << " us.\n";
    generate_Huff_tree(out);
    // auto huff_end = std::chrono::high_resolution_clock::now();
    // std::cout << "Huffman time taken: "
    //           << std::chrono::duration_cast<std::chrono::microseconds>(
    //                  huff_end - header_end)
    //                  .count()
    //           << " us.\n";
    ADC_compress(out);
    // auto adc_end = std::chrono::high_resolution_clock::now();
    // std::cout << "ADC time taken: "
    //           << std::chrono::duration_cast<std::chrono::microseconds>(
    //                  adc_end - huff_end)
    //                  .count()
    //           << " us.\n";
  }
};  // FelixCompressor

// Single function for compressing data from a fragment.
std::vector<char> FelixCompress(const dune::FelixFragment& frag) {
  FelixCompressor compressor(frag);
  std::vector<char> result;
  compressor.compress_copy(result);

  return result;
}

// Similarly, a function for decompressing data and returning an
// artdaq::Fragment.
artdaq::Fragment FelixDecompress(const std::vector<char>& buff) {
  artdaq::Fragment result;
  const char* src = buff.data();

  // Read metadata and apply to fragment.
  const MetaData* meta = reinterpret_cast<MetaData const*>(src);
  size_t num_frames = meta->num_frames;
  uint16_t unique_values = meta->unique_values;
  result.resizeBytes(num_frames * sizeof(FelixFrame));
  src += sizeof(MetaData);

  // Handle for filling fragment data.
  FelixFrame* frame = reinterpret_cast<FelixFrame*>(result.dataBeginBytes());

  // Access error frames field.
  std::vector<uint8_t> bad_headers(num_frames/8+1);
  memcpy(&bad_headers[0], src, num_frames/8+1);
  src += num_frames / 8 + 1;

  // Access saved headers and generate headers.
  unsigned sizeof_header_set = sizeof(WIBHeader) + 4 * sizeof(ColdataHeader);
  const WIBHeader* whead = reinterpret_cast<WIBHeader const*>(src);
  std::vector<const ColdataHeader*> chead(4);
  for(unsigned i = 0; i < 4; ++i) {
    chead[i] = reinterpret_cast<ColdataHeader const*>(
        src + sizeof(WIBHeader) + i * sizeof(ColdataHeader));
  }
  size_t bad_header_counter = 0;
  for (unsigned i = 0; i < num_frames; ++i) {
    const WIBHeader* curr_whead;
    std::vector<const ColdataHeader*> curr_chead(4);
    // See if the current headers were bad.
    bool bad_header = (bad_headers[i / 8] >> (7 - (1 % 8))) & 1;
    if(bad_header) {
      ++bad_header_counter;
      // Set current headers to be the bad one.
      std::cout << "BAD HEADER\n";
      curr_whead = reinterpret_cast<WIBHeader const*>(
          src + bad_header_counter * sizeof_header_set);
      for (unsigned j = 0; j < 4; ++j) {
        curr_chead[j] = reinterpret_cast<ColdataHeader const*>(
            src + bad_header_counter * sizeof_header_set +
            sizeof(WIBHeader) + j * sizeof(ColdataHeader));
      }
    } else {
      // Set current headers to be the first.
      curr_whead = whead;
      for(unsigned j = 0; j < 4; ++j) {
        curr_chead[j] = chead[j];
      }
    }
    // Assign header fields.
    (frame + i)->set_sof(curr_whead->sof);
    (frame + i)->set_version(curr_whead->version);
    (frame + i)->set_fiber_no(curr_whead->fiber_no);
    (frame + i)->set_crate_no(curr_whead->crate_no);
    (frame + i)->set_slot_no(curr_whead->slot_no);
    (frame + i)->set_mm(curr_whead->mm);
    (frame + i)->set_oos(curr_whead->oos);
    (frame + i)->set_wib_errors(curr_whead->wib_errors);
    (frame + i)->set_timestamp(curr_whead->timestamp() + i * 25);
    (frame + i)->set_wib_counter(curr_whead->wib_counter());
    (frame + i)->set_z(curr_whead->z);
    for(unsigned j = 0; j < 4; ++j) {
      (frame + i)->set_s1_error(j, curr_chead[j]->s1_error);
      (frame + i)->set_s2_error(j, curr_chead[j]->s2_error);
      (frame + i)->set_coldata_convert_count(
          j, curr_chead[j]->coldata_convert_count + i * 25);
      (frame + i)->set_error_register(j, curr_chead[j]->error_register);
    }
  }
  src += (bad_header_counter+1) * sizeof_header_set;

  // Read frequency table and generate a Huffman tree.
  std::unordered_map<uint16_t, unsigned> freq_table;
  for (unsigned i = 0; i < unique_values; ++i) {
    const uint16_t* v = reinterpret_cast<uint16_t const*>(src);
    const uint32_t* f =
        reinterpret_cast<uint32_t const*>(src + sizeof(uint16_t));
    freq_table[*v] = *f;
    src += sizeof(uint16_t) + sizeof(uint32_t);
  }
  std::vector<HuffTree::Node> nodes;
  for (auto p : freq_table) {
    HuffTree::Node curr_node;
    curr_node.value = p.first;
    curr_node.frequency = p.second;
    nodes.push_back(curr_node);
  }
  HuffTree hufftree;
  hufftree.make_tree(nodes);

  // Read ADC values from the buffer.
  size_t bits_read = 0;
  for (unsigned i = 0; i < num_frames * 256; ++i) {
    // Pointer to walk through the tree.
    HuffTree::Node* pnode = hufftree.root;
    while (pnode->left != NULL) {
      if (*src >> (bits_read % 8) & 1) {  // Next bit is 1.
        pnode = pnode->right;
      } else {  // Next bit is 0.
        pnode = pnode->left;
      }
      // Increment bits read and possibly the source pointer.
      ++bits_read;
      if (bits_read % 8 == 0) {
        ++src;  // Next byte reached.
      }
    }
    // Value reached.
    adc_t found_val = pnode->value;
#ifdef PREV
    if (i % num_frames != 0) {
      found_val += (frame + i % num_frames - 1)->channel(i / num_frames);
    }
#endif
    (frame + i % num_frames)->set_channel(i / num_frames, found_val);
  }

  return result;
}

}  // namespace dune

#endif /* artdaq_dune_Overlays_FelixComp_hh */
