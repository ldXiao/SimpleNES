#ifndef APU_H
#define APU_H
#include "CPUOpcodes.h"
#include "MainBus.h"

namespace sn
{
	class AudioStream;
	class ChannelBase
	{
		public:
			virtual size_t getValue()=0;
			virtual void getStates(MainBus& mem) =0;
			virtual void init()=0;
	};

// Produces the square wave based on one of 4 duty cycles
// http://wiki.nesdev.com/w/index.php/APU_Pulse
	class PulseWaveGenerator
	{
	public:
		PulseWaveGenerator() : m_duty(0), m_step(0) {}

		void Restart()
		{
			m_step = 0;
		}

		void SetDuty(uint8 duty)
		{
			assert(duty < 4);
			m_duty = duty;
		}

		// Clocked by an Timer, outputs bit (0 or 1)
		void Clock()
		{
			m_step = (m_step + 1) % 8;
		}

		size_t GetValue() const
		{
			static uint8 sequences[4][8] =
			{
				{ 0, 1, 0, 0, 0, 0, 0, 0 }, // 12.5%
				{ 0, 1, 1, 0, 0, 0, 0, 0 }, // 25%
				{ 0, 1, 1, 1, 1, 0, 0, 0 }, // 50%
				{ 1, 0, 0, 1, 1, 1, 1, 1 }  // 25% negated
			};

			const uint8 value = sequences[m_duty][m_step];
			return value;
		}

	private:
		uint8 m_duty; // 2 bits
		uint8 m_step; // 0-7
	};

	class ChannelPulse0
		: public ChannelBase
	{
		public:
			size_t getValue() override;
			void getStates(MainBus& mem) override;
			void init() override;
		private:
			Byte r_4000;
			Byte r_4001;
			Byte r_4002;
			Byte r_4003;


	};

	class ChannelPulse1
	{

	};

	class ChannelTriangle

	{

	};

	class ChannelNoise
	{

	};

	class ChannelDMC
	{

	};

	class APU {
		public:
			APU(MainBus &mem);
			void step();
			void reset();
			float sampleAndMix();
		private:
			MainBus &m_bus;
			std::unique_ptr<AudioStream> m_stream;
			std::unique_ptr<ChannelPulse0> m_channelPulse0;
			std::unique_ptr<ChannelPulse1> m_channelPulse1;
			std::unique_ptr<ChannelTriangle> m_channelTriangle;
			std::unique_ptr<ChannelNoise> m_channelNoise;
			std::unique_ptr<ChannelDMC> m_channelDMC;


	};
}
#endif // APU_H