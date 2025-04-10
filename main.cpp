
#include <iostream>

#include <string>
using std::string;

#include <chrono>
#include <mutex>
#include <thread>

#include <fstream>
#include <queue>

#include <alsa/asoundlib.h>

class AudioEngine {
	static AudioEngine* s_instance;
	snd_pcm_t* pcmHandle;
	
	bool audioThreadOn;
	std::queue<std::string> audioQueue;
	std::thread audioThread;

	struct WaveHeader {
		char riff[4];                // "RIFF"
		uint32_t chunkSize;         // File size - 8
		char wave[4];               // "WAVE"
		char fmt[4];                // "fmt "
		uint32_t subchunk1Size;     // 16 for PCM
		uint16_t audioFormat;       // PCM = 1
		uint16_t numChannels;       
		uint32_t sampleRate;        
		uint32_t byteRate;          
		uint16_t blockAlign;        
		uint16_t bitsPerSample;     
		char data[4];               // "data"
		uint32_t dataSize;          
	};

	AudioEngine() {
		audioThreadOn = true;
		audioThread = std::thread([this]() {
			monitorQueue();	
		});

		int err;

		err = snd_pcm_open(&pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
		if (err < 0) {
			std::cout << "Playback open error: " << snd_strerror(err) << "\n";
		}

		snd_pcm_prepare(pcmHandle);
	}
	
	void monitorQueue() {
		while (audioThreadOn) {
			if (audioQueue.size() > 0) {
				string filename = audioQueue.front();
				playWavFile(filename);
				audioQueue.pop();
			} else {
				std::cout << "Waiting for audio.\n";
			}
		}	
	}
	
	void playWavFile(string filename_string) {
		const char * filename = filename_string.c_str();	
		snd_pcm_hw_params_t* params;
		std::ifstream file(filename, std::ios::binary);

    	if (!file.is_open()) {
			std::cerr << "Unable to open file: " << filename << "\n";
        	return; 
    	}

		WaveHeader header;
		file.read(reinterpret_cast<char*>(&header), sizeof(header));
	    if (strncmp(header.riff, "RIFF", 4) || strncmp(header.wave, "WAVE", 4)) {
      		std::cerr << "Invalid WAV file\n";
        	return;
    	}

		int err;
		err = snd_pcm_hw_params_malloc(&params);	
		if (err < 0) {
			std::cout << "Hardware paramater malloc error: " << snd_strerror(err) << "\n";
			return;
		}
		err = snd_pcm_hw_params_any(pcmHandle, params);
		if (err < 0) {
			std::cout << "Default hardware paramater allocation error: " << snd_strerror(err) << "\n";
			return;
		}
		err = snd_pcm_hw_params_set_access(pcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
		if (err < 0) {
			std::cout << "Data access method hardware paramater setting error: " << snd_strerror(err) << "\n";
		}

		snd_pcm_format_t format;
		if (header.bitsPerSample == 16)
			format = SND_PCM_FORMAT_S16_LE;
		else {
			std::cerr << "Unsupported bit depth\n";
			return;
		}

		snd_pcm_hw_params_set_format(pcmHandle, params, format);
		snd_pcm_hw_params_set_channels(pcmHandle, params, header.numChannels);
		snd_pcm_hw_params_set_rate(pcmHandle, params, header.sampleRate, 0);
		err = snd_pcm_hw_params(pcmHandle, params);
		if (err < 0) {
			std::cerr << "Error finalizing hardware paramater values: " << snd_strerror(err) << "\n";
			return;
		}
    	snd_pcm_hw_params_free(params);
		const int bufferSize = 4096;
    	char buffer[bufferSize];

    	while (!file.eof()) {
		    file.read(buffer, bufferSize);
	        std::streamsize bytesRead = file.gcount();
 		    snd_pcm_writei(pcmHandle, buffer, bytesRead / (header.bitsPerSample / 8 * header.numChannels));
    	}

    	file.close();
	}
	
public:
	static AudioEngine* getInstance() { 	
		if (s_instance == nullptr) { s_instance = new AudioEngine; }
		return s_instance;
	}
	
	void playWav(const char* filename) {
		audioQueue.push(filename);	
	}
};

AudioEngine* AudioEngine::s_instance = nullptr;

int main() {
	AudioEngine* myEngine = AudioEngine::getInstance();
	myEngine->playWav("example.wav");

	std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}

