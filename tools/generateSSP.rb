
# Generate the FHiCL document which configures the lbne::TpcRceReceiver class


require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateSSP(startingFragmentId, boardId, interfaceType, fragmentType)

  sspConfig = String.new( "\
    generator: SSP
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    interface_type: %{interface_type}
    board_ip: \"192.168.1.123\"

    HardwareConfig:{

        eventDataInterfaceSelect: 0x00000001
      	c2c_control:             0x00000007
	c2c_master_intr_control: 0x00000000
	comm_clock_control:      0x00000001
	comm_led_config:         0x00000000
	comm_led_input:          0x00000000
	qi_dac_config:           0x00000000
	qi_dac_control:          0x00000001

	mon_config:              0x0012F000
	mon_select:              0x00FFFF00
	mon_gpio:                0x00000000
	mon_control:             0x00010001

	module_id:               0x00000001
	c2c_slave_intr_control:  0x00000000

	# ARR_channel_control:    [0x00F0E0C1,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000,
        #                         0x00000000]

        # ALL_channel_control:      0x80F00401
        # ALL_channel_control:      0x00006001
        ALL_channel_control:      0x00F0E081
           
	ALL_led_threshold:         150
	ALL_cfd_parameters:        0x1800
	ALL_readout_pretrigger:    100
	ALL_readout_window:        2046
	ALL_p_window:              0x20
	ALL_i2_window:             500
	ALL_m1_window:             10
	ALL_m2_window:             10
	ALL_d_window:              20
	ALL_i1_window:             500
	ALL_disc_width:            10
	ALL_baseline_start:        0x0000

        trigger_input_delay:       0x00000020
        gpio_output_width:         0x00001000
        front_panel_config:        0x00001101 # standard config?
        dsp_led_config:            0x00000000
        dsp_led_input:             0x00000000
        baseline_delay:            5
        diag_channel_input:        0x00000000
        qi_config:                 0x0FFF1F00
        qi_delay:                  0x00000000
        qi_pulse_width:            0x00000000
        external_gate_width:       0x00008000
        # dsp_clock_control:         0x00000013 # 0x1  - use ext clock to drive ADCs
                                              # 0x2  - use NOvA clock (0 value uses front panel input)
                                              # 0x10 - Enable clock jitter correction

        # dsp_clock_control:         0x00000000 # Use internal clock to drive ADCs, front panel
        #                                        # clock for sync


        #ALL_bias_config:           0x00040E21 # 26.5V - bit 0x4000 enables bias, bits 0xFFF set value
        #                                      # in range 0-30V

      }

      DAQConfig:{
        
        MillisliceLength:          1E7
        MillisliceOverlap:         1E6
        UseExternalTimestamp:      0
      }
    " )

  
  sspConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  sspConfig.gsub!(/\%\{board_id\}/, String(boardId))
  sspConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 
  sspConfig.gsub!(/\%\{interface_type\}/, String(interfaceType)) 

  return sspConfig

end
