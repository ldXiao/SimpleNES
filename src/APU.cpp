#include "APU.h"
#include "AudioStream.h"


namespace sound {
	#define TWOPI 6.283185307

	float SineWave(double time, double freq, double amp) {
		short result;
		double tpc = 44100 / freq; // ticks per cycle
		double cycles = time / tpc;
		double rad = TWOPI * cycles;
		short amplitude = 32767 * amp;
		result = amplitude * sin(rad);
		return (float)std::abs(sin(rad));
	}
}

namespace sn
{

size_t ChannelPulse0::getValue()
{
	if (m_sweepUnit.SilenceChannel())
		return 0;

	if (m_lengthCounter.ChennelSilenced())
		return 0;

	auto value = m_volumeEnvelope.GetVolume() * m_pulseWaveGenerator.GetValue();

	assert(value < 16);
	return value;
}


void ChannelPulse0::init()
{

}

void ChannelPulse0::ClockTimer()
{
	if (m_timer.Clock())
	{
		m_pulseWaveGenerator.Clock();
	}
}

void ChannelPulse0::handleWrite(IORegisters reg, Byte data)
{
	switch (reg)
	{
	case APUPULSE0_0: // duty, volume, halt, loop setup
	{
		Byte duty = (data & 0b11000000) >> 6;
		m_pulseWaveGenerator.SetDuty(duty);
		bool halt = (data & 0b00100000) != 0;
		m_lengthCounter.SetHalt(halt);
		bool loop = halt;
		m_volumeEnvelope.SetLoop(loop); // Same bit for length counter halt and envelope loop
		bool constVolume = (data & 0b00010000) != 0;
		m_volumeEnvelope.SetConstantVolumeMode(constVolume);
		uint16_t volume = data & 0b00001111;
		m_volumeEnvelope.SetConstantVolume(volume);
		break;
	}
	case APUPULSE0_1: // sweep unit setup
	{
		bool enabled = (data & 0b10000000) != 0;
		m_sweepUnit.SetEnabled(enabled);
		size_t period = (data & 0b01110000) >> 4;
		m_sweepUnit.SetPeriod(period, m_timer);
		bool negate = (data & 0b00001000) != 0;
		m_sweepUnit.SetNegate(negate);
		uint8_t shift = data & 0b00000111;
		m_sweepUnit.SetShiftCount(shift);
		m_sweepUnit.Restart(); // Side effect
		break;
	}
	case APUPULSE0_2:
	{
		m_timer.SetPeriodLow8(data);
		break;
	}
	case APUPULSE0_3:
	{
		Byte periodHigh3 = data & 0b00000111;
		m_timer.SetPeriodHigh3(periodHigh3);
		Byte counterIndex = (data & 0b11111000) >> 3;
		m_lengthCounter.LoadCounterFromLengthTable(counterIndex);

		// Side effects...
		m_volumeEnvelope.Restart();
		m_pulseWaveGenerator.Restart(); //@TODO: for pulse channels the phase is reset - IS THIS RIGHT?
		break;
	}
	default:
	{
		break;
	}
	}
}

void ChannelPulse0::ClockHalfFrame()
{
	m_lengthCounter.Clock();
	m_sweepUnit.Clock(m_timer);
}

void ChannelPulse0::ClockQuarterFrame()
{
	m_volumeEnvelope.Clock();

}


APU::APU()
{
	m_stream = std::make_unique<AudioStream>();
	m_channelPulse0 = std::make_unique<ChannelPulse0>();
	m_frameCounter = std::make_unique<FrameCounter>(*m_channelPulse0);
	m_sampleSum = 0;
	m_cycles = 0;
	m_numSamples = 0;
	m_time = 0;
}

APU::~APU()
{

}

void APU::handleWrite(IORegisters reg, Byte data)
{
	// TODO Lind dispatch the command to each channel/ or frame counter
	switch (reg)
	{
	case APUPULSE0_0:
	case APUPULSE0_1:
	case APUPULSE0_2:
	case APUPULSE0_3:
		m_channelPulse0->handleWrite(reg, data);
		break;
	case APUCTRL:
	{
		// TODO Lind enable more channels
		bool enablePulseChannel0 = (data & 0b00000001 ) != 0;
		m_channelPulse0->GetLengthCounter().SetEnabled(enablePulseChannel0);
		break;
	}
	default:
		break;
	}
	return;
}

// return a float number between 0 and 1
float APU::sampleAndMix()
{
	// TODO Lind get samples from each channel
	// return sound::SineWave(m_time, 400, 0.9);
	const size_t pulse1 = static_cast<size_t>(m_channelPulse0->getValue());

	// Lookup Table (accurate)
	static float pulseTable[31] = { ~0 };
	if (pulseTable[0] == ~0)
	{
		for (size_t i = 0; i < 31; ++i)
		{
			pulseTable[i] = 95.52f / (8128.0f / i + 100.0f);
		}
	}
	static float tndTable[203] = { ~0 };
	if (tndTable[0] == ~0)
	{
		for (size_t i = 0; i < 203; ++i)
		{
			tndTable[i] = 163.67f / (24329.0f / i + 100.0f);
		}
	}
	const float pulseOut = pulseTable[pulse1];
	float sample  = pulseOut;
	return 	sample;


}

void APU::step()
{
	// TODO tik the clock of each channel
	const float kAvgNumScreenPpuCycles = 89342 - 0.5; // 1 less every odd frame when rendering is enabled
	const float kCpuCyclesPerSec = (kAvgNumScreenPpuCycles / 3) * 60.0;
	const float kCpuCyclesPerSample = kCpuCyclesPerSec / (float) m_stream->getSampleRate();
	m_sampleSum += sampleAndMix();
	++m_numSamples;
	m_frameCounter->Clock();
	m_channelPulse0->ClockTimer(); // TODO add even predicate
	if (++m_cycles >= kCpuCyclesPerSample)
	{
		m_cycles -= kCpuCyclesPerSample;
		const float sample = m_sampleSum / m_numSamples;
		m_sampleSum = m_numSamples = 0;
		m_time ++;
		assert(sample >= 0.0f && sample <= 1.0f);
		float targetSample = sample * std::numeric_limits<std::int16_t>::max();
		// Lock guard inside addSample method
		m_stream->addSample(static_cast<std::int16_t>(targetSample));
	}
}

}