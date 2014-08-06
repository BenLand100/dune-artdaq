#include "RegMap.h"


SSPDAQ::RegMap& SSPDAQ::RegMap::Get(void)
{

  static SSPDAQ::RegMap* instance = 0;

  if(!instance){
    instance=new SSPDAQ::RegMap;
    // NOTE: All comments about default values, read masks, and write masks are current as of 2/11/2014

    // Registers in the Zynq ARM	Address				Address		Default Value	Read Mask		Write Mask		Code Name
    instance->armStatus				= 0x00000000;	//	0x0000,		0xABCDEF01		0xFFFFFFFF		0x00000000		rregStatus
    instance->armError				= 0x00000004;	//	0x0004,		0xEF012345		0xFFFFFFFF		0x00000000		regError
    instance->armCommand				= 0x00000008;	//	0x0008,		0x00000000		0xFFFFFFFF		0xFFFFFFFF		regCommand
    instance->armVersion				= 0x0000000C;	//	0x000C,		0x00000001		0xFFFFFFFF		0x00000000		regVersion
    instance->armTest[0]				= 0x00000010;	//	0x0010,		0x12345678		0xFFFFFFFF		0xFFFFFFFF		regTest0
    instance->armTest[1]				= 0x00000014;	//	0x0014,		0x567890AB		0xFFFFFFFF		0xFFFFFFFF		regTest1
    instance->armTest[2]				= 0x00000018;	//	0x0018,		0x90ABCDEF		0xFFFFFFFF		0xFFFFFFFF		regTest2
    instance->armTest[3]				= 0x0000001C;	//	0x001C,		0xCDEF1234		0xFFFFFFFF		0xFFFFFFFF		regTest3
    instance->armRxAddress			= 0x00000020;	//	0x0020,		0xFFFFFFF0		0xFFFFFFFF		0x00000000		regRxAddress    
    instance->armRxCommand			= 0x00000024;	//	0x0024,		0xFFFFFFF1		0xFFFFFFFF		0x00000000		regRxCommand
    instance->armRxSize				= 0x00000028;	//	0x0028,		0xFFFFFFF2		0xFFFFFFFF		0x00000000		regRxSize   
    instance->armRxStatus				= 0x0000002C;	//	0x002C,		0xFFFFFFF3		0xFFFFFFFF		0x00000000		regRxStatus    
    instance->armTxAddress			= 0x00000030;	//	0x0030,		0xFFFFFFF4		0xFFFFFFFF		0x00000000		regTxAddress    
    instance->armTxCommand			= 0x00000034;	//	0x0034,		0xFFFFFFF5		0xFFFFFFFF		0x00000000		regTxCommand    
    instance->armTxSize				= 0x00000038;	//	0x0038,		0xFFFFFFF6		0xFFFFFFFF		0x00000000		regTxSize    
    instance->armTxStatus				= 0x0000003C;	//	0x003C,		0xFFFFFFF7		0xFFFFFFFF		0x00000000		regTxStatus    
    instance->armPackets				= 0x00000040;	//	0x0040,		0x00000000		0xFFFFFFFF		0x00000000		regPackets    
    instance->armOperMode				= 0x00000044;	//	0x0044,		0x00000000		0xFFFFFFFF		0x00000000		regOperMode    
    instance->armOptions				= 0x00000048;	//	0x0048,		0x00000000		0xFFFFFFFF		0x00000000		regOptions    
    instance->armModemStatus			= 0x0000004C;	//	0x004C,		0x00000000		0xFFFFFFFF		0x00000000		regModemStatus    
	
    // Registers in the Zynq FPGA	Address				Address		Default Value	Read Mask		Write Mask		VHDL Name
    instance->zynqTest[0]				= 0x40000000;	//	X"000",		X"33333333",	X"FFFFFFFF",	X"00000000",	regin_test_0
    instance->zynqTest[1]				= 0x40000004;	//	X"004",		X"44444444",	X"FFFFFFFF",	X"00000000",	unnamed test register
    instance->zynqTest[2]				= 0x40000008;	//	X"008",		X"55555555",	X"FFFFFFFF",	X"FFFF0000",	unnamed test register
    instance->zynqTest[3]				= 0x4000000C;	//	X"00C",		X"66666666",	X"FFFFFFFF",	X"0000FFFF",	unnamed test register
    instance->zynqTest[4]				= 0x40000010;	//	X"010",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_test_1
    instance->zynqTest[5]				= 0x40000014;	//	X"014",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	unnamed test register
    instance->fakeControl				= 0x40000020;	//	X"020",		X"00000000",	X"000000F8",	X"FFFFFFFF",	reg_fake_control
    instance->fakeNumEvents			= 0x40000024;	//	X"024",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_fake_num_events
    instance->fakeEventSize			= 0x40000028;	//	X"028",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_fake_event_size
    instance->fakeBaseline			= 0x4000002C;	//	X"02C",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_fake_baseline
    instance->fakePeakSum				= 0x40000030;	//	X"030",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_fake_peak_sum
    instance->fakePrerise				= 0x40000034;	//	X"034",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_fake_prerise
    instance->timestamp[0]			= 0x40000038;	//	X"038",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_timestamp (low bits)
    instance->timestamp[1]			= 0x4000003C;	//	X"03C",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_timestamp (high bits);

    instance->codeErrCounts[0]		= 0x40000100;	//	X"100",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_code_err_counts(0);
    instance->codeErrCounts[1]		= 0x40000104;	//	X"104",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_code_err_counts(1);
    instance->codeErrCounts[2]		= 0x40000108;	//	X"108",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_code_err_counts(2);
    instance->codeErrCounts[3]		= 0x4000010C;	//	X"10C",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_code_err_counts(3);
    instance->codeErrCounts[4]		= 0x40000110;	//	X"110",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_code_err_counts(4);
    instance->dispErrCounts[0]		= 0x40000120;	//	X"120",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_disp_err_counts(0);
    instance->dispErrCounts[1]		= 0x40000124;	//	X"124",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_disp_err_counts(1);
    instance->dispErrCounts[2]		= 0x40000128;	//	X"128",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_disp_err_counts(2)
    instance->dispErrCounts[3]		= 0x4000012C;	//	X"12C",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_disp_err_counts(3)
    instance->dispErrCounts[4]		= 0x40000130;	//	X"130",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_disp_err_counts(4);
    instance->linkStatus				= 0x40000140;	//	X"140",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_link_status;
    instance->eventDataControl		= 0x40000144;	//	X"144",		X"0020001F",	X"FFFFFFFF",	X"0033001F",	reg_event_data_control;
    instance->eventDataPhaseControl	= 0x40000148;	//	X"148",		X"00000000",	X"00000000",	X"00000003",	reg_event_data_phase_control;
    instance->eventDataPhaseStatus	= 0x4000014C;	//	X"14C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_event_data_phase_status;
    instance->c2cStatus				= 0x40000150;	//	X"150",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_c2c_status;
    instance->c2cControl				= 0x40000154;	//	X"154",		X"00000007",	X"FFFFFFFF",	X"00000007",	reg_c2c_control;
    instance->c2cIntrControl			= 0x40000158;	//	X"158",		X"00000000",	X"FFFFFFFF",	X"0000000F",	reg_c2c_intr_control;
    instance->dspStatus				= 0x40000160;	//	X"160",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_dsp_status
    instance->clockStatus				= 0x40000170;	//	X"170",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_clock_status;
    instance->clockControl			= 0x40000174;	//	X"174",		X"00000001",	X"FFFFFFFF",	X"00000001",	reg_clock_control
    instance->ledConfig				= 0x40000180;	//	X"180",		X"00000000",	X"FFFFFFFF",	X"00000003",	reg_led_config
    instance->ledInput				= 0x40000184;	//	X"184",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_led_input
    instance->eventDataStatus			= 0x40000190;	//	X"190",		X"00000000",	X"FFFFFFFF",	X"00000000",	regin_event_data_status

    // Registers in the Artix FPGA	Address				Default Value	Read Mask		Write Mask		VHDL Name
    instance->board_id				= 0x80000000;	//	X"000",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_board_id
    instance->fifo_control			= 0x80000004;	//	X"004",		X"00000000",	X"0FFFFFFF",	X"08000000",	reg_fifo_control
    instance->mmcm_status				= 0x80000020;	//	X"020",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_mmcm_status
    instance->module_id				= 0x80000024;	//	X"024",		X"00000000",	X"00000FFF",	X"00000FFF",	reg_module_id
    instance->win_comp_min			= 0x80000028;	//	X"028",		X"0000FE00",	X"0000FFFF",	X"0000FFFF",	reg_win_comp_min
    instance->win_comp_max			= 0x8000002C;	//	X"02C",		X"00000200",	X"0000FFFF",	X"0000FFFF",	reg_win_comp_max
    instance->c2c_status				= 0x80000030;	//	X"030",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_c2c_status
    instance->c2c_intr_control		= 0x80000034;	//	X"034",		X"00000000",	X"FFFFFFFF",	X"0000000F",	reg_c2c_intr_control

    instance->control_status[0]		= 0x80000040;	//	X"040",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(0)
    instance->control_status[1]		= 0x80000044;	//	X"044",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(1)
    instance->control_status[2]		= 0x80000048;	//	X"048",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(2)
    instance->control_status[3]		= 0x8000004C;	//	X"04C",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(3)
    instance->control_status[4]		= 0x80000050;	//	X"050",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(4)
    instance->control_status[5]		= 0x80000054;	//	X"054",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(5)
    instance->control_status[6]		= 0x80000058;	//	X"058",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(6)
    instance->control_status[7]		= 0x8000005C;	//	X"05C",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(7)
    instance->control_status[8]		= 0x80000060;	//	X"060",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(8)
    instance->control_status[9]		= 0x80000064;	//	X"064",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(9)
    instance->control_status[10]		= 0x80000068;	//	X"068",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(10)
    instance->control_status[11]		= 0x8000006C;	//	X"06C",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_control_status(11)

    instance->led_threshold[0]		= 0x80000080;	//	X"080",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(0)
    instance->led_threshold[1]		= 0x80000084;	//	X"084",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(1)
    instance->led_threshold[2]		= 0x80000088;	//	X"088",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(2)
    instance->led_threshold[3]		= 0x8000008C;	//	X"08C",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(3)
    instance->led_threshold[4]		= 0x80000090;	//	X"090",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(4)
    instance->led_threshold[5]		= 0x80000094;	//	X"094",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(5)
    instance->led_threshold[6]		= 0x80000098;	//	X"098",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(6)
    instance->led_threshold[7]		= 0x8000009C;	//	X"09C",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(7)
    instance->led_threshold[8]		= 0x800000A0;	//	X"0A0",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(8)
    instance->led_threshold[9]		= 0x800000A4;	//	X"0A4",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(9)
    instance->led_threshold[10]		= 0x800000A8;	//	X"0A8",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(10)
    instance->led_threshold[11]		= 0x800000AC;	//	X"0AC",		X"00000064",	X"00FFFFFF",	X"00FFFFFF",	reg_led_threshold(11)

    instance->cfd_parameters[0]		= 0x800000C0;	//	X"0C0",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(0)
    instance->cfd_parameters[1]		= 0x800000C4;	//	X"0C4",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(1)
    instance->cfd_parameters[2]		= 0x800000C8;	//	X"0C8",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(2)
    instance->cfd_parameters[3]		= 0x800000CC;	//	X"0CC",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(3)
    instance->cfd_parameters[4]		= 0x800000D0;	//	X"0D0",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(4)
    instance->cfd_parameters[5]		= 0x800000D4;	//	X"0D4",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(5)
    instance->cfd_parameters[6]		= 0x800000D8;	//	X"0D8",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(6)
    instance->cfd_parameters[7]		= 0x800000DC;	//	X"0DC",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(7)
    instance->cfd_parameters[8]		= 0x800000E0;	//	X"0E0",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(8)
    instance->cfd_parameters[9]		= 0x800000E4;	//	X"0E4",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(9)
    instance->cfd_parameters[10]		= 0x800000E8;	//	X"0E8",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(10)
    instance->cfd_parameters[11]		= 0x800000EC;	//	X"0EC",		X"00001800",	X"00001FFF",	X"00001FFF",	reg_cfd_parameters(11)

    instance->readout_pretrigger[0]	= 0x80000100;	//	X"100",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(0)
    instance->readout_pretrigger[1]	= 0x80000104;	//	X"104",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(1)
    instance->readout_pretrigger[2]	= 0x80000108;	//	X"108",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(2)
    instance->readout_pretrigger[3]	= 0x8000010C;	//	X"10C",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(3)
    instance->readout_pretrigger[4]	= 0x80000110;	//	X"110",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(4)
    instance->readout_pretrigger[5]	= 0x80000114;	//	X"114",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(5)
    instance->readout_pretrigger[6]	= 0x80000118;	//	X"118",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(6)
    instance->readout_pretrigger[7]	= 0x8000011C;	//	X"11C",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(7)
    instance->readout_pretrigger[8]	= 0x80000120;	//	X"120",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(8)
    instance->readout_pretrigger[9]	= 0x80000124;	//	X"124",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(9)
    instance->readout_pretrigger[10]	= 0x80000128;	//	X"128",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(10)
    instance->readout_pretrigger[11]	= 0x8000012C;	//	X"12C",		X"00000019",	X"000007FF",	X"000007FF",	reg_readout_pretrigger(11)

    instance->readout_window[0]		= 0x80000140;	//	X"140",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(0)
    instance->readout_window[1]		= 0x80000144;	//	X"144",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(1)
    instance->readout_window[2]		= 0x80000148;	//	X"148",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(2)
    instance->readout_window[3]		= 0x8000014C;	//	X"14C",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(3)
    instance->readout_window[4]		= 0x80000150;	//	X"150",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(4)
    instance->readout_window[5]		= 0x80000154;	//	X"154",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(5)
    instance->readout_window[6]		= 0x80000158;	//	X"158",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(6)
    instance->readout_window[7]		= 0x8000015C;	//	X"15C",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(7)
    instance->readout_window[8]		= 0x80000160;	//	X"160",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(8)
    instance->readout_window[9]		= 0x80000164;	//	X"164",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(9)
    instance->readout_window[10]		= 0x80000168;	//	X"168",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(10)
    instance->readout_window[11]		= 0x8000016C;	//	X"16C",		X"000000C8",	X"000007FE",	X"000007FE",	reg_readout_window(11)

    instance->p_window[0]				= 0x80000180;	//	X"180",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(0)
    instance->p_window[1]				= 0x80000184;	//	X"184",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(1)
    instance->p_window[2]				= 0x80000188;	//	X"188",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(2)
    instance->p_window[3]				= 0x8000018C;	//	X"18C",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(3)
    instance->p_window[4]				= 0x80000190;	//	X"190",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(4)
    instance->p_window[5]				= 0x80000194;	//	X"194",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(5)
    instance->p_window[6]				= 0x80000198;	//	X"198",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(6)
    instance->p_window[7]				= 0x8000019C;	//	X"19C",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(7)
    instance->p_window[8]				= 0x800001A0;	//	X"1A0",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(8)
    instance->p_window[9]				= 0x800001A4;	//	X"1A4",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(9)
    instance->p_window[10]			= 0x800001A8;	//	X"1A8",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(10)
    instance->p_window[11]			= 0x800001AC;	//	X"1AC",		X"00000000",	X"000003FF",	X"000003FF",	reg_p_window(11)

    instance->k_window[0]				= 0x800001C0;	//	X"1C0",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(0)
    instance->k_window[1]				= 0x800001C4;	//	X"1C4",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(1)
    instance->k_window[2]				= 0x800001C8;	//	X"1C8",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(2)
    instance->k_window[3]				= 0x800001CC;	//	X"1CC",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(3)
    instance->k_window[4]				= 0x800001D0;	//	X"1D0",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(4)
    instance->k_window[5]				= 0x800001D4;	//	X"1D4",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(5)
    instance->k_window[6]				= 0x800001D8;	//	X"1D8",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(6)
    instance->k_window[7]				= 0x800001DC;	//	X"1DC",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(7)
    instance->k_window[8]				= 0x800001E0;	//	X"1E0",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(8)
    instance->k_window[9]				= 0x800001E4;	//	X"1E4",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(9)
    instance->k_window[10]			= 0x800001E8;	//	X"1E8",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(10)
    instance->k_window[11]			= 0x800001EC;	//	X"1EC",		X"00000014",	X"000003FF",	X"000003FF",	reg_k_window(11)

    instance->m1_window[0]			= 0x80000200;	//	X"200",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(0)
    instance->m1_window[1]			= 0x80000204;	//	X"204",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(1)
    instance->m1_window[2]			= 0x80000208;	//	X"208",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(2)
    instance->m1_window[3]			= 0x8000020C;	//	X"20C",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(3)
    instance->m1_window[4]			= 0x80000210;	//	X"210",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(4)
    instance->m1_window[5]			= 0x80000214;	//	X"214",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(5)
    instance->m1_window[6]			= 0x80000218;	//	X"218",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(6)
    instance->m1_window[7]			= 0x8000021C;	//	X"21C",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(7)
    instance->m1_window[8]			= 0x80000220;	//	X"220",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(8)
    instance->m1_window[9]			= 0x80000224;	//	X"224",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(9)
    instance->m1_window[10]			= 0x80000228;	//	X"228",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(10)
    instance->m1_window[11]			= 0x8000022C;	//	X"22C",		X"0000000A",	X"000003FF",	X"000003FF",	reg_m1_window(11)

    instance->m2_window[0]			= 0x80000240;	//	X"240",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(0)
    instance->m2_window[1]			= 0x80000244;	//	X"244",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(1)
    instance->m2_window[2]			= 0x80000248;	//	X"248",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(2)
    instance->m2_window[3]			= 0x8000024C;	//	X"24C",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(3)
    instance->m2_window[4]			= 0x80000250;	//	X"250",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(4)
    instance->m2_window[5]			= 0x80000254;	//	X"254",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(5)
    instance->m2_window[6]			= 0x80000258;	//	X"258",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(6)
    instance->m2_window[7]			= 0x8000025C;	//	X"25C",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(7)
    instance->m2_window[8]			= 0x80000260;	//	X"260",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(8)
    instance->m2_window[9]			= 0x80000264;	//	X"264",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(9)
    instance->m2_window[10]			= 0x80000268;	//	X"268",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(10)
    instance->m2_window[11]			= 0x8000026C;	//	X"26C",		X"00000014",	X"0000007F",	X"0000007F",	reg_m2_window(11)

    instance->d_window[0]				= 0x80000280;	//	X"280",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(0)
    instance->d_window[1]				= 0x80000284;	//	X"284",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(1)
    instance->d_window[2]				= 0x80000288;	//	X"288",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(2)
    instance->d_window[3]				= 0x8000028C;	//	X"28C",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(3)
    instance->d_window[4]				= 0x80000290;	//	X"290",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(4)
    instance->d_window[5]				= 0x80000294;	//	X"294",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(5)
    instance->d_window[6]				= 0x80000298;	//	X"298",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(6)
    instance->d_window[7]				= 0x8000029C;	//	X"29C",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(7)
    instance->d_window[8]				= 0x800002A0;	//	X"2A0",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(8)
    instance->d_window[9]				= 0x800002A4;	//	X"2A4",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(9)
    instance->d_window[10]			= 0x800002A8;	//	X"2A8",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(10)
    instance->d_window[11]			= 0x800002AC;	//	X"2AC",		X"00000014",	X"0000007F",	X"0000007F",	reg_d_window(11)

    instance->i_window[0]				= 0x800002C0;	//	X"2C0",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(0)
    instance->i_window[1]				= 0x800002C4;	//	X"2C4",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(1)
    instance->i_window[2]				= 0x800002C8;	//	X"2C8",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(2)
    instance->i_window[3]				= 0x800002CC;	//	X"2CC",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(3)
    instance->i_window[4]				= 0x800002D0;	//	X"2D0",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(4)
    instance->i_window[5]				= 0x800002D4;	//	X"2D4",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(5)
    instance->i_window[6]				= 0x800002D8;	//	X"2D8",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(6)
    instance->i_window[7]				= 0x800002DC;	//	X"2DC",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(7)
    instance->i_window[8]				= 0x800002E0;	//	X"2E0",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(8)
    instance->i_window[9]				= 0x800002E4;	//	X"2E4",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(9)
    instance->i_window[10]			= 0x800002E8;	//	X"2E8",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(10)
    instance->i_window[11]			= 0x800002EC;	//	X"2EC",		X"00000010",	X"000003FF",	X"000003FF",	reg_i_window(11)

    instance->disc_width[0]			= 0x80000300;	//	X"300",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(0)
    instance->disc_width[1]			= 0x80000304;	//	X"304",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(1)
    instance->disc_width[2]			= 0x80000308;	//	X"308",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(2)
    instance->disc_width[3]			= 0x8000030C;	//	X"30C",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(3)
    instance->disc_width[4]			= 0x80000310;	//	X"310",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(4)
    instance->disc_width[5]			= 0x80000314;	//	X"314",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(5)
    instance->disc_width[6]			= 0x80000318;	//	X"318",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(6)
    instance->disc_width[7]			= 0x8000031C;	//	X"31C",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(7)
    instance->disc_width[8]			= 0x80000320;	//	X"320",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(8)
    instance->disc_width[9]			= 0x80000324;	//	X"324",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(9)
    instance->disc_width[10]			= 0x80000328;	//	X"328",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(10)
    instance->disc_width[11]			= 0x8000032C;	//	X"32C",		X"00000000",	X"0000FFFF",	X"0000FFFF",	reg_disc_width(11)

    instance->baseline_start[0]		= 0x80000340;	//	X"340",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(0)
    instance->baseline_start[1]		= 0x80000344;	//	X"344",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(1)
    instance->baseline_start[2]		= 0x80000348;	//	X"348",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(2)
    instance->baseline_start[3]		= 0x8000034C;	//	X"34C",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(3)
    instance->baseline_start[4]		= 0x80000350;	//	X"350",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(4)
    instance->baseline_start[5]		= 0x80000354;	//	X"354",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(5)
    instance->baseline_start[6]		= 0x80000358;	//	X"358",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(6)
    instance->baseline_start[7]		= 0x8000035C;	//	X"35C",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(7)
    instance->baseline_start[8]		= 0x80000360;	//	X"360",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(8)
    instance->baseline_start[9]		= 0x80000364;	//	X"364",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(9)
    instance->baseline_start[10]		= 0x80000368;	//	X"368",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(10)
    instance->baseline_start[11]		= 0x8000036C;	//	X"36C",		X"00002000",	X"00003FFF",	X"00003FFF",	reg_baseline_start(11)

    instance->trigger_input_delay		= 0x80000400;	//	X"400",		X"00000010",	X"0000FFFF",	X"0000FFFF",	reg_trigger_input_delay
    instance->gpio_output_width		= 0x80000404;	//	X"404",		X"0000000F",	X"0000FFFF",	X"0000FFFF",	reg_gpio_output_width
    instance->misc_config				= 0x80000408;	//	X"408",		X"00000111",	X"00730333",	X"00730333",	reg_misc_config
    instance->channel_pulsed_control	= 0x8000040C;	//	X"40C",		X"00000000",	X"00000000",	X"FFFFFFFF",	reg_channel_pulsed_control
    instance->led_config				= 0x80000410;	//	X"410",		X"00000000",	X"FFFFFFFF",	X"00000003",	reg_led_config
    instance->led_input				= 0x80000414;	//	X"414",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_led_input
    instance->baseline_delay			= 0x80000418;	//	X"418",		X"00000019",	X"000000FF",	X"000000FF",	reg_baseline_delay
    instance->diag_channel_input		= 0x8000041C;	//	X"41C",		X"00000000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_diag_channel_input
    instance->event_data_control		= 0x80000424;	//	X"424",		X"00000001",	X"00020001",	X"00020001",	reg_event_data_control
    instance->adc_config				= 0x80000428;	//	X"428",		X"00010000",	X"FFFFFFFF",	X"FFFFFFFF",	reg_adc_config
    instance->adc_config_load			= 0x8000042C;	//	X"42C",		X"00000000",	X"00000000",	X"00000001",	reg_adc_config_load
    instance->lat_timestamp_lsb		= 0x80000484;	//	X"484",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_lat_timestamp (lsb)
    instance->lat_timestamp_msb		= 0x80000488;	//	X"488",		X"00000000",	X"0000FFFF",	X"00000000",	reg_lat_timestamp (msb)
    instance->live_timestamp_lsb		= 0x8000048C;	//	X"48C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_live_timestamp (lsb)
    instance->live_timestamp_msb		= 0x80000490;	//	X"490",		X"00000000",	X"0000FFFF",	X"00000000",	reg_live_timestamp (msb)
    instance->sync_period				= 0x80000494;	//	X"494",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_last_sync_reset_count
    instance->sync_delay				= 0x80000498;	//	X"498",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_external_timestamp (lsb)
    instance->sync_count				= 0x8000049C;	//	X"49C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_external_timestamp (msb)

    instance->master_logic_status		= 0x80000500;	//	X"500",		X"00000000",	X"FFFFFFFF",	X"00000073",	reg_master_logic_status
    instance->trigger_config			= 0x80000504;	//	X"504",		X"00000000",	X"00000003",	X"00000003",	reg_trigger_config
    instance->overflow_status			= 0x80000508;	//	X"508",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_overflow_status
    instance->phase_value				= 0x8000050C;	//	X"50C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_phase_value
    instance->link_status				= 0x80000510;	//	X"510",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_link_status

    instance->code_revision			= 0x80000600;	//	X"600",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_code_revision
    instance->code_date				= 0x80000604;	//	X"604",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_code_date

    instance->dropped_event_count[0]	= 0x80000700;	//	X"700",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(0)
    instance->dropped_event_count[1]	= 0x80000704;	//	X"704",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(1)
    instance->dropped_event_count[2]	= 0x80000708;	//	X"708",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(2)
    instance->dropped_event_count[3]	= 0x8000070C;	//	X"70C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(3)
    instance->dropped_event_count[4]	= 0x80000710;	//	X"710",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(4)
    instance->dropped_event_count[5]	= 0x80000714;	//	X"714",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(5)
    instance->dropped_event_count[6]	= 0x80000718;	//	X"718",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(6)
    instance->dropped_event_count[7]	= 0x8000071C;	//	X"71C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(7)
    instance->dropped_event_count[8]	= 0x80000720;	//	X"720",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(8)
    instance->dropped_event_count[9]	= 0x80000724;	//	X"724",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(9)
    instance->dropped_event_count[10]	= 0x80000728;	//	X"728",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(10)
    instance->dropped_event_count[11]	= 0x8000072C;	//	X"72C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_dropped_event_count(11)

    instance->accepted_event_count[0]	= 0x80000740;	//	X"740",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(0)
    instance->accepted_event_count[1]	= 0x80000744;	//	X"744",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(1)
    instance->accepted_event_count[2]	= 0x80000748;	//	X"748",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(2)
    instance->accepted_event_count[3]	= 0x8000074C;	//	X"74C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(3)
    instance->accepted_event_count[4]	= 0x80000750;	//	X"750",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(4)
    instance->accepted_event_count[5]	= 0x80000754;	//	X"754",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(5)
    instance->accepted_event_count[6]	= 0x80000758;	//	X"758",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(6)
    instance->accepted_event_count[7]	= 0x8000075C;	//	X"75C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(7)
    instance->accepted_event_count[8]	= 0x80000760;	//	X"760",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(8)
    instance->accepted_event_count[9]	= 0x80000764;	//	X"764",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(9)
    instance->accepted_event_count[10]= 0x80000768;	//	X"768",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(10)
    instance->accepted_event_count[11]= 0x8000076C;	//	X"76C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_accepted_event_count(11)

    instance->ahit_count[0]			= 0x80000780;	//	X"780",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(0)
    instance->ahit_count[1]			= 0x80000784;	//	X"784",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(1)
    instance->ahit_count[2]			= 0x80000788;	//	X"788",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(2)
    instance->ahit_count[3]			= 0x8000078C;	//	X"78C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(3)
    instance->ahit_count[4]			= 0x80000790;	//	X"790",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(4)
    instance->ahit_count[5]			= 0x80000794;	//	X"794",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(5)
    instance->ahit_count[6]			= 0x80000798;	//	X"798",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(6)
    instance->ahit_count[7]			= 0x8000079C;	//	X"79C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(7)
    instance->ahit_count[8]			= 0x800007A0;	//	X"7A0",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(8)
    instance->ahit_count[9]			= 0x800007A4;	//	X"7A4",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(9)
    instance->ahit_count[10]			= 0x800007A8;	//	X"7A8",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(10)
    instance->ahit_count[11]			= 0x800007AC;	//	X"7AC",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_ahit_count(11)

    instance->disc_count[0]			= 0x800007C0;	//	X"7C0",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(0)
    instance->disc_count[1]			= 0x800007C4;	//	X"7C4",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(1)
    instance->disc_count[2]			= 0x800007C8;	//	X"7C8",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(2)
    instance->disc_count[3]			= 0x800007CC;	//	X"7CC",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(3)
    instance->disc_count[4]			= 0x800007D0;	//	X"7D0",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(4)
    instance->disc_count[5]			= 0x800007D4;	//	X"7D4",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(5)
    instance->disc_count[6]			= 0x800007D8;	//	X"7D8",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(6)
    instance->disc_count[7]			= 0x800007DC;	//	X"7DC",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(7)
    instance->disc_count[8]			= 0x800007E0;	//	X"7E0",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(8)
    instance->disc_count[9]			= 0x800007E4;	//	X"7E4",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(9)
    instance->disc_count[10]			= 0x800007E8;	//	X"7E8",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(10)
    instance->disc_count[11]			= 0x800007EC;	//	X"7EC",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_disc_count(11)

    instance->idelay_count[0]			= 0x80000800;	//	X"800",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(0)
    instance->idelay_count[1]			= 0x80000804;	//	X"804",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(1)
    instance->idelay_count[2]			= 0x80000808;	//	X"808",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(2)
    instance->idelay_count[3]			= 0x8000080C;	//	X"80C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(3)
    instance->idelay_count[4]			= 0x80000810;	//	X"810",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(4)
    instance->idelay_count[5]			= 0x80000814;	//	X"814",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(5)
    instance->idelay_count[6]			= 0x80000818;	//	X"818",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(6)
    instance->idelay_count[7]			= 0x8000081C;	//	X"81C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(7)
    instance->idelay_count[8]			= 0x80000820;	//	X"820",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(8)
    instance->idelay_count[9]			= 0x80000824;	//	X"824",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(9)
    instance->idelay_count[10]		= 0x80000828;	//	X"828",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(10)
    instance->idelay_count[11]		= 0x8000082C;	//	X"82C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_idelay_count(11)

    instance->adc_data_monitor[0]		= 0x80000840;	//	X"840",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(0)
    instance->adc_data_monitor[1]		= 0x80000844;	//	X"844",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(1)
    instance->adc_data_monitor[2]		= 0x80000848;	//	X"848",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(2)
    instance->adc_data_monitor[3]		= 0x8000084C;	//	X"84C",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(3)
    instance->adc_data_monitor[4]		= 0x80000850;	//	X"850",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(4)
    instance->adc_data_monitor[5]		= 0x80000854;	//	X"854",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(5)
    instance->adc_data_monitor[6]		= 0x80000858;	//	X"858",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(6)
    instance->adc_data_monitor[7]		= 0x8000085C;	//	X"85C",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(7)
    instance->adc_data_monitor[8]		= 0x80000860;	//	X"860",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(8)
    instance->adc_data_monitor[9]		= 0x80000864;	//	X"864",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(9)
    instance->adc_data_monitor[10]	= 0x80000868;	//	X"868",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(10)
    instance->adc_data_monitor[11]	= 0x8000086C;	//	X"86C",		X"00000000",	X"0000FFFF",	X"00000000",	reg_adc_data_monitor(11)

    instance->adc_status[0]			= 0x80000880;	//	X"880",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(0)
    instance->adc_status[1]			= 0x80000884;	//	X"884",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(1)
    instance->adc_status[2]			= 0x80000888;	//	X"888",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(2)
    instance->adc_status[3]			= 0x8000088C;	//	X"88C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(3)
    instance->adc_status[4]			= 0x80000890;	//	X"890",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(4)
    instance->adc_status[5]			= 0x80000894;	//	X"894",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(5)
    instance->adc_status[6]			= 0x80000898;	//	X"898",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(6)
    instance->adc_status[7]			= 0x8000089C;	//	X"89C",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(7)
    instance->adc_status[8]			= 0x800008A0;	//	X"8A0",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(8)
    instance->adc_status[9]			= 0x800008A4;	//	X"8A4",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(9)
    instance->adc_status[10]			= 0x800008A8;	//	X"8A8",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(10)
    instance->adc_status[11]			= 0x800008AC;	//	X"8AC",		X"00000000",	X"FFFFFFFF",	X"00000000",	reg_adc_status(11)
  }
  return *instance;
}
