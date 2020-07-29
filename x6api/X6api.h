// This is the main header file for x6_1000m api
// author: LiuQichun
// date: 2020-04-15

// x6api.h

#ifndef X6apiH
#define X6apiH

#include "arb_wf.h"
#include <array>
#include <stdint.h>
#include <X6_1000M_Mb.h>
#include <VitaPacketStream_Mb.h>
#include <SoftwareTimer_Mb.h>
#include <Application/TriggerManager_App.h>


using namespace std;

class X6api;
    
typedef std::vector<char>   BoolArray;
typedef std::vector<float>  FloatArray;

//==============================================================================
//  CLASS RxSettings
//==============================================================================
struct RxSettings
{
    int             BusmasterSize;
	float           SampleRate;
    //  Trigger
    int             ExternalTrigger;
    int             EdgeTrigger;
    int             Framed;
    int             FrameSize;
	int             repeats;
	int             TriggerDelayPeriod;
	std::array<int,2>	ActiveChannels;
    bool            DecimationEnable;
    int             DecimationFactor;
    //  Streaming
	int             PacketSize;
    bool            ForceSize;
    // Testing
    bool            TestCounterEnable;
    int             TestGenMode;
    //
    //  Not saved in INI file
    //  ..Eeprom
    FloatArray      Gain;
    FloatArray      Offset;
    bool            Calibrated;
};
//==============================================================================
//  CLASS TxSettings
//==============================================================================
struct TxSettings
{
    int             BusmasterSize;
	float           SampleRate;

    //  Trigger
    int             ExternalTrigger;
    int             EdgeTrigger;
    int             Framed;
    int             FrameSize;
	int             TriggerDelayPeriod;
	std::array<int, 4>	ActiveChannels;
    bool            DecimationEnable;
    int             DecimationFactor;
    //  Streaming
	int             PacketSize;
    bool            AutoPreconfig;
    //
    //  Not saved in INI file
    //  ..Eeprom
    FloatArray      Gain;
    FloatArray      Offset;
    bool            Calibrated;

    struct PatternModeSettings
    {
        unsigned int  Tag;
        unsigned int  Addr;
        unsigned int  SizeInEvents;
        unsigned int  RepCount;
        unsigned int  Mode;
        int           DB_Selection;
        std::string   DB_Label;
        bool          LoopMode;
    };
    // Pattern
    PatternModeSettings  Pattern;
    
    struct PatternDBEntry
    {
        unsigned int  Addr;
        unsigned int  SizeInWords;
        std::string   DB_Label;

        PatternDBEntry(std::string label, unsigned int addr, unsigned int size)
            :  Addr(addr), SizeInWords(size), DB_Label(label)
            {}
    };

    typedef std::vector<PatternDBEntry> PatternDBArray;
    PatternDBArray LoadedPatterns;
};

//==============================================================================
//  CLASS ApplicationSettings
//==============================================================================

struct ApplicationSettings
{
    typedef std::vector<char>   BoolArray;
    typedef std::vector<int>    IntArray;
    //  Board Settings
    int             Target;
    //  Config Data
    //  Clock
    int             ExtClockSrcSelection;
    int             ReferenceClockSource;
    float           ReferenceRate;
    int             SampleClockSource;
    int             ExtTriggerSrcSelection;
    //
    std::string     ModuleName;
    std::string     ModuleRevision;
    //
    RxSettings      Rx;
    TxSettings      Tx;
};


//===========================================================================
//  CLASS X6api  -- Hardware Access and Application Io Class
//===========================================================================

class X6api
{
public:
    X6api();
    ~X6api();

    // Methods
	void	do_trigger(int trig_n);
    unsigned int    BoardCount();
    vector<string>  BoardNames();
    string          PrintDevices();
	void            ParaInit();
	void            Open(int target);
    bool            IsOpen(){  return FOpened;  }
    void            Close();
	void            StreamPreconfigure();
	bool            StartStreaming();
    void            StopStreaming();

	// adc and dac parameter setting
	void            set_ReferenceClockSource(int ref_clk_s);
	void            set_SampleClockSource(int sample_clk_s);
	void            set_ExternalTrigger(int ext_trig);
	void            set_AdcRate(double rate);
	void            set_DacRate(double rate);
	void            set_AdcFrameSize(int size);
	void            set_DacFrameSize(int size);
	void            set_AdcRepeats(int repeats);
	void            set_AdcActiveChannel(vector<int> active_channels);
	void            set_DacActiveChannel(vector<int> active_channels);

    bool            IsStreaming(){  return Timer.Enabled();  }
	void            write_wishbone_register(int baseAddr, int offset, int data);
	int        read_wishbone_register(int baseAddr, int offset) const;
    void            WriteRom();
    void            ReadRom();
    unsigned int     OutputChannels() const {  return 4;  }
    unsigned int     InputChannels() const {  return 2;  }
	
	// settings and waveform builder
	ApplicationSettings             Settings;
	Innovative::ArbWaveBuilder     Builder;

	// ADC data and DAC wavedata
	vector<int>            adc_data_;
	vector<int>            read_adc_data();
	vector<double>         wavedata_;
	void                   write_dac_wavedata(vector<double> wavedata);

    void    DacTestStatus()
	{
		int D0_value = Module.Output().Device(0).DacTestStatus();
		int D1_value = Module.Output().Device(1).DacTestStatus();
	}
    void    ClearDacTestStatus()
	{
		Module.Output().Device(0).ClearDacTestStatus();
		Module.Output().Device(1).ClearDacTestStatus();
	}

    float   Temperature();
    bool    PllLocked();
    bool    DacInternalCal();
    //
    void	EnterPatternMode();
    void	LeavePatternMode();
    void	PatternLoadCommand();
    void	PatternReplayCommand();
    //
    unsigned int  PatternSize();
    void	BufferTransmit(/*const Innovative::Buffer & Packet*/);
    void	ManualTrigger(double state);


    ///////////////////
    //Board Information
    ///////////////////
    //Logic Version
    std::string FpgaLogicVersion();
    //Hardware Variant
    unsigned short FpgaHardwareVariant();
    //Revision
    unsigned short PciLogicRevision();
    //Subrevision
    unsigned short FpgaLogicSubrevision();
    //Type
    std::string PciLogicType();
    //Board Revision
    std::string PciLogicPcb();
    //Chip
    std::string FpgaName();
    //PCI EXPRESS
    std::string PCIExpressLanes();
    //Bitstream Date
    std::string BitStreamDate();
    //Bitstream Time
    std::string BitStreamTime();

private:
    //
    //  Member Data
	Innovative::X6_1000M            Module;
    Innovative::VitaPacketStream    Stream;
    Innovative::TriggerManager      Trig;
	Innovative::SoftwareTimer       Timer;
    Innovative::StopWatch           RunTimeSW;
	Innovative::VeloBuffer          WaveformPacket;
	// App State Variables
	bool                            FOpened;
	bool                            FStreamConnected;
	bool                            Stopped;
	int                             PrefillPacketCount;

protected:
    void  HandleDataAvailable(Innovative::VitaPacketStreamDataEvent & Event);
	void  HandleBeforeStreamStart(OpenWire::NotifyEvent & Event);
    void  HandleAfterStreamStart(OpenWire::NotifyEvent & Event);
    void  HandleAfterStreamStop(OpenWire::NotifyEvent & Event);
	void  HandleAfterStop(OpenWire::NotifyEvent & Event);
    void  HandleTimer(OpenWire::NotifyEvent & Event);
    void  HandleDisableTrigger(OpenWire::NotifyEvent & Event);
    void  HandleExternalTrigger(OpenWire::NotifyEvent & Event);
    void  HandleSoftwareTrigger(OpenWire::NotifyEvent & Event);
    void  HandleManualTrigger(Innovative::ManualTriggerEvent & Event);
	// handle alerts
	void  HookAlerts();
	void  HandleTimestampRolloverAlert(Innovative::AlertSignalEvent & event);
	void  HandleSoftwareAlert(Innovative::AlertSignalEvent & event);
	void  HandleWarningTempAlert(Innovative::AlertSignalEvent & event);
	void  HandleInputFifoOverrunAlert(Innovative::AlertSignalEvent & event);
	void  HandleOutputFifoUnderflowAlert(Innovative::AlertSignalEvent & event);
	void  HandleTriggerAlert(Innovative::AlertSignalEvent & event);
	void  HandleInputOverrangeAlert(Innovative::AlertSignalEvent & event);
	void  HandleOutputOverrangeAlert(Innovative::AlertSignalEvent & event);
	void  HandlePatternDoneAlert(Innovative::AlertSignalEvent & event);
};
#endif
