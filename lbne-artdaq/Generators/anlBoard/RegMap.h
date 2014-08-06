#ifndef __REGMAP_H__
#define __REGMAP_H__

#include "anlTypes.h"

namespace SSPDAQ{

class RegMap{
 public:

  static RegMap& Get();
  // Registers in the ARM Processor
  unsigned int armStatus;
  unsigned int armError;
  unsigned int armCommand;
  unsigned int armVersion;
  unsigned int armTest[4];
  unsigned int armRxAddress;	// 0x00000020
  unsigned int armRxCommand;	// 0x00000024
  unsigned int armRxSize;		// 0x00000028
  unsigned int armRxStatus;	// 0x0000002C
  unsigned int armTxAddress;	// 0x00000030
  unsigned int armTxCommand;	// 0x00000034
  unsigned int armTxSize;		// 0x00000038
  unsigned int armTxStatus;	// 0x0000003C
  unsigned int armPackets;	// 0x00000040
  unsigned int armOperMode;	// 0x00000044
  unsigned int armOptions;	// 0x00000048
  unsigned int armModemStatus;// 0x0000004C

  // Registers in the Zynq FPGA
  unsigned int zynqTest[6];
  unsigned int fakeControl;
  unsigned int fakeNumEvents;
  unsigned int fakeEventSize;
  unsigned int fakeBaseline;
  unsigned int fakePeakSum;
  unsigned int fakePrerise;
  unsigned int timestamp[2];
  unsigned int codeErrCounts[5];
  unsigned int dispErrCounts[5];
  unsigned int linkStatus;
  unsigned int eventDataControl;
  unsigned int eventDataPhaseControl;
  unsigned int eventDataPhaseStatus;
  unsigned int c2cStatus;
  unsigned int c2cControl;
  unsigned int c2cIntrControl;
  unsigned int dspStatus;
  unsigned int clockStatus;
  unsigned int clockControl;
  unsigned int ledConfig;
  unsigned int ledInput;
  unsigned int eventDataStatus;

  // Registers in the Artix FPGA
  unsigned int board_id;
  unsigned int fifo_control;
  unsigned int mmcm_status;
  unsigned int module_id;
  unsigned int win_comp_min;
  unsigned int win_comp_max;
  unsigned int c2c_status;
  unsigned int c2c_intr_control;

  unsigned int control_status[12];
  unsigned int led_threshold[12];
  unsigned int cfd_parameters[12];
  unsigned int readout_pretrigger[12];
  unsigned int readout_window[12];

  unsigned int p_window[12];
  unsigned int k_window[12];
  unsigned int m1_window[12];
  unsigned int m2_window[12];
  unsigned int d_window[12];
  unsigned int i_window[12];
  unsigned int disc_width[12];
  unsigned int baseline_start[12];

  unsigned int trigger_input_delay;
  unsigned int gpio_output_width;
  unsigned int misc_config;
  unsigned int channel_pulsed_control;
  unsigned int led_config;
  unsigned int led_input;
  unsigned int baseline_delay;
  unsigned int diag_channel_input;
  unsigned int event_data_control;
  unsigned int adc_config;
  unsigned int adc_config_load;
  unsigned int lat_timestamp_lsb;
  unsigned int lat_timestamp_msb;
  unsigned int live_timestamp_lsb;
  unsigned int live_timestamp_msb;
  unsigned int sync_period;
  unsigned int sync_delay;
  unsigned int sync_count;
	
  unsigned int master_logic_status;
  unsigned int trigger_config;
  unsigned int overflow_status;
  unsigned int phase_value;
  unsigned int link_status;

  unsigned int code_revision;
  unsigned int code_date;

  unsigned int dropped_event_count[12];
  unsigned int accepted_event_count[12];
  unsigned int ahit_count[12];
  unsigned int disc_count[12];
  unsigned int idelay_count[12];
  unsigned int adc_data_monitor[12];
  unsigned int adc_status[12];

 private:
  RegMap(){};
  RegMap(RegMap const&); //Don't implement
  void operator=(RegMap const&); //Don't implement
};

}//namespace
#endif
