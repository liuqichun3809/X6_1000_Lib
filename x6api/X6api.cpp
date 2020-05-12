// This is the main cpp file for x6_1000m api
// author: LiuQichun
// date: 2020-04-15

// x6api.cpp

#define NOMINMAX

#include <afx.h>
#include "X6api.h"
#include <iostream>
#include <fstream>
#include <Malibu_Mb.h>
#include <IppMemoryUtils_Mb.h>
#include <SystemSupport_Mb.h>
#include <Exception_Mb.h>

using namespace Innovative;
using namespace std;

//===========================================================================
//===========================================================================
//  CLASS X6api  -- Hardware Access and Application I/O Class
//===========================================================================
//===========================================================================

//---------------------------------------------------------------------------
//  constructor for class X6api
//---------------------------------------------------------------------------
X6api::X6api()
{
    // Use IPP performance memory functions.
    Init::UsePerformanceMemoryFunctions();
    //
    Timer.Interval(1000);
	ParaInit();
}

//---------------------------------------------------------------------------
//  destructor for class X6api
//---------------------------------------------------------------------------
X6api::~X6api()
{
   Close();
}

//---------------------------------------------------------------------------
//  X6api::ParaInit()
//---------------------------------------------------------------------------
void X6api::ParaInit()
{
	FOpened = false;
	FStreamConnected = false;
	Stopped = true;

	Settings.Target = 0;
	const int   kDefHbuSige = 32;
	Settings.Rx.BusmasterSize = kDefHbuSige;
	Settings.Tx.BusmasterSize = kDefHbuSige;
	//  Config Data
	//  ..Clock
	Settings.ExtClockSrcSelection = 0; // 0 for front panel
	Settings.ReferenceClockSource = 1; // 0 for external
	Settings.ReferenceRate = 10.0; // freq unit MHz
	Settings.SampleClockSource = 1; // 0 for external
	Settings.Rx.SampleRate = 1000.0; // freq unit MHz
	Settings.Tx.SampleRate = 500.0; // freq unit MHz
	//  ..Trigger
	Settings.Tx.ExternalTrigger = 0;
	Settings.Tx.EdgeTrigger = 1;
	Settings.Tx.Framed = 1;
	Settings.Tx.FrameSize = 0x10000;
	Settings.ExtTriggerSrcSelection = 0; // 0 for front panel
	Settings.Tx.TriggerDelayPeriod = 1;
	//  ..Analog
	Settings.Tx.ActiveChannels[0] = 1;
	Settings.Tx.ActiveChannels[1] = 1;
	Settings.Tx.ActiveChannels[2] = 1;
	Settings.Tx.ActiveChannels[3] = 1;
	Settings.Tx.DecimationEnable = false;
	Settings.Tx.DecimationFactor = 0;

	Settings.Tx.Pattern.LoopMode = false;
	Settings.Tx.Pattern.Addr = 0x0;
	Settings.Tx.Pattern.RepCount = 1;

	//  ..Streaming
	Settings.Tx.PacketSize = 0x100000;
	Settings.Tx.AutoPreconfig = true;

	// Rx
	Settings.Rx.ExternalTrigger = 0;
	Settings.Rx.EdgeTrigger = 1;
	Settings.Rx.Framed = 1;
	Settings.Rx.FrameSize = 0x800;
	Settings.Rx.repeats = 100;

	Settings.Rx.TriggerDelayPeriod = 1;
	Settings.Rx.DecimationEnable = false;
	Settings.Rx.DecimationFactor = 0;
	Settings.Rx.ActiveChannels[0] = 1;
	Settings.Rx.ActiveChannels[1] = 1;
	Settings.Rx.PacketSize = (Settings.Rx.FrameSize+16)*Settings.Rx.repeats*2;//each frame has 14 header and 2 tailer data, default active 2 input channel
	Settings.Rx.ForceSize = true;
	Settings.Rx.TestCounterEnable = false;
	Settings.Rx.TestGenMode = 0;
}
void X6api::set_ReferenceClockSource(int ref_clk_s)
{
	Settings.ReferenceClockSource = ref_clk_s;
}
void X6api::set_SampleClockSource(int sample_clk_s)
{
	Settings.SampleClockSource = sample_clk_s;
}
void X6api::set_ExternalTrigger(int ext_trig)
{
	Settings.Rx.ExternalTrigger = ext_trig;
	Settings.Tx.ExternalTrigger = ext_trig;
}
void X6api::set_AdcRate(double rate)
{
	Settings.Rx.SampleRate = rate;
}
void X6api::set_DacRate(double rate)
{
	Settings.Tx.SampleRate = rate;
}
void X6api::set_AdcFrameSize(int size)
{
	int framesize = Module.Input().Info().TriggerFrameGranularity();
	if (size % framesize)
	{
		Settings.Rx.FrameSize = (size / framesize + 1)*framesize;
	}
	else
	{
		Settings.Rx.FrameSize = size;
	}
}
void X6api::set_DacFrameSize(int size)
{
	int framesize = Module.Output().Info().TriggerFrameGranularity();
	if (size % framesize)
	{
		Settings.Tx.FrameSize = (size / framesize + 1)*framesize;
	}
	else
	{
		Settings.Tx.FrameSize = size;
	}
}
void X6api::set_AdcRepeats(int repeats)
{
	Settings.Rx.repeats = repeats;
}
void X6api::set_AdcActiveChannel(vector<int> active_channels)
{
	Settings.Rx.ActiveChannels[0] = active_channels[0];
	Settings.Rx.ActiveChannels[1] = active_channels[1];
}
void X6api::set_DacActiveChannel(vector<int> active_channels)
{
	Settings.Tx.ActiveChannels[0] = active_channels[0];
	Settings.Tx.ActiveChannels[1] = active_channels[1];
	Settings.Tx.ActiveChannels[2] = active_channels[2];
	Settings.Tx.ActiveChannels[3] = active_channels[3];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// X6 board operation
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//---------------------------------------------------------------------------
//  X6api::BoardCount() -- Query number of installed boards
//---------------------------------------------------------------------------
unsigned int X6api::BoardCount()
{
    return static_cast<unsigned int>(Module.BoardCount());
}

//---------------------------------------------------------------------------
//  X6api::BoardNames() -- Get Device Names
//---------------------------------------------------------------------------
vector<string> X6api::BoardNames()
{
    return Module.BoardNames();
}

//---------------------------------------------------------------------------
//  X6api::PrintDevices() -- Print Device Names and Target #
//---------------------------------------------------------------------------
string X6api::PrintDevices()
{
    return Module.PrintAllDevices();
}

//---------------------------------------------------------------------------
// X6api::Open()
//---------------------------------------------------------------------------
void X6api::Open(int target)
{
	ParaInit();
	Settings.Target = target;
	//  Configure Trigger Manager Event Handlers
    Trig.OnDisableTrigger.SetEvent(this, &X6api::HandleDisableTrigger);
    Trig.OnExternalTrigger.SetEvent(this, &X6api::HandleExternalTrigger);
    Trig.OnSoftwareTrigger.SetEvent(this, &X6api::HandleSoftwareTrigger);
    Trig.OnManualTrigger.SetEvent(this, &X6api::HandleManualTrigger);

    Trig.DelayedTrigger(true);   // trigger delayed after start

    //
    //  Configure Module Event Handlers
    Module.OnBeforeStreamStart.SetEvent(this, &X6api::HandleBeforeStreamStart);
    Module.OnBeforeStreamStart.Synchronize();
    Module.OnAfterStreamStart.SetEvent(this, &X6api::HandleAfterStreamStart);
    Module.OnAfterStreamStart.Synchronize();
    Module.OnAfterStreamStop.SetEvent(this, &X6api::HandleAfterStreamStop);
    Module.OnAfterStreamStop.Synchronize();

    //
    //  Alerts
    HookAlerts();

    //
    //  Configure Stream Event Handlers
    //Stream.OnVeloDataRequired.SetEvent(this, &X6api::HandleDataRequired);
    Stream.OnVeloDataAvailable.SetEvent(this, &X6api::HandleDataAvailable);
	Stream.DirectDataMode(false);

	Stream.OnAfterStop.SetEvent(this, &X6api::HandleAfterStop);
	Stream.OnAfterStop.Synchronize();

    Stream.RxLoadBalancing(false);
    Stream.TxLoadBalancing(false);

    Timer.OnElapsed.SetEvent(this, &X6api::HandleTimer);
    Timer.OnElapsed.Thunk();

    // Insure BM size is a multiple of four MB
    const int Meg = 1024 * 1024;
    const int RxBmSize = std::max(Settings.Rx.BusmasterSize/4, 1) * 4;
    const int TxBmSize = std::max(Settings.Tx.BusmasterSize/4, 1) * 4;
    Module.IncomingBusMasterSize(RxBmSize * Meg);
    Module.OutgoingBusMasterSize(TxBmSize * Meg);
    Module.Target(Settings.Target);
    //
    //  Open Devices
    try
        {
		Module.Open();
        }
    catch (Innovative::MalibuException & exception)
        {
        cout << "Open Failure \n";
		cout << exception.what() << "\n";
        return;
        }
    catch(...)
        {
        cout << "Module Device Open Failure! \n";
        return;
        }
        
    Module.Reset();
    FOpened = true;
    //
    //  Connect Stream
    Stream.ConnectTo(&Module);
    FStreamConnected = true;
    PrefillPacketCount = Stream.PrefillPacketCount();
}

//---------------------------------------------------------------------------
// X6api::Close()
//---------------------------------------------------------------------------
void X6api::Close()
{
    Stream.Disconnect();
    Module.Close();
    FStreamConnected = false;
    FOpened = false;
	//
	adc_data_.clear();
	adc_data_.swap(vector<int>());
	//
}

//---------------------------------------------------------------------------
// X6api::StreamPreconfigure()
//---------------------------------------------------------------------------
void X6api::StreamPreconfigure()
{
	// update Rx.PacketSize
	Settings.Rx.PacketSize = (Settings.Rx.FrameSize + 16)*Settings.Rx.repeats * 2;
	//  Set Channel Enables
    Module.Output().ChannelDisableAll();
    for (unsigned int i = 0; i < Module.Output().Channels(); ++i)
        {
        bool active = Settings.Tx.ActiveChannels[i] ? true : false;
        if (active==true)
            Module.Output().ChannelEnabled(i, true);
        }

    //  Channel Enables
    Module.Input().ChannelDisableAll();
    for (unsigned int i = 0; i < Module.Input().Channels(); ++i)
        {
        bool active = Settings.Rx.ActiveChannels[i] ? true : false;
        if (active==true)
            Module.Input().ChannelEnabled(i, true);
        }

    //
    // Clock Configuration
    //   Route ext clock source
    IX6ClockIo::IIClockSelect cks[] = { IX6ClockIo::cslFrontPanel, IX6ClockIo::cslP16 };
    Module.Clock().ExternalClkSelect(cks[Settings.ExtClockSrcSelection]);
    //   Route reference.
    IX6ClockIo::IIReferenceSource ref[] = { IX6ClockIo::rsExternal, IX6ClockIo::rsInternal };
    Module.Clock().Reference(ref[Settings.ReferenceClockSource]);
    Module.Clock().ReferenceFrequency(Settings.ReferenceRate * 1e6);
    //   Route clock
    IX6ClockIo::IIClockSource src[] = { IX6ClockIo::csExternal, IX6ClockIo::csInternal };
    Module.Clock().Source(src[Settings.SampleClockSource]);
    Module.Clock().Adc().Frequency(Settings.Rx.SampleRate * 1e6);
    Module.Clock().Dac().Frequency(Settings.Tx.SampleRate * 1e6);
    // Readback Frequency
    double adc_freq_actual = Module.Clock().Adc().FrequencyActual();
    double dac_freq_actual = Module.Clock().Dac().FrequencyActual();
    double adc_freq = Module.Clock().Adc().Frequency();
    double dac_freq = Module.Clock().Dac().Frequency();
    Stream.Preconfigure();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ADC stream and get data
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//---------------------------------------------------------------------------
// X6api::StartStreaming()
//---------------------------------------------------------------------------
bool X6api::StartStreaming()
{
    //  if auto-preconfiging, call preconfig here.
    if (Settings.Tx.AutoPreconfig)
        StreamPreconfigure();

    if (!FStreamConnected)
        {
        cout << "Stream not connected! -- Open the boards \n";
        return false;
        }

    if (Settings.Tx.Framed)
        {
        // Granularity is firmware limitation
        int framesize = Module.Output().Info().TriggerFrameGranularity();
        if (Settings.Tx.FrameSize % framesize)
            {
            std::stringstream msg;
            msg << "Error: Ouput frame count must be a multiple of " << framesize;
			cout << msg.str() << "\n";
            return false;
            }
        }

    if (Settings.Rx.Framed)
        {
        // Granularity is firmware limitation
        int framesize = Module.Input().Info().TriggerFrameGranularity();
        if (Settings.Rx.FrameSize % framesize)
            {
            std::stringstream msg;
            msg << "Error: Input frame count must be a multiple of " << framesize;
			cout << msg.str() << "\n";
            return false;
            }
        }

    // Configure Trigger Mananger
    Trig.DelayedTriggerPeriod(Settings.Tx.TriggerDelayPeriod);
	Trig.ExternalTrigger(Settings.Tx.ExternalTrigger ? true : false);
    // In Pattern mode, trigger manually
    Trig.ManualTrigger(Module.Output().Pattern().PatternModeEnable());
    Trig.AtConfigure();

    //
    //  Channel Enables
    Module.Input().ChannelDisableAll();
    for (unsigned int i = 0; i < Module.Input().Channels(); ++i)
        {
        bool active = Settings.Rx.ActiveChannels[i] ? true : false;
        if (active==true)
            {
                Module.Input().ChannelEnabled(i, true);
            }
        }

    //
    //  Check Channel Enables
    if (!Module.Output().ActiveChannels() && !Module.Input().ActiveChannels())
        {
        cout << "Error: Must enable at least one channel \n";
        return false;
        }
    //
    // Trigger Configuration
    //  Frame Triggering
    Module.Output().Trigger().FramedMode((Settings.Tx.Framed)? true : false);
    Module.Output().Trigger().Edge((Settings.Tx.EdgeTrigger)? true : false);
    Module.Output().Trigger().FrameSize(Settings.Tx.FrameSize);
    Module.Input().Trigger().FramedMode((Settings.Rx.Framed)? true : false);
    Module.Input().Trigger().Edge((Settings.Rx.EdgeTrigger)? true : false);
    Module.Input().Trigger().FrameSize(Settings.Rx.FrameSize);

    //  Route External Trigger source
    IX6IoDevice::AfeExtSyncOptions syncsel[] = { IX6IoDevice::essFrontPanel, IX6IoDevice::essP16 };
    Module.Output().Trigger().ExternalSyncSource( syncsel[ Settings.ExtTriggerSrcSelection ] );
    Module.Input().Trigger().ExternalSyncSource( syncsel[ Settings.ExtTriggerSrcSelection ] );

     //  ...add to module configuration
	Module.Input().Pulse().Reset();
	Module.Input().Pulse().Enabled(false); //default pulse mode is enable£¬so must disable it
     //  ...add to module configuration
	Module.Output().Pulse().Reset();
	Module.Output().Pulse().Enabled(false); //default pulse mode is enable£¬so must disable it

    //
    //  Velocia Packet Size
	Module.Velo().VeloDataSize(0, Settings.Rx.PacketSize);
    Module.Velo().ForceVeloPacketSize(Settings.Rx.ForceSize);

    //
    //  Output Test Generator Setup
	Module.Input().TestModeEnabled(Settings.Rx.TestCounterEnable, Settings.Rx.TestGenMode);

    // Set Decimation Factor
    int factor = Settings.Tx.DecimationEnable ? Settings.Tx.DecimationFactor : 0;
    Module.Output().Decimation(factor);
    factor = Settings.Rx.DecimationEnable ? Settings.Rx.DecimationFactor : 0;
    Module.Input().Decimation(factor);

	//
	adc_data_.clear();
	adc_data_.swap(vector<int>());
    //

    Stream.PrefillPacketCount(0);
    Trig.AtStreamStart();
    //  Start Streaming
    Stopped = false;
    Stream.Start();
    return true;
}

//---------------------------------------------------------------------------
// X6api::StopStreaming()
//---------------------------------------------------------------------------
void X6api::StopStreaming()
{
    if (!IsStreaming())
        return;
    if (!FStreamConnected)
        {
        cout << "Stream not connected! -- Open the boards \n";
        return;
        }
    //  Stop Streaming
    Stream.Stop();
    Stopped = true;
    Timer.Enabled(false);
    //  Disable test generator
    if (Settings.Rx.TestCounterEnable)
		Module.Input().TestModeEnabled(false, Settings.Rx.TestGenMode);
    Trig.AtStreamStop();
}

//---------------------------------------------------------------------------
//  X6api::HandleDataAvailable() --  Handle received packet
//---------------------------------------------------------------------------
void  X6api::HandleDataAvailable(VitaPacketStreamDataEvent & Event)
{
	if (Stopped)
        return;
    VeloBuffer Packet;
    //  Extract the packet from the Incoming Queue...
    Event.Sender->Recv(Packet);
	AccessDatagram<int short> ShortDG(Packet);
	for (int idx = 0; idx < ShortDG.SizeInInts(); idx++)
	{
		adc_data_.push_back(ShortDG[idx]);
	}
	//  Stop streaming when both Channels have passed their limit
	double elapsed = RunTimeSW.Stop();
	StopStreaming();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  DAC arbitrary wave pattern builder
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//------------------------------------------------------------------------------
//  X6api::EnterPatternMode() --
//------------------------------------------------------------------------------
void  X6api::EnterPatternMode()
{
	Module.Output().Pattern().PatternModeEnable(true);
	//StartStreaming();
}

//------------------------------------------------------------------------------
//  X6api::LeavePatternMode() --
//------------------------------------------------------------------------------
void  X6api::LeavePatternMode()
{
	//StopStreaming();
	Module.Output().Pattern().PatternModeEnable(false);
}

//------------------------------------------------------------------------------
//  X6api::PatternLoadCommand() --
//------------------------------------------------------------------------------
void  X6api::PatternLoadCommand()
{
	//  Pack SIDs, Tags Arrays
	std::vector<unsigned int> sids;
	unsigned int sid_0 = Module.VitaOut().VitaStreamId(0);
	unsigned int sid_1 = Module.VitaOut().VitaStreamId(1);
	if (Module.Output().ChannelEnabled(0) || Module.Output().ChannelEnabled(1))
		sids.push_back(sid_0);
	if (Module.Output().ChannelEnabled(2) || Module.Output().ChannelEnabled(3))
		sids.push_back(sid_1);
	std::vector<char> tags;
	for (unsigned int i = 0; i<sids.size(); i++)
		tags.push_back(i);
	//
	//  Add to loaded patterns database
	Settings.Tx.LoadedPatterns.clear();
	Settings.Tx.Pattern.DB_Selection = 0;
	unsigned int buffer_size_ints = PatternSize(); // whole wavedata length for 4 channels
	TxSettings::PatternDBEntry entry("ArbWave", Settings.Tx.Pattern.Addr, buffer_size_ints);
	Settings.Tx.LoadedPatterns.push_back(entry);

	IPatternModeSystem::PatternRepeatType mode =
		Settings.Tx.Pattern.LoopMode ?
		IPatternModeSystem::prPlayAgain :
		IPatternModeSystem::prFlatline;
	//
	//  Send the Info Packet
	Module.Output().Pattern().SendPatternInfo(
		0, // pid 
		sids,
		tags, //Settings.Tx.Pattern.Tag, 
		Settings.Tx.Pattern.Addr,
		buffer_size_ints,
		Settings.Tx.Pattern.RepCount,
		IPatternModeSystem::piLoad,
		mode);
	//  Send the Data Packet(s) - uses WaveformPacket
	BufferTransmit();
}

//------------------------------------------------------------------------------
//  X6api::PatternReplayCommand() --
//------------------------------------------------------------------------------
void  X6api::PatternReplayCommand()
{
	//  Find pattern information from the database
	TxSettings::PatternDBEntry entry = Settings.Tx.LoadedPatterns[Settings.Tx.Pattern.DB_Selection];
	IPatternModeSystem::PatternRepeatType mode =
		Settings.Tx.Pattern.LoopMode ?
		IPatternModeSystem::prPlayAgain :
		IPatternModeSystem::prFlatline;
	//  Pack SIDs, Tags Arrays
	std::vector<unsigned int> sids;
	unsigned int sid_0 = Module.VitaOut().VitaStreamId(0);
	unsigned int sid_1 = Module.VitaOut().VitaStreamId(1);
	if (Module.Output().ChannelEnabled(0) || Module.Output().ChannelEnabled(1))
		sids.push_back(sid_0);
	if (Module.Output().ChannelEnabled(2) || Module.Output().ChannelEnabled(3))
		sids.push_back(sid_1);
	std::vector<char> tags;
	for (unsigned int i = 0; i<sids.size(); i++)
		tags.push_back(i);
	//
	//  Send the Info Packet
	Module.Output().Pattern().SendPatternInfo(
		0, // pid 
		sids,
		tags, //Settings.Tx.Pattern.Tag, 
		entry.Addr,
		entry.SizeInWords,
		Settings.Tx.Pattern.RepCount,
		IPatternModeSystem::piReplay,
		mode);
}

//---------------------------------------------------------------------------
//  X6api::BufferTransmit() -- 
//---------------------------------------------------------------------------
void X6api::BufferTransmit()
{
	//  Builds a N channel buffer
	int channels = Module.Output().ActiveChannels();
	int bits = Module.Output().Info().Bits();
	int samples = static_cast<int>(Settings.Tx.Pattern.SizeInEvents);

	//  Pack SIDs Array
	std::vector<int> sids;
	int sid_0 = Module.VitaOut().VitaStreamId(0);
	int sid_1 = Module.VitaOut().VitaStreamId(1);
	if (Module.Output().ChannelEnabled(0) || Module.Output().ChannelEnabled(1))
		sids.push_back(sid_0);
	if (Module.Output().ChannelEnabled(2) || Module.Output().ChannelEnabled(3))
		sids.push_back(sid_1);

	// build waveform buffer
	Builder.set_wavedata(wavedata_);
	Builder.Format(sids, channels, bits, samples);
	Builder.BuildWave(WaveformPacket);
	// In pattern mode directly send it
	if (Module.Output().Pattern().PatternModeEnable())
	{
		Stream.Send(WaveformPacket);
	}
}

//------------------------------------------------------------------------------
//  X6api::PatternSize() -- Calculate Size of Pattern in words
//------------------------------------------------------------------------------
unsigned int  X6api::PatternSize()
{
	unsigned int sizeInSamples = Settings.Tx.Pattern.SizeInEvents * Module.Output().ActiveChannels();

	return sizeInSamples * sizeof(short) / sizeof(int);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Support Functions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//---------------------------------------------------------------------------
//  X6api::Temperature() --  Query current module temperature
//---------------------------------------------------------------------------
float X6api::Temperature()
{
    return static_cast<float>(Module.Thermal().LogicTemperature());
}

//---------------------------------------------------------------------------
//  X6api::PllLocked() --  Query pll lock status
//---------------------------------------------------------------------------
bool X6api::PllLocked()
{
    return Module.Clock().Locked();
}

//---------------------------------------------------------------------------
//  X6api::write_wishbone_register() --  write wishbone register
//---------------------------------------------------------------------------
void X6api::write_wishbone_register(uint32_t baseAddr, uint32_t offset, uint32_t data)
{
	// Initialize WishboneAddress Space for APS specific firmware
	Innovative::AddressingSpace & logicMemory = Innovative::LogicMemorySpace(const_cast<X6_1000M&>(Module));
	Innovative::WishboneBusSpace WB_X6 = Innovative::WishboneBusSpace(logicMemory, baseAddr);
	//Register.Value is defined as an ii32 in HardwareRegister_Mb.cpp and ii32 is typedefed as unsigend in DataTypes_Mb.h
	Innovative::Register reg = Register(WB_X6, offset);
	reg.Value(data);
}

//---------------------------------------------------------------------------
//  X6api::read_wishbone_register() --  read wishbone register
//---------------------------------------------------------------------------
uint32_t X6api::read_wishbone_register(uint32_t baseAddr, uint32_t offset) const
{
	Innovative::AddressingSpace & logicMemory = Innovative::LogicMemorySpace(const_cast<X6_1000M&>(Module));
	Innovative::WishboneBusSpace WB_X6 = Innovative::WishboneBusSpace(logicMemory, baseAddr);
	//Register.Value is defined as an ii32 in HardwareRegister_Mb.cpp and ii32 is typedefed as unsigend in DataTypes_Mb.h
	Innovative::Register reg = Register(WB_X6, offset);
	return reg.Value();
}

//---------------------------------------------------------------------------
// X6api::WriteRom()
//---------------------------------------------------------------------------
void X6api::WriteRom()
{
    //  System Page Operations
    Module.IdRom().System().Name(Settings.ModuleName);
    Module.IdRom().System().Revision(Settings.ModuleRevision);
    Module.IdRom().System().Updated(true);  // T if data has been loaded

    Module.IdRom().System().StoreToRom();

    //  AdcCal Page Operations
    for (unsigned int ch = 0; ch < InputChannels(); ++ch)
        {
        Module.Input().Cal().Gain(ch, Settings.Rx.Gain[ch]);
        Module.Input().Cal().Offset(ch, Settings.Rx.Offset[ch]);
        }
    Module.Input().Cal().Calibrated(Settings.Rx.Calibrated);

    Module.IdRom().AdcCal().StoreToRom();
    //  DacCal Page Operations
    for (unsigned int ch = 0; ch < OutputChannels(); ++ch)
        {
        Module.Output().Cal().Gain(ch, Settings.Tx.Gain[ch]);
        Module.Output().Cal().Offset(ch, Settings.Tx.Offset[ch]);
        }
    Module.Output().Cal().Calibrated(Settings.Tx.Calibrated);

    Module.IdRom().DacCal().StoreToRom();
}

//---------------------------------------------------------------------------
// X6api::ReadRom()
//---------------------------------------------------------------------------
void X6api::ReadRom()
{
    //  System Page Operations
    Module.IdRom().System().LoadFromRom();

    Settings.ModuleName = Module.IdRom().System().Name();
    Settings.ModuleRevision = Module.IdRom().System().Revision();
    //  Can use 'Updated' to check if data valid
    
    Module.IdRom().AdcCal().LoadFromRom();
    for (unsigned int ch = 0; ch < InputChannels(); ++ch)
        {
        Settings.Rx.Gain[ch] = Module.Input().Cal().Gain(ch);
        Settings.Rx.Offset[ch] = Module.Input().Cal().Offset(ch);
        }

    Settings.Rx.Calibrated = Module.Input().Cal().Calibrated();

    // Dac
    Module.IdRom().DacCal().LoadFromRom();
    for (unsigned int ch = 0; ch < OutputChannels(); ++ch)
        {
        Settings.Tx.Gain[ch] = Module.Output().Cal().Gain(ch);
        Settings.Tx.Offset[ch] = Module.Output().Cal().Offset(ch);
        }

    Settings.Tx.Calibrated = Module.Output().Cal().Calibrated();

}

//---------------------------------------------------------------------------
//  X6api::DacInternalCal() --  Query DacInternalCal status
//---------------------------------------------------------------------------
bool X6api::DacInternalCal()
{
    return Module.Output().DacInternalCalibrationOk();
}

//------------------------------------------------------------------------------
//  X6api::ManualTrigger() --
//------------------------------------------------------------------------------
void  X6api::ManualTrigger(double state)
{
	Trig.SetActiveTrigger((bool)state);
}


//---------------------------------------------------------------------------
//  X6api::FpgaLogicVersion() --
//---------------------------------------------------------------------------
std::string X6api::FpgaLogicVersion()
{
	stringstream ss;
	ss << std::hex << Module.Info().FpgaLogicVersion() << " Subrev "
		<< FpgaLogicSubrevision();
	return  ss.str();
}

//---------------------------------------------------------------------------
//  X6api::FpgaHardwareVariant() --
//---------------------------------------------------------------------------
unsigned short X6api::FpgaHardwareVariant()
{
	return  Module.Info().FpgaHardwareVariant();
}

//---------------------------------------------------------------------------
//  X6api::PciLogicRevision() --
//---------------------------------------------------------------------------
unsigned short X6api::PciLogicRevision()
{
	return  Module.Info().PciLogicRevision();
}

//---------------------------------------------------------------------------
//  X6api::FpgaLogicSubrevision() --
//---------------------------------------------------------------------------
unsigned short X6api::FpgaLogicSubrevision()
{
	return  Module.Info().FpgaLogicSubrevision();
}

//---------------------------------------------------------------------------
//  X6api::PciLogicType() --
//---------------------------------------------------------------------------
std::string X6api::PciLogicType()
{
	return Module.BoardName(Module.Info().PciLogicType());
}

//---------------------------------------------------------------------------
//  X6api::PciLogicPcb() --
//---------------------------------------------------------------------------
std::string X6api::PciLogicPcb()
{
	char RevNumber = 'A' + static_cast<char>(Module.Info().PciLogicPcb());
	stringstream ss;
	ss << "Rev" << RevNumber;
	return ss.str();
}

//---------------------------------------------------------------------------
//  X6api::FpgaName() --
//---------------------------------------------------------------------------
std::string X6api::FpgaName()
{
	return Module.FpgaName(Module.Info().FpgaChipType());
}

//---------------------------------------------------------------------------
//  X6api::PCIExpressLanes() --
//---------------------------------------------------------------------------
std::string X6api::PCIExpressLanes()
{
	stringstream msg;
	msg << "PCI Express Lanes: " << Module.Debug()->LaneCount();
	if (Module.Debug()->IsGen2Capable())
		msg << " Gen 2";
	else
		msg << " Gen 1 only";

	return msg.str();
}

//---------------------------------------------------------------------------
//  X6api::BitStreamDate() --
//---------------------------------------------------------------------------
std::string X6api::BitStreamDate()
{
	return Module.Info().BitStreamDateString_YMD();
}

//---------------------------------------------------------------------------
//  X6api::BitStreamTime() --
//---------------------------------------------------------------------------
std::string X6api::BitStreamTime()
{
	return Module.Info().BitStreamDateString_HMS();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Configuration Event Handlers
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//---------------------------------------------------------------------------
//  X6api::HandleBeforeStreamStart() --  Pre-streaming init event
//---------------------------------------------------------------------------
void X6api::HandleBeforeStreamStart(OpenWire::NotifyEvent & /*Event*/)
{
}

//---------------------------------------------------------------------------
//  X6api::HandleAfterStreamStart() --  Post streaming init event
//---------------------------------------------------------------------------
void X6api::HandleAfterStreamStart(OpenWire::NotifyEvent & /*Event*/)
{
    Timer.Enabled(true);
}

//---------------------------------------------------------------------------
//  X6api::HandleAfterStreamStop() --  Post stream termination event
//---------------------------------------------------------------------------
void X6api::HandleAfterStreamStop(OpenWire::NotifyEvent & /*Event*/)
{
    // Disable external triggering initially
	Module.Input().SoftwareTrigger(false);
    Module.Input().Trigger().External(false);
	Module.Output().SoftwareTrigger(false);
    Module.Output().Trigger().External(false);
}

//---------------------------------------------------------------------------
//  X6api::HandleAfterStop() --  Post stream termination event
//---------------------------------------------------------------------------
void X6api::HandleAfterStop(OpenWire::NotifyEvent & /*Event*/)
{
}

//---------------------------------------------------------------------------
//  X6api::HandleTimer() --  Per-second status timer event
//---------------------------------------------------------------------------
void X6api::HandleTimer(OpenWire::NotifyEvent & /*Event*/)
{
    Trig.AtTimerTick();
}

//---------------------------------------------------------------------------
//  X6api::HandleDisableTrigger() --  Trigger Manager Trig OFF
//------------------------------------------------------------------------------
void  X6api::HandleDisableTrigger(OpenWire::NotifyEvent & /*Event*/)
{
    Module.Output().Trigger().External(false);
    Module.Input().Trigger().External(false);
}

//---------------------------------------------------------------------------
//  X6api::HandleSoftwareTrigger() -- 
//------------------------------------------------------------------------------
void  X6api::HandleSoftwareTrigger(OpenWire::NotifyEvent & /*Event*/)
{
	if (Settings.Tx.ExternalTrigger == 0)
		Module.Output().SoftwareTrigger(true);
    if (Settings.Rx.ExternalTrigger == 0) 
		Module.Input().SoftwareTrigger(true);
}

//---------------------------------------------------------------------------
//  X6api::HandleManualTrigger() --  
//------------------------------------------------------------------------------
void  X6api::HandleManualTrigger(ManualTriggerEvent & Event)
{
	Module.Output().SoftwareTrigger(Event.State);
	Module.Input().SoftwareTrigger(Event.State);
	if (!Event.State && !Settings.Tx.LoadedPatterns.empty())
	{
		PatternReplayCommand();
	}
}

//---------------------------------------------------------------------------
//  X6api::HandleExternalTrigger() --  Enable External trigger
//------------------------------------------------------------------------------
void  X6api::HandleExternalTrigger(OpenWire::NotifyEvent & /*Event*/)
{
    Module.Output().Trigger().External(true);
    Module.Input().Trigger().External(true);
}

//---------------------------------------------------------------------------
//  X6api::HookAlerts() --
//---------------------------------------------------------------------------
void  X6api::HookAlerts()
{
	// Output
	Module.Alerts().OnTimeStampRolloverAlert.SetEvent(this, &X6api::HandleTimestampRolloverAlert);
	Module.Alerts().OnSoftwareAlert.SetEvent(this, &X6api::HandleSoftwareAlert);
	Module.Alerts().OnWarningTemperature.SetEvent(this, &X6api::HandleWarningTempAlert);
	Module.Alerts().OnOutputUnderflow.SetEvent(this, &X6api::HandleOutputFifoUnderflowAlert);
	Module.Alerts().OnTrigger.SetEvent(this, &X6api::HandleTriggerAlert);
	Module.Alerts().OnOutputOverrange.SetEvent(this, &X6api::HandleOutputOverrangeAlert);
	Module.Alerts().OnPatternDoneAlert.SetEvent(this, &X6api::HandlePatternDoneAlert);
	// Input 
	Module.Alerts().OnInputOverflow.SetEvent(this, &X6api::HandleInputFifoOverrunAlert);
	Module.Alerts().OnInputOverrange.SetEvent(this, &X6api::HandleInputOverrangeAlert);
}
//---------------------------------------------------------------------------
//  Alert Handlers
//---------------------------------------------------------------------------
void  X6api::HandleTimestampRolloverAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleSoftwareAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleWarningTempAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleInputFifoOverrunAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleOutputFifoUnderflowAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleTriggerAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleInputOverrangeAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandleOutputOverrangeAlert(Innovative::AlertSignalEvent & event){}
void  X6api::HandlePatternDoneAlert(Innovative::AlertSignalEvent & event){}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ADC data output and DAC waveform data input
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//---------------------------------------------------------------------------
//  X6api::read_adc_data() --  read adc data
//---------------------------------------------------------------------------
void X6api::do_trigger(int trig_n)
{
	int idx = trig_n;
	while (idx)
	{
		ManualTrigger(0);
		Innovative::Sleep(1);
		ManualTrigger(1);
		idx--;
	}
}
vector<int> X6api::read_adc_data()
{
	return adc_data_;
}

//---------------------------------------------------------------------------
//  X6api::write_dac_wavedata() --
//---------------------------------------------------------------------------
void X6api::write_dac_wavedata(vector<double> wavedata)
{
	// clear wavedata_, in case there already has historical data in it
	wavedata_.clear();
	wavedata_.swap(vector<double>());
	// copy data into it
	wavedata_.swap(wavedata);
	
	// make sure wavedata size is multiple of output TriggerFrameGranularity
	int framesize = Module.Output().Info().TriggerFrameGranularity();
	int res = wavedata_.size() % framesize;
	if (res)
	{
		for (int idx = 0; idx < (framesize - res); idx++)
		{
			wavedata_.push_back(0);
		}
	}

	int channels = Module.Output().ActiveChannels();
	Settings.Tx.Pattern.SizeInEvents = wavedata_.size() / channels;
	Settings.Tx.FrameSize = wavedata_.size();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/*
void main()
{
	cout << "test start: " << "\n";

	vector<double> wavedata;
	for (int i = 0; i < 0x400; i++)
	{
		double res = double(i/4);
		wavedata.push_back(0.8);
	}
	for (int i = 0; i < 0x10000; i++)
	{
		double res = double(i / 4);
		wavedata.push_back(-0.2);
	}

	X6api x6;
	x6.Open(0);
	x6.StartStreaming();
	x6.EnterPatternMode();
	
	x6.write_dac_wavedata(wavedata);
	x6.PatternLoadCommand();

	//x6.write_wishbone_register(0x800, 0xd, 0x200);
	//x6.write_wishbone_register(0x800, 0xb, 0x00010005);

	Innovative::Sleep(10);
	int idx = 210;
	while (idx)
	{
		x6.ManualTrigger(0);
		Innovative::Sleep(1);
		x6.ManualTrigger(1);
		cout << idx << "\n";
		idx--;
	}
	x6.LeavePatternMode();

	vector<int> ADC_Data = x6.read_adc_data();
	ofstream out("output.txt");
	for (int idx = 0; idx<ADC_Data.size(); idx++)
	{
		out << ADC_Data[idx];
		if ((idx + 1) % (2048+16))
		{
			out << "  ";
		}
		else
		{
			out << "\n";
		}
	}
	out.close();
	ADC_Data.clear();
	ADC_Data.swap(vector<int>());

	cout << "temperature: " << "\n";
	cout << x6.Temperature() << "\n";
	x6.Close();
	cout << "this is the end \n";
}
*/