// This is the main cpp file for arbitrary waveform generation of x6_1000m api
// author: LiuQichun
// date: 2020-04-15

// arb_wf.cpp


#include <sstream>
#include <fstream>
#include <limits>
#include "arb_wf.h"
#include <IppCharDG_Mb.h>
#include <Poco/Random.h>

using namespace std;

namespace Innovative
{

	//=============================================================================
	//  CLASS ArbWaveform  --  Endpoint-disciplined waveform generator
	//=============================================================================

	ArbWaveform::ArbWaveform()
		: FBits(16),
		FChannels(1), Generator(new Poco::Random)
	{
	}

	ArbWaveform::~ArbWaveform()
	{
		if (Generator) delete Generator;
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Format()
	//------------------------------------------------------------------------

	void ArbWaveform::Format(int channels, int bits, int samples)
	{
		FSamples = samples;
		FChannels = channels;
		FBits = bits;

		DoubleDG Waveform(Data);

		Waveform.Resize(FSamples*FChannels);
		Zero();
		Generator->seed();
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Resize()
	//------------------------------------------------------------------------

	void ArbWaveform::Resize(Buffer & data)
	{
		if (FBits <= 8)
			data.Resize(Holding<char>(FSamples*FChannels));
		else if (FBits <= 16)
			data.Resize(Holding<short>(FSamples*FChannels));
		else
			data.Resize(Holding<int>(FSamples*FChannels));
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Zero()
	//------------------------------------------------------------------------

	void ArbWaveform::Zero(int ch)
	{
		DoubleDG Waveform(Data);

		for (unsigned int n = 0; n < FSamples; ++n)
			Waveform[n*FChannels + ch] = 0.;
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Zero()
	//------------------------------------------------------------------------

	void ArbWaveform::Zero()
	{
		for (unsigned int ch = 0; ch < FChannels; ++ch)
			Zero(ch);
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Apply() -- Copy data out to buffer
	//------------------------------------------------------------------------

	void ArbWaveform::Apply(Buffer & data)
	{
		for (unsigned int ch = 0; ch < FChannels; ++ch)
		{
			Copy(data, ch);
		}
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Accumulate_ch()
	//------------------------------------------------------------------------

	bool ArbWaveform::Accumulate_ch(size_t deviceid, int ch)
	{
		DoubleDG Waveform(Data);

		// Amplitude.
		double FScale = (1 << (FBits - 1)) - 1;
		double A = FScale * 0.95;

		int start = deviceid + ch;
		int step = 2*FChannels;
		for (unsigned int n = 0; n < FSamples; ++n)
		{
			double sample = A*wavedata[n*step + start];
			Waveform[n*FChannels + ch] += sample;
		}
		//Normalize(ch);
		return true;
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Accumulate()
	//------------------------------------------------------------------------

	bool ArbWaveform::Accumulate(size_t deviceid)
	{
		bool result = true;
		for (unsigned int ch = 0; ch < FChannels; ++ch)
			result &= Accumulate_ch(deviceid, ch);

		return result;
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Copy()
	//------------------------------------------------------------------------

	void ArbWaveform::Copy(Buffer & data, int ch)
	{
		DoubleDG Waveform(Data);
		// Samples per buffer.

		if (FBits <= 8)
		{
			CharDG dg(data);
			// Populate buffer.
			for (unsigned int n = 0; n < FSamples; ++n)
			{
				int idx = n*FChannels + ch;
				char v = static_cast<char>(Waveform[idx]);
				dg[idx] = v;
			}
		}
		else if (FBits <= 16)
		{
			ShortDG dg(data);
			// Populate buffer.
			for (unsigned int n = 0; n < FSamples; ++n)
			{
				int idx = n*FChannels + ch;
				short v = static_cast<short>(Waveform[idx]);
				dg[idx] = v;
			}
		}
		else
		{
			IntegerDG dg(data);
			// Populate buffer.
			for (unsigned int n = 0; n < FSamples; ++n)
			{
				int idx = n*FChannels + ch;
				int v = static_cast<int>(Waveform[idx]);
				dg[idx] = v;
			}
		}
	}

	//------------------------------------------------------------------------
	// ArbWaveform::Normalize()
	//------------------------------------------------------------------------

	bool ArbWaveform::Normalize(int ch)
	{
		DoubleDG Waveform(Data);

		// Add waveform elements
		double maximum = (std::numeric_limits<double>::min)();
		for (unsigned int n = 0; n < FSamples; ++n)
		{
			int idx = n*FChannels + ch;
			maximum = (std::max)(Waveform[idx], maximum);
		}

		// Normalize
		double A = ((1 << (FBits - 1)) - 1) * 0.95;
		double scale = A / maximum;
		for (unsigned int n = 0; n < FSamples; ++n)
		{
			int idx = n*FChannels + ch;
			Waveform[idx] *= scale;
		}

		return true;
	}

	//==============================================================================
	//  CLASS WaveGenerator
	//==============================================================================
	//---------------------------------------------------------------------------
	//  DualWaveGenerator::SingleWave() -- Calculate single wave channel
	//---------------------------------------------------------------------------
	/*
	bool  WaveGenerator::SingleWave(Buffer & data, int ch)
	{
		Gen.Zero(ch);
		bool rtn = Gen.Accumulate(ch);
		if (rtn)
			Gen.Apply(data);
		return rtn;
	}
	*/
	//---------------------------------------------------------------------------
	//  DualWaveGenerator::SingleWave() -- Calculate single wave channel
	//---------------------------------------------------------------------------

	bool  WaveGenerator::SingleWave(size_t deviceid, Buffer & data)
	{
		Gen.Zero();
		bool rtn = Gen.Accumulate(deviceid);
		if (rtn)
			Gen.Apply(data);
		return rtn;
	}

	//==============================================================================
	//  CLASS ArbWaveBuilder -- Class to manage X6 Wave Building Setup
	//==============================================================================
	//------------------------------------------------------------------------------
	//  ArbWaveBuilder::BuildWave() -- Fill Buffer(s) with a waveform
	//------------------------------------------------------------------------------

	void ArbWaveBuilder::BuildWave(VeloBuffer & Buffer)
	{
		//  Fill scratch buffers
		CreateScratchBuffers();

		//  Fill scratch buffers
		GenerateWave();

		//  Create Output Playback Buffer
		FillOutputWaveBuffer(Buffer);
	}

	//------------------------------------------------------------------------------
	//  ArbWaveBuilder::CreateScratchBuffers() --
	//------------------------------------------------------------------------------

	void  ArbWaveBuilder::CreateScratchBuffers()
	{
		//  Each scratch buffer holds total_channels/total_devices channels
		size_t devices = FDeviceSids.size();
		size_t scratch_buffer_size = CalculateScratchBufferSize();

		Scratch.empty();
		Scratch.resize(devices);

		for (unsigned int i = 0; i<Scratch.size(); i++)
			Scratch[i].Resize(scratch_buffer_size);
	}

	//------------------------------------------------------------------------------
	//  ArbWaveBuilder::GenerateWave() --
	//------------------------------------------------------------------------------

	void  ArbWaveBuilder::GenerateWave()
	{
		size_t devices = FDeviceSids.size();
		size_t channels = (devices) ? FChannels / devices : FChannels;
		for (size_t i = 0; i<devices; i++)
		{
			WaveGen.Gen.Format((int)channels, FBits, FSamples);
			WaveGen.SingleWave(i, Scratch[i]);   //  One Wave on all channels
		}
	}

	//------------------------------------------------------------------------------
	//  ArbWaveBuilder::FillOutputWaveBuffer() --
	//------------------------------------------------------------------------------

	void  ArbWaveBuilder::FillOutputWaveBuffer(VeloBuffer & OutputWaveform)
	{
		//  We now have the data in an array of scratch buffers.
		//     We need to copy it into VITA packets, and then pack those VITAs
		//     into the outbound Waveform packet
		size_t words_remaining = Scratch[0].SizeInInts();
		//
		//  Use a packer to load full VITA packets into a velo packet
		//  ...Make the packer output size so big, we will not fill it before finishing
		VitaPacketPacker VPPk(words_remaining*Scratch.size() * 2);
		VPPk.OnDataAvailable.SetEvent(this, &ArbWaveBuilder::HandlePackedDataAvailable);
		//
		//  Bust up the scratch buffer into VITA packets
		size_t offset = 0;
		size_t packet = 0;
		size_t MaxVitaSize = 0x100000;
		while (words_remaining)
		{
			//  calculate size of VITA packet
			size_t VP_size = min(MaxVitaSize, words_remaining);

			//  Get a properly cleared/inited Vita Header
			VitaBuffer VBuf(VP_size);
			ClearHeader(VBuf);
			ClearTrailer(VBuf);
			InitHeader(VBuf);
			InitTrailer(VBuf);

			Innovative::UIntegerDG   VitaDG(VBuf);

			for (unsigned int stream = 0; stream<Scratch.size(); stream++)
			{
				Innovative::UIntegerDG   ScratchDG(Scratch[stream]);

				//  Copy Data to Vita
				for (unsigned int idx = 0; idx<VitaDG.size(); idx++)
					VitaDG[idx] = ScratchDG[offset + idx];

				//  Init Vita Header
				VitaHeaderDatagram VitaH(VBuf);
				VitaH.StreamId(FDeviceSids[stream]);
				VitaH.PacketCount(static_cast<int>(packet));

				//  Shove in the new VITA packet
				VPPk.Pack(VBuf);
			}
			//  fix up buffer counters
			words_remaining -= VP_size;
			offset += VP_size;

			packet++;
		}

		VPPk.Flush();   // outputs the one waveform buffer into WaveformPacket

		ClearHeader(WaveformPacket);
		InitHeader(WaveformPacket);   // make sure header packet size is valid...

									  // WaveformPacket is now correctly filled with data...

		OutputWaveform = WaveformPacket;   // "return" the waveform

	}
	//---------------------------------------------------------------------------
	//  ArbWaveBuilder::HandlePackedDataAvailable() -- Packer Callback
	//---------------------------------------------------------------------------

	void  ArbWaveBuilder::HandlePackedDataAvailable(Innovative::VitaPacketPackerDataAvailable & event)
	{
		WaveformPacket = event.Data;
	}

	//------------------------------------------------------------------------------
	//  ArbWaveBuilder::CalculateScratchBufferSize() --
	//------------------------------------------------------------------------------

	size_t ArbWaveBuilder::CalculateScratchBufferSize()
	{
		//  Each scratch buffer holds total_channels/total_devices channels
		size_t devices = FDeviceSids.size();
		size_t channels = (devices) ? FChannels / devices : FChannels;

		// calculate scratch packet size in ints
		size_t scratch_pkt_size;
		if (FBits <= 8)
			scratch_pkt_size = channels * Holding<char>(FSamples);
		else if (FBits <= 16)
			scratch_pkt_size = channels * Holding<short>(FSamples);
		else
			scratch_pkt_size = channels * FSamples;

		return scratch_pkt_size;
	}

	//------------------------------------------------------------------------------
	//  ArbWaveBuilder::set_wavedata() --
	//------------------------------------------------------------------------------
	void ArbWaveBuilder::set_wavedata(vector<double> wavedata)
	{
		WaveGen.Gen.wavedata = wavedata;
	}


} // namespace