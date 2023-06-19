#include "AudioStream.h"
#include <iostream>

namespace sn
{

AudioStream::AudioStream(size_t channelNum, size_t sampleRate)
{
	initialize(channelNum, sampleRate);
}

AudioStream::~AudioStream()
{

}

bool AudioStream::addSample(std::int16_t sample)
{
	const std::lock_guard<std::mutex> lock(mtx);
	if(m_samples.size()< m_maxsize)
	{
		// std::cout << "add" << sample << std::endl;
		m_samples.push_back(sample);
		return true;
	}
	else if(m_samples.size()==m_maxsize && m_offset > 0)
	{
		m_samples.assign(m_samples.begin() + static_cast<std::vector<std::int16_t>::difference_type>(m_offset), m_samples.end());
		m_samples.push_back(sample);
		m_offset = 0;
		return  true;
	}
	return false;
}

void AudioStream::onSeek(sf::Time timeOffset)
{
	const std::lock_guard<std::mutex> lock(mtx);
	std::cout << "called seek" << m_offset <<  std::endl;
	m_offset = static_cast<std::size_t>(timeOffset.asMilliseconds()) * getSampleRate() * getChannelCount() / 1000;
}

bool AudioStream::onGetData(Chunk& data)
{
	const std::lock_guard<std::mutex> lock(mtx);
	// std::cout << "called get" << m_samples.size() <<  std::endl;
	if(m_offset >= m_samples.size()){
		std::cout << "failed get" << m_offset <<  std::endl;
		// return false;
		return false;
	}
	data.samples = &m_samples[m_offset];
	data.sampleCount = m_samples.size() - m_offset;
	m_offset += data.sampleCount;
	std::cout << "ok" << m_offset <<  std::endl;
	return true;

}
}