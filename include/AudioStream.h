#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H
#include <SFML/Audio.hpp>
#include <vector>
#include <mutex>

namespace sn{
	class AudioStream : public sf::SoundStream {
		public:
			AudioStream(size_t channelNum=1, size_t sampleRate=44100);
			~AudioStream();
			bool addSample(std::int16_t sample);

		protected:
			bool onGetData(Chunk& data) override;
			void onSeek(sf::Time timeOffset) override;
		private:
			std::vector<std::int16_t> m_samples;
			size_t m_offset;
			size_t m_maxsize = 50000;
			std::mutex mtx;
	};
}

#endif //AUDIOSTREAM_H