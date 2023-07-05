#ifndef APU_H
#define APU_H
#include "CPUOpcodes.h"
#include "MainBus.h"
#include <cstdint>

namespace sn
{
class AudioStream;
// Divider outputs a clock periodically.
// Note that the term 'period' in this code really means 'period reload value', P,
// where the actual output clock period is P + 1.
// Simply a counter
// used in Envelope and SweepUnit
class Divider
{
public:
	Divider() : m_period(0), m_counter(0) {}

	size_t GetPeriod() const { return m_period; }
	size_t GetCounter() const { return m_counter;  }

	void SetPeriod(size_t period)
	{
		m_period = period;
	}

	void ResetCounter()
	{
		m_counter = m_period;
	}

	bool Clock()
	{
		// We count down from P to 0 inclusive, clocking out every P + 1 input clocks.
		if (m_counter-- == 0)
		{
			ResetCounter();
			return true;
		}
		return false;
	}

private:
	size_t m_period;
	size_t m_counter;
};

// The length counter provides automatic duration control for the NES APU waveform channels.
// Once loaded with a value, it can optionally count down (when the length counter halt flag is clear).
// Once it reaches zero, the corresponding channel is silenced.
// Input: Clocked by Frame Sequencer
// Output: Low frequency counter
// http://wiki.nesdev.com/w/index.php/APU_Length_Counter
class LengthCounter
{
public:
	LengthCounter() : m_enabled(false), m_halt(false), m_counter(0) {}

	void SetEnabled(bool enabled)
	{
		m_enabled = enabled;

		// Disabling resets counter to 0, and it stays that way until enabled again
		if (!m_enabled)
			m_counter = 0;
	}

	void SetHalt(bool halt)
	{
		m_halt = halt;
	}

	void LoadCounterFromLengthTable(std::uint8_t index)
	{
		if (!m_enabled)
			return;

		static std::vector<std::uint8_t> lengthTable =
		{
			10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
			12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
		};
		// static_assert(lengthTable.size() == 32, "Invalid");
		assert(index < lengthTable.size());

		m_counter = lengthTable[index];
	}

	// Clocked by Frame Sequencer
	void Clock()
	{
		if (m_halt) // Halting locks counter at current value
			return;

		if (m_counter > 0) // Once it reaches 0, it stops, and channel is silenced
			--m_counter;
	}

	size_t GetValue() const
	{
		return m_counter;
	}

	bool ChennelSilenced() const
	{
		return m_counter == 0;
	}

private:
	bool m_enabled;
	bool m_halt;
	size_t m_counter;
};

// Controls volume in 2 ways: decreasing saw with optional looping, or constant volume
// Input: Clocked by Frame Sequencer
// Output: 4-bit volume value (0-15)
// used in Pualse and Noise Channel
// http://wiki.nesdev.com/w/index.php/APU_Envelope
class VolumeEnvelope
{
public:
	VolumeEnvelope()
		: m_restart(true)
		, m_loop(false)
		, m_counter(0)
		, m_constantVolumeMode(false)
		, m_constantVolume(0)
	{
	}

	void Restart() { m_restart = true; }
	void SetLoop(bool loop) { m_loop = loop;  }
	void SetConstantVolumeMode(bool mode) { m_constantVolumeMode = mode; }

	void SetConstantVolume(std::uint16_t value)
	{
		assert(value < 16);
		m_constantVolume = value;
		m_divider.SetPeriod(m_constantVolume); // Constant volume doubles up as divider reload value
	}

	size_t GetVolume() const
	{
		size_t result = m_constantVolumeMode ? m_constantVolume : m_counter;
		assert(result < 16);
		return result;
	}

	// Clocked by FrameCounter
	void Clock()
	{
		if (m_restart)
		{
			m_restart = false;
			m_counter = 15;
			m_divider.ResetCounter();
		}
		else
		{
			if (m_divider.Clock())
			{
				if (m_counter > 0)
				{
					--m_counter;
				}
				else if (m_loop)
				{
					m_counter = 15;
				}
			}
		}
	}
private:

	bool m_restart;
	bool m_loop;
	Divider m_divider;
	size_t m_counter; // decress envelope volume value (if not constant volume mode)
	bool m_constantVolumeMode;
	size_t m_constantVolume; // Also reload value for divider
};

// A timer is used in each of the five channels to control the sound frequency. It contains a divider which is
// clocked by the CPU clock. The triangle channel's timer is clocked on every CPU cycle, but the pulse, noise,
// and DMC timers are clocked only on every second CPU cycle and thus produce only even periods.
// The input is a 11bit number for the divider period
// http://wiki.nesdev.com/w/index.php/APU_Misc#Glossary
class Timer
{
public:
	Timer() : m_minPeriod(0) {}

	void Reset()
	{
		m_divider.ResetCounter();
	}

	size_t GetPeriod() const { return m_divider.GetPeriod(); }

	void SetPeriod(size_t period)
	{
		m_divider.SetPeriod(period);
	}

	void SetPeriodLow8(std::uint8_t value)
	{
		size_t period = m_divider.GetPeriod();
		period = (period & 0x700) | value; // Keep high 3 bits, 0x700 == 0b11100000000
		SetPeriod(period);
	}

	void SetPeriodHigh3(size_t value)
	{
		assert(value <= 0b111); // 0b111 largest
		size_t period = m_divider.GetPeriod();
		period = (value << 8) | (period & 0xFF); // Keep low 8 bits
		m_divider.SetPeriod(period);

		m_divider.ResetCounter();
	}

	void SetMinPeriod(size_t minPeriod)
	{
		m_minPeriod = minPeriod;
	}

	// Clocked by CPU clock every cycle (triangle channel) or second cycle (pulse/noise channels)
	// Returns true when output chip should be clocked
	bool Clock()
	{
		// Avoid popping and weird noises from ultra sonic frequencies
		if (m_divider.GetPeriod() < m_minPeriod)
			return false;

		if (m_divider.Clock())
		{
			return true;
		}
		return false;
	}

private:

	Divider m_divider;
	size_t m_minPeriod;
};

// Periodically adjusts the period of the Timer, sweeping the frequency high or low over time
// used in Pulse Channels
// http://wiki.nesdev.com/w/index.php/APU_Sweep
class SweepUnit
{
public:
	SweepUnit()
		: m_subtractExtra(0)
		, m_enabled(false)
		, m_negate(false)
		, m_reload(false)
		, m_silenceChannel(false)
		, m_shiftCount(0)
		, m_targetPeriod(0)
	{
	}

	void SetSubtractExtra()
	{
		m_subtractExtra = 1;
	}

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	void SetNegate(bool negate) { m_negate = negate; }

	void SetPeriod(size_t period, Timer& timer)
	{
		assert(period <= 0b111); // 3 bits
		m_divider.SetPeriod(period); // Don't reset counter

		// From wiki: The adder computes the next target period immediately after the period is updated by $400x writes
		// or by the frame counter.
		ComputeTargetPeriod(timer);
	}

	void SetShiftCount(std::uint8_t shiftCount)
	{
		assert(shiftCount <= 0b111);
		m_shiftCount = shiftCount;
	}

	void Restart() { m_reload = true;  }

	// Clocked by FrameCounter
	// adjust a timer
	void Clock(Timer& timer)
	{
		ComputeTargetPeriod(timer);

		if (m_reload)
		{
			// From nesdev wiki https://www.nesdev.org/wiki/APU_Sweep
			//"If the divider's counter is zero, the sweep is enabled, and the sweep unit is not muting the channel: The pulse's period is set to the target period.
			// If the divider's counter is zero or the reload flag is true: The divider counter is set to P and the reload flag is cleared. Otherwise, the divider counter is decremented.
			// To keep a the logic simple, muting/silence is handled in the function AdjusTimerPeriod
			if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod(timer);
			}

			m_divider.ResetCounter();

			m_reload = false;
		}
		else
		{
			// From the nesdev wiki, it looks like the divider is always decremented, but only
			// reset to its period if the sweep is enabled.
			if (m_divider.GetCounter() > 0)
			{
				m_divider.Clock();
			}
			else if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod(timer);
			}

		}
	}

	bool SilenceChannel() const
	{
		return m_silenceChannel;
	}

private:
	void ComputeTargetPeriod(Timer& timer)
	{
		assert(m_shiftCount <= 0b111); // 3 bits

		const size_t currPeriod = timer.GetPeriod();
		const size_t shiftedPeriod = currPeriod >> m_shiftCount;

		if (m_negate)
		{
			// Pulse 1's adder's carry is hardwired, so the subtraction adds the one's complement
			// instead of the expected two's complement (as pulse 2 does)
			m_targetPeriod = currPeriod - (shiftedPeriod - m_subtractExtra);
		}
		else
		{
			m_targetPeriod = currPeriod + shiftedPeriod;
		}

		// Channel will be silenced under certain conditions even if Sweep unit is disabled
		// to avoid ultra high frequence sound
		m_silenceChannel = (currPeriod < 8 || m_targetPeriod > 0x7FF);
	}

	void AdjustTimerPeriod(Timer& timer)
	{
		// If channel is not silenced, it means we're in range
		if (m_enabled && m_shiftCount > 0 && !m_silenceChannel)
		{
			timer.SetPeriod(m_targetPeriod);
		}
	}

private:

	size_t m_subtractExtra;
	bool m_enabled;
	bool m_negate;
	bool m_reload;
	bool m_silenceChannel; // This is the Sweep -> Gate connection, if true channel is silenced
	std::uint8_t m_shiftCount; // [0,7]
	Divider m_divider;
	size_t m_targetPeriod; // Target period for the timer; is computed continuously in real hardware
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

		void SetDuty(uint8_t duty)
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
			static std::uint8_t sequences[4][8] =
			{
				{ 0, 1, 0, 0, 0, 0, 0, 0 }, // 12.5%
				{ 0, 1, 1, 0, 0, 0, 0, 0 }, // 25%
				{ 0, 1, 1, 1, 1, 0, 0, 0 }, // 50%
				{ 1, 0, 0, 1, 1, 1, 1, 1 }  // 25% negated
			};

			const std::uint8_t value = sequences[m_duty][m_step];
			return value;
		}

	private:
		std::uint8_t m_duty; // 2 bits
		std::uint8_t m_step; // 0-7
	};



class ChannelBase
{
public:
	virtual size_t getValue()=0;
	virtual void init()=0;
	virtual void ClockTimer() = 0;
protected:
	Timer m_timer;
	LengthCounter m_lengthCounter;

};

class ChannelPulse0
	: public ChannelBase
{
	public:
		ChannelPulse0(){};
		size_t getValue() override;
		void init() override;
		void ClockTimer() override;
	private:
		// registers
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
		APU();
		~APU();
		void handleWrite(sn::IORegisters reg, Byte data);
		void step(); // tik the clocks of each channel
		void reset(); // reset each channel
		float sampleAndMix();
		std::unique_ptr<AudioStream> m_stream;
	private:
		std::unique_ptr<ChannelPulse0> m_channelPulse0;
		std::unique_ptr<ChannelPulse1> m_channelPulse1;
		std::unique_ptr<ChannelTriangle> m_channelTriangle;
		std::unique_ptr<ChannelNoise> m_channelNoise;
		std::unique_ptr<ChannelDMC> m_channelDMC;
		size_t m_cycles;
		size_t m_time;
		size_t m_numSamples;
		float m_sampleSum;


};
}
#endif // APU_H