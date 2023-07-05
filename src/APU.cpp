#include "APU.h"
#include "AudioStream.h"

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
	return 0.0f;
}

void APU::step()
{
	// TODO tik the clock of each channel

	float sample = sampleAndMix();
	assert(sample >= 0.0f && sample <= 1.0f);
	float targetSample = sample * std::numeric_limits<std::int16_t>::max();

	// Lock guard inside addSample method
	m_stream->addSample(static_cast<std::int16_t>(targetSample));
}

}