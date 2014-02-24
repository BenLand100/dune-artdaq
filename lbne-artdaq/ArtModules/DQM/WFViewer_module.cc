
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Run.h"
#include "art/Utilities/InputTag.h"
#include "art/Framework/Core/ModuleMacros.h"

#include "artdaq/DAQdata/Fragments.hh"

#include "lbne-artdaq/Overlays/FragmentType.hh"
#include "lbne-artdaq/Overlays/ToyFragment.hh"

#include "cetlib/exception.h"

#include "TRootCanvas.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TAxis.h"
#include "TH1D.h"
#include "TStyle.h"

#include <numeric>
#include <vector>
#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <initializer_list>
#include <limits>

using std::cout;
using std::cerr;
using std::endl;

namespace lbne {

  class WFViewer : public art::EDAnalyzer {

  public:
    explicit WFViewer (fhicl::ParameterSet const & p);
    virtual ~WFViewer () = default;

    void analyze (art::Event const & e) override;
    void beginRun(art::Run const &) override;

  private:

    std::unique_ptr<TCanvas> canvas_[2];
    std::vector<Double_t> x_;
    int prescale_;
    bool digital_sum_only_;
    art::RunNumber_t current_run_;

    int fragments_per_board_;
    int fragment_receiver_count_;

    std::vector<std::unique_ptr<TGraph>> graphs_;
    std::vector<std::unique_ptr<TH1D>> histograms_;

    std::vector<std::string> fragment_types_;

  };

}

lbne::WFViewer::WFViewer (fhicl::ParameterSet const & ps): 
  art::EDAnalyzer(ps), 
  prescale_(ps.get<int> ("prescale")), 
  digital_sum_only_(ps.get<bool> ("digital_sum_only", false)), 
  current_run_(0), 
  fragments_per_board_(ps.get<int>("fragments_per_board", 1)), 
  fragment_receiver_count_(ps.get<int>("fragment_receiver_count")), 
  graphs_(fragments_per_board_*fragment_receiver_count_), 
  histograms_(fragments_per_board_*fragment_receiver_count_),
  fragment_types_(ps.get<std::vector<std::string>>("fragment_type_labels"))
 {

  if (fragments_per_board_ != 1) {
    throw cet::exception("Default value of fragments_per_board == 1 must be used; contact John Freeman if you have any questions");
  }

  // Throw out any duplicate fragment_types_ ; we only care about the
  // different types that we plan to encounter, not how many of each
  // there are

  sort( fragment_types_.begin(), fragment_types_.end() );
  fragment_types_.erase( unique( fragment_types_.begin(), fragment_types_.end() ), fragment_types_.end() );

  gStyle->SetOptStat("irm");
  gStyle->SetMarkerStyle(22);
  gStyle->SetMarkerColor(4);
}


void lbne::WFViewer::analyze (art::Event const & e) {

  static int evt_cntr = -1;
  evt_cntr++;

  // John F., 1/22/14 -- there's probably a more elegant way of
  // collecting fragments of various types using ART interface code;
  // will investigate. Right now, we're actually re-creating the
  // fragments locally

  artdaq::Fragments fragments;

  for (auto label: fragment_types_) {

    art::Handle<artdaq::Fragments> fragments_with_label;

    e.getByLabel ("daq", label, fragments_with_label);
    
    for (int i_l = 0; i_l < static_cast<int>(fragments_with_label->size()); ++i_l) {
      fragments.emplace_back( (*fragments_with_label)[i_l] );
    }
  }

  int total_frags = fragments_per_board_*fragment_receiver_count_;

  // John F., 1/22/14 -- Changed consequences of an unexpected # of
  // fragments from a warning to an exception throw

  if (total_frags != static_cast<int>(fragments.size())) {
    cerr << "Warning in WFViewer: mismatch between expected and actual fragments: fragments_per_board_ = " << fragments_per_board_ << ", fragment_receiver_count_ = " << fragment_receiver_count_ << ", number of fragments = " << fragments.size() << endl;

    throw cet::exception("Error in WFViewer: mismatch between expected and actual fragments");
  }

  // John F., 1/5/14 

  // Here, we loop over the fragments passed to the analyze
  // function. A warning is flashed if either (A) the fragments aren't
  // all from the same event, or (B) there's an unexpected number of
  // fragments given the number of boardreaders and the number of
  // fragments per board

  // For every Nth event, where N is the "prescale" setting, plot the
  // distribution of ADC counts from each board_id / fragment_id
  // combo. Also, if "digital_sum_only" is set to false in the FHiCL
  // string, then plot, for the Nth event, a graph of the ADC values
  // across all channels in each board_id / fragment_id combo

  int expected_sequence_id = -1;

  for (size_t i = 0; i < fragments.size(); ++i) {

    // Pointers to the types of fragment overlays WFViewer can handle;
    // only one will be used per fragment, of course

    std::unique_ptr<ToyFragment> toyPtr;
    
    const auto& frag( fragments[i] );  // Basically a shorthand

    if (i == 0) 
      expected_sequence_id = frag.sequenceID();

    if (expected_sequence_id != static_cast<int>(frag.sequenceID())) {
      cerr << "Warning in WFViewer: expected fragment with sequence ID " << expected_sequence_id << ", received one with sequence ID " << frag.sequenceID() << endl;
    }
    
    FragmentType fragtype = static_cast<FragmentType>( frag.type() );
    int max_adc_count = std::numeric_limits<int>::max();
    int total_adc_values = std::numeric_limits<int>::max();

    // John F., 1/22/14 -- this should definitely be improved; I'm
    // just using the max # of bits per ADC value for a given fragment
    // type as is currently defined for the V172x fragments (as
    // opposed to the Toy fragment, which have this value in their
    // metadata). Since it's not using external variables for this
    // quantity, this would need to be edited should these values
    // change.

    switch ( fragtype ) {

    case FragmentType::TOY1: 
      toyPtr.reset( new ToyFragment(frag ));
      total_adc_values = toyPtr->total_adc_values();
      max_adc_count = pow(2, frag.template metadata<ToyFragment::Metadata>()->num_adc_bits) -1;
      break;
    case FragmentType::TOY2: 
      toyPtr.reset( new ToyFragment(frag ));
      total_adc_values = toyPtr->total_adc_values();
      max_adc_count = pow(2, frag.template metadata<ToyFragment::Metadata>()->num_adc_bits) -1;
      break;
    default: 
      throw cet::exception("Error in WFViewer: unknown fragment type supplied");
    }



    int fragment_id = static_cast<int>(frag.fragmentID() );

    int lg = fragment_id; // assuming fragment counting begins at 0            
    int lg_canvas = lg % total_frags + 1;
    int padnum = lg_canvas;

    // If a histogram doesn't exist for this board_id / fragment_id combo, create it

    if (!histograms_[lg]) {

      histograms_[lg] = std::unique_ptr<TH1D>(new TH1D( Form ("Fragment_%d_hist", fragment_id), "", max_adc_count+1, -0.5, max_adc_count + 0.5));

      histograms_[lg]->SetTitle (Form ("Frag %d, Type %s", fragment_id, 
				       fragmentTypeToString( fragtype  ).c_str() ) );
      histograms_[lg]->GetXaxis()->SetTitle("ADC value");
    }

    // For every event, fill the histogram (prescale is ignored here)

    // Is there some way to templatize an ART module? If not, we're
    // stuck with this switch code...

    switch ( fragtype ) {

    case FragmentType::TOY1: 
    case FragmentType::TOY2: 
      for (auto val = toyPtr->dataBegin(); val != toyPtr->dataEnd(); ++val ) 
	histograms_[lg]->Fill( *val );
      break;
  
    default: 
      throw cet::exception("Error in WFViewer: unknown fragment type supplied");
    }

    if (evt_cntr % prescale_ - 1) {
      continue;
    }

    // If we pass the prescale, then if we're not going with
    // digital_sum_only, plot the ADC counts for this particular event/board/fragment_id
    
    if (!digital_sum_only_) {

      // Create the graph's x-axis

      if (x_.size () != static_cast<size_t>(total_adc_values)) {
	x_.resize (total_adc_values);

	std::iota (x_.begin (), x_.end (), 0);
      }

      // If the graph doesn't exist, create it. Not sure whether to
      // make it an error if the total_adc_values is new

      if (!graphs_[lg] || graphs_[lg]->GetN () != total_adc_values) {
	graphs_[lg] = std::unique_ptr<TGraph>(new TGraph (total_adc_values));
	graphs_[lg]->SetName( Form ("Fragment_%d_graph", fragment_id));
	graphs_[lg]->SetLineColor ( 4 );
	std::copy (x_.begin (), x_.end (), graphs_[lg]->GetX ());
      }

      // Get the data from the fragment

      // Is there some way to templatize an ART module? If not, we're stuck with this awkward switch code...

      switch ( fragtype ) {

      case FragmentType::TOY1: 
      case FragmentType::TOY2: 
	{
	  std::copy (toyPtr->dataBegin (), toyPtr->dataBegin() + total_adc_values, graphs_[lg]->GetY ());
	}
	break;
  
      default: 
	throw cet::exception("Error in WFViewer: unknown fragment type supplied");
      }


      // And now prepare the graphics without actually drawing anything yet
      
      canvas_[1] -> cd(padnum);
      TVirtualPad* pad = static_cast<TVirtualPad*>(canvas_[1]->GetPad(padnum) ); 

      Double_t lo_x, hi_x, lo_y, hi_y, dummy;

      graphs_[lg]->GetPoint(0, lo_x, dummy);
      graphs_[lg]->GetPoint( graphs_[lg]->GetN()-1, hi_x, dummy);

      lo_x -= 0.5;
      hi_x += 0.5;

      lo_y = -0.5;
      hi_y = max_adc_count+0.5;


      TH1F* padframe = static_cast<TH1F*>( pad->DrawFrame( lo_x, lo_y, hi_x, hi_y ) );
      padframe->SetTitle( Form ("Frag %d, Type %s, SeqID %d", fragment_id, 
				fragmentTypeToString( fragtype  ).c_str(), 
				expected_sequence_id) );
      padframe->GetXaxis()->SetTitle("ADC #");
      pad->SetGrid();
      padframe->Draw("SAME");

    }

    // Draw the histogram

    canvas_[0]->cd(lg+1);
    histograms_[lg]->Draw();

    canvas_[0] -> Modified();
    canvas_[0] -> Update();

    // And, if desired, the Nth event's ADC counts
  
    if (!digital_sum_only_) {
      canvas_[1]->cd(lg+1);
      graphs_[lg]->Draw("PSAME");

      canvas_[1] -> Modified();
      canvas_[1] -> Update();
    }

  }
}

void lbne::WFViewer::beginRun(art::Run const &e) { 
  if (e.run () == current_run_) return;
  current_run_ = e.run ();

  for (int i = 0; i < 2; i++) canvas_[i] = 0;
  for (auto &x: graphs_) x = 0;
  for (auto &x: histograms_) x = 0;

  for (int i = 0; (i < 2 && !digital_sum_only_) || i < 1 ; i++) {
    canvas_[i] = std::unique_ptr<TCanvas>(new TCanvas(Form("wf%d",i)));
    canvas_[i]->Divide (fragments_per_board_, fragment_receiver_count_);
    canvas_[i]->Update ();
    ((TRootCanvas*)canvas_[i]->GetCanvasImp ())->DontCallClose ();
  }

  canvas_[0]->SetTitle("ADC Value Distribution");

  if (! digital_sum_only_) {
    canvas_[1]->SetTitle("ADC Values, Event Snapshot");
  }
}


DEFINE_ART_MODULE(lbne::WFViewer)
