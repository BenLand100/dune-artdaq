
# Generate the FHiCL document which configures the lbne::TpcRceReceiver class


require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateSSP(startingFragmentId, boardId, interfaceType, fragmentType)

  sspConfig = String.new( "\
    generator: SSP                        ###Don't edit these lines
    fragment_type: %{fragment_type}       ###
    fragment_id: %{starting_fragment_id}  ###
    board_id: %{board_id}                 ###
    interface_type: %{interface_type}     ### (Currently set in manage script, to 0=USB, 1=Ethernet, 2=Emulated)

    board_ip: \"192.168.1.123\"           #Need to set for each board. Normally 192.168.1.1xx
                                          #where xx is device serial number (with leading 0 if appropriate)


      #####################
      #Set up DAQ software#
      #####################

      DAQConfig:{
        
      #**Important note**
      #NOvA clock is multiplied up so that ADC samples will be generated at 128MHz, but NOvA ticks used for timestamp
      #generation (and thus affecting physical millislice length) are at 64Hz.
      #Internal clock runs at 150MHz for both timestamping and ADC sampling.

        MillisliceLength:          1E7       #Amount of time to assign to a single fragment in clock ticks
        MillisliceOverlap:         1E6       #Amount at start of next fragment to also put into current fragment

        UseExternalTimestamp:      0         #0=Use internal timestamp to split events into millislices, 1=Use NOvA timestamp.

        StartOnNOvASync:           0         #Tell SSP to wait for sync pulse from NOvA timing unit before taking data.
                                             #In this case then DAQ will start 1st millislice exactly at NOvA sync timestamp

        EmptyWriteDelay:           1000000   #Time in us that DAQ will wait for SSP events, before starting to build and send
                                             #empty millislices. Limits the number of empty slices that will be generated simultaneously
                                             #if delay between events is large.

        HardwareClockRate:         150       #in MHz. Must be 64 for NOvA timestamping, 150 for internal.
                                             #NB This doesn't set the hardware clock, just tells the DAQ what it is!
      }

    ############################
    #Set named registers in SSP#
    ############################
                                         
    HardwareConfig:{                      #Won't affect behaviour of emulated devices

        ####################
        #Interface type, ID#
        ####################

        eventDataInterfaceSelect: 0x00000001     #0=USB, 1=Ethernet
	module_id:                0x00000001     #Set as desired to identify module; goes into event header.

        #################################
        #Trigger/channel enable settings#
        #################################

        #ALL_channel_control:      0x80F00401    #rising edge trigger
        #ALL_channel_control:      0x00006001    #front panel trigger
        ALL_channel_control:      0x00F0E051     #timestamp trigger 2.3kHz
        #ALL_channel_control:      0x00F0E071    #timestamp 143Hz
        #ALL_channel_control:      0x00F0E091    #timestamp 35.8Hz
        #ALL_channel_control:      0x00F0E0C1    #timestamp 1.12Hz

        #Alternative to setting ALL_channel_control - set channels up separately.
        #0x0 means channel disabled.
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

        ALL_led_threshold:        500            #Leading edge trigger threshold in ADC

        trigger_input_delay:       0x00000020    #Delay to apply to front panel trigger.
                                                 #Dont set less than 0x20.
        ################
        #Clock settings#
        ################

        #Use NOvA TDU
        #dsp_clock_control:         0x00000013 # Bit |  Meaning 
                                               # 0x1 |  1: use ext clock to drive DSP (i.e. sampling rate), 0: use internal clock
                                               # 0x2 |  1: External clock is NOvA TDU, 0: External clock is front panel input
                                               # 0x10|  1: Enable clock jitter correction

        #Use internal clock
        dsp_clock_control:         0x00000000 

        ############################
        #Data readout configuration#
        ############################

        ALL_readout_window:        200           #Number of ADC samples to read out
	ALL_readout_pretrigger:    0             #Of which before trigger time

        #Parameters used for calculation of metadata
        #i.e. peak shape, baseline etc as reported in event header.
        #Leave alone unless you know what you're doing!
	ALL_cfd_parameters:        0x1800
	ALL_p_window:              0x20
	ALL_i2_window:             500
	ALL_m1_window:             10
	ALL_m2_window:             10
	ALL_d_window:              20
	ALL_i1_window:             500
	ALL_disc_width:            10
	ALL_baseline_start:        0x0000

        ############################
        #Bias voltage configuration#
        ############################

        #bias_config register sets channel bias voltages - bit 0x40000 enables biasing,
        #bits 0xFFF set value on linear scale 0-30V.
        #Comment out all lines to disable biasing.
        #Can use ARR_bias_config (as above for ARR_channel_config) to set
        #bias voltages per channel.

        #ALL_bias_config:           0x00040FFF # 30V
        #ALL_bias_config:           0x00040E21 # 26.5V

        ###########################
        #Leave these alone please!#
        ###########################

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

	c2c_slave_intr_control:  0x00000000
           
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

      }

    " )

  
  sspConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  sspConfig.gsub!(/\%\{board_id\}/, String(boardId))
  sspConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 
  sspConfig.gsub!(/\%\{interface_type\}/, String(interfaceType)) 

  return sspConfig

end
