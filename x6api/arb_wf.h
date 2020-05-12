// This is the main header file for arbitrary waveform generation of x6_1000m api
// author: LiuQichun
// date: 2020-04-15

// arb_wf.h

#ifndef arb_wfH
#define arb_wfH

#include <Buffer_Mb.h>
#include <BufferDatagrams_Mb.h>
#include <VitaPacketStream_Mb.h>


// Forward declaration
namespace Poco {
	class Random;
}

namespace Innovative
{
#ifdef __CLR_VER
#pragma managed(push, off)
#endif
	//==============================================================================
	//  CLASS  ArbWaveform
	//==============================================================================

	class ArbWaveform
	{
	public:
		vector<double> wavedata;

		// Ctor
		ArbWaveform();
		~ArbWaveform();

		// Methods
		void        Format(int channels, int bits, int samples);
		void        Resize(Buffer & data);
		void        Zero(int ch);
		void        Zero();
		bool        Accumulate_ch(size_t deviceid, int ch);
		bool        Accumulate(size_t deviceid);
		void        Apply(Buffer & data);

	protected:
		// Fields
		unsigned int    FSamples;
		unsigned int    FBits;
		unsigned int    FChannels;

		// Data
		Poco::Random   *Generator;
		Buffer          Data;

		bool Normalize(int ch);
		void Copy(Buffer & data, int ch);
	//private:
	//	// No copy or assignment
	//	ArbWaveform(const ArbWaveform &);
	//	ArbWaveform &operator=(const ArbWaveform &);
	};

	//==============================================================================
	//  CLASS DualWaveGenerator
	//==============================================================================

	class WaveGenerator
	{
	public:
		ArbWaveform   Gen;
		WaveGenerator(){}
		//bool        SingleWave(Buffer & data, int ch);
		bool        SingleWave(size_t deviceid, Buffer & data);
	};

	//==============================================================================
	//  CLASS ArbWaveBuilder -- Class to manage Wave Building for X6 Boards
	//==============================================================================

	class ArbWaveBuilder
	{
	public:
		ArbWaveBuilder(){}

		void set_wavedata(vector<double> wavedata);
		//
		//  Properties
		void Format(const std::vector<int> & device_sids, int channels, int bits, int samples)
		{
			FDeviceSids = device_sids;
			FChannels = channels;
			FBits = bits;
			FSamples = samples;
		}
		//
		//  Methods
		void  BuildWave(VeloBuffer & Buffer);

	private:
		//
		//  Member Data
		unsigned int            FSamples;
		double                  FSampleRate;
		unsigned int            FBits;
		unsigned int            FChannels;
		std::vector<int>        FDeviceSids;

		WaveGenerator    WaveGen;
		std::vector<Buffer>  Scratch;
		VeloBuffer           WaveformPacket;

		void    CreateScratchBuffers();
		void    GenerateWave();
		void    FillOutputWaveBuffer(VeloBuffer & OutputWaveform);

		size_t  CalculateScratchBufferSize();

		void    HandlePackedDataAvailable(VitaPacketPackerDataAvailable & event);
	};

#ifdef __CLR_VER
#pragma managed(pop)
#endif
} // namespace Innovative

#endif
