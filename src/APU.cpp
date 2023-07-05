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
	return 0;

}


void ChannelPulse0::init()
{

}

void ChannelPulse0::ClockTimer()
{

}


APU::APU()
{
	m_stream = std::make_unique<AudioStream>();
	m_channelPulse0 = std::make_unique<ChannelPulse0>();
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
	return;
}

// return a float number between 0 and 1
float APU::sampleAndMix()
{
	// TODO Lind get samples from each channel
	return sound::SineWave(m_time, 400, 0.9);
}

void APU::step()
{
	// TODO tik the clock of each channel
	const float kAvgNumScreenPpuCycles = 89342 - 0.5; // 1 less every odd frame when rendering is enabled
	const float kCpuCyclesPerSec = (kAvgNumScreenPpuCycles / 3) * 60.0;
	const float kCpuCyclesPerSample = kCpuCyclesPerSec / (float) m_stream->getSampleRate();
	m_sampleSum += sampleAndMix();
	++m_numSamples;

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