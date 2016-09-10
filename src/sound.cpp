/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

// XXX It'd be nice if this was abstracted, then the iphone stuff could be hidden better.

#pragma comment(lib, "SDL2_mixer")

#include <assert.h>

#include <iostream>
#include <map>
#include <vector>

#include <vorbis/vorbisfile.h>

#include "SDL_mixer.h"

#include "asserts.hpp"
#include "filesystem.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "thread.hpp"


#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#include "iphone_sound.h"
#endif

#include "variant_utils.hpp"

PREF_INT(mixer_looped_sounds_fade_time_ms, 100, "Number of milliseconds looped sounds should fade for");


#ifndef OLD_AUDIO_SYSTEM

namespace sound 
{

namespace {

//The sample rate we use for all sounds.
const int SampleRate = 44100;

struct MusicInfo {
	MusicInfo() : volume(1.0) {}
	float volume;
};

//Index of music info. Accessed only by the game thread.
std::map<std::string, MusicInfo> g_music_index;

//Name of the current music being played. Accessed only by the game thread.
std::string g_current_music;

std::map<std::string,std::string>& get_music_paths() {
	static std::map<std::string,std::string> res;
	if(res.empty()) {
		module::get_unique_filenames_under_dir("music/", &res);
	}
	return res;
}

//Object capable of decoding ogg music.
class MusicPlayer : public game_logic::FormulaCallable
{
public:

	//Should be constructed in the main thread.
	explicit MusicPlayer(const std::string& file) : bit_stream_(0), volume_(1.0f), finished_(false),
	                                                fadeout_time_(-1.0f), fadeout_current_(-1.0f) {
		auto itor = module::find(get_music_paths(), file);
		ASSERT_LOG(itor != get_music_paths().end(), "Could not find path for file: " << file);
		const int res = ov_fopen(itor->second.c_str(), &file_);
		ASSERT_LOG(res == 0, "Failed to read vorbis file: " << file);

		info_ = ov_info(&file_, -1);

		auto i = g_music_index.find(file);
		if(i != g_music_index.end()) {
			volume_ = i->second.volume;
		}
	}

	~MusicPlayer() {
		ov_clear(&file_);
	}

	//Tell the music to stop playing. It can fade out over some time period.
	//This function can be called from the game thread, and the music thread will
	//then start fading the music out. When fade out is finished it will be put
	//into finished() state and ready for the game thread to destroy.
	void stopPlaying(float fadeout) {
		threading::lock lck(mutex_);
		fadeout_time_ = fadeout;
		fadeout_current_ = 0.0f;
	}

	//start music over from the beginning.
	//This can be called from any thread.
	void restart() {
		threading::lock lck(mutex_);
		seekTime(0.0);
		finished_ = false;
		fadeout_time_ = -1.0f;
		fadeout_current_ = -1.0f;
	}

	//Read samples which will be mixed into the given buffer, returning the
	//number of samples read. This is designed to be called from the music thread.
	//It will set the object into the finished() state ready for cleanup by the
	//game thread if it reaches EOF.
	//
	//This will read out interleaved stereo, converting mono -> stereo if necessary.
	int read(float* out, int out_nsamples) {
		char buf[4096];

		int max_needed = out_nsamples*sizeof(short)/(numChannels() == 1 ? 2 : 1);

		int nread = std::min<int>(sizeof(buf), max_needed);

		long nbytes;
		
		float fadeout_time = 0.0f;
		float fadeout_current = 0.0f;
		int nsamples;
		{
			threading::lock lck(mutex_);
			nbytes = ov_read(&file_, buf, nread, 0, 2, 1, &bit_stream_);
		
			if(nbytes <= 0) {
				finished_ = true;
				return 0;
			}

			fadeout_time = fadeout_time_;
			fadeout_current = fadeout_current_;
			nsamples = nbytes/2;

			if(fadeout_time >= 0.0f) {
				if(fadeout_current_ >= fadeout_time_) {
					return 0;
				}
				fadeout_current_ += float(nsamples*2/numChannels())/float(SampleRate);
				if(fadeout_current_ >= fadeout_time_) {
					finished_ = true;
				}
			}
		}

		float* start_out = out;

		short* data = reinterpret_cast<short*>(buf);

		if(fadeout_time >= 0.0f) {
			if(numChannels() == 2) {
				assert(nsamples <= out_nsamples);
				for(int n = 0; n != nsamples; ++n) {
					*out++ += (1.0f - fadeout_current/fadeout_time)*volume_*static_cast<float>(data[n])/SHRT_MAX;
					fadeout_current += 1.0/float(SampleRate);
				}
			} else {
				assert(nsamples*2 <= out_nsamples);
				for(int n = 0; n != nsamples; ++n) {
					*out++ += (1.0f - fadeout_current/fadeout_time)*volume_*static_cast<float>(data[n])/SHRT_MAX;
					*out++ += (1.0f - fadeout_current/fadeout_time)*volume_*static_cast<float>(data[n])/SHRT_MAX;
					fadeout_current += 1.0/float(SampleRate);
				}
			}
		} else {

			if(numChannels() == 2) {
				assert(nsamples <= out_nsamples);
				for(int n = 0; n != nsamples; ++n) {
					*out++ += volume_*static_cast<float>(data[n])/SHRT_MAX;
				}
			} else {
				assert(nsamples*2 <= out_nsamples);
				for(int n = 0; n != nsamples; ++n) {
					*out++ += volume_*static_cast<float>(data[n])/SHRT_MAX;
					*out++ += volume_*static_cast<float>(data[n])/SHRT_MAX;
				}
			}

		}

		return out - start_out;
	}

	//Length of the track in seconds
	double timeLength() {
		threading::lock lck(mutex_);
		return ov_time_total(&file_, -1);
	}

	//current position in the track in seconds.
	double timeCurrent() {
		threading::lock lck(mutex_);
		return ov_time_tell(&file_);
	}

	//Move the current play position. Can be called from any thread.
	void seekTime(double t) {
		threading::lock lck(mutex_);
		int res = ov_time_seek(&file_, t);
		ASSERT_LOG(res == 0, "Failed to seek music: " << res);
	}

	int numChannels() const {
		return info_->channels;
	}

	//If finished() returns true, the game thread can remove this
	//object from the playing list as long as it holds the g_music_thread_mutex.
	bool finished() const {
		threading::lock lck(mutex_);
		return finished_;
	}
private:
	DECLARE_CALLABLE(MusicPlayer);

	OggVorbis_File file_;
	vorbis_info* info_;
	int bit_stream_;

	threading::mutex mutex_;

	float volume_;

	//will only be set by the music thread, after which it won't touch the object again.
	//then the music player can be released.
	bool finished_;

	float fadeout_time_, fadeout_current_;
};


BEGIN_DEFINE_CALLABLE_NOBASE(MusicPlayer)
END_DEFINE_CALLABLE(MusicPlayer)

//Current music being played. This will usually have one music player in it
//but may have more if we are crossfading to different music.
//Access is controlled by g_music_thread_mutex. Only the game thread will
//modify this, so it can read from it without locking the mutex.
std::vector<boost::intrusive_ptr<MusicPlayer> > g_music_players;

//Flag to tell the music thread to exit. Access controlled by g_music_thread_mutex.
bool g_music_thread_exit = false;

//Mutex used to control access to g_music_players and g_music_thread_exit.
threading::mutex g_music_thread_mutex;

//Next music track to  play. Only accessed from the game thread.
boost::intrusive_ptr<MusicPlayer> g_music_queue;


//Ring buffer which music is mixed into by the music thread. The mixer thread consumes
//this for the mix.
//Access to g_music_buf_read and g_music_buf_nsamples is controlled by g_music_thread_mutex.
//g_music_buf_write is only used by the music thread so doesn't need to be synchronized.
float g_music_buf[8192];
float* g_music_buf_read = g_music_buf;
float* g_music_buf_write = g_music_buf;
int g_music_buf_nsamples = 0;

//The music thread. The purpose of this thread is to fill the music ring buffer with music
//mixed from the g_music_players music tracks.
void MusicThread()
{
	for(;;) {
		std::vector<MusicPlayer*> players;

		float* buf_read;

		int nspace_available = 0;

		{
			threading::lock lck(g_music_thread_mutex);
			if(g_music_thread_exit) {
				return;
			}

			for(const boost::intrusive_ptr<MusicPlayer>& p : g_music_players) {
				if(p->finished() == false) {
					players.push_back(p.get());
				}
			}

			buf_read = g_music_buf_read;

			nspace_available = sizeof(g_music_buf)/sizeof(*g_music_buf) - g_music_buf_nsamples;
		}

		float* end_music_buf = g_music_buf + sizeof(g_music_buf)/sizeof(*g_music_buf);

		int nwrite = 0;
		while(nwrite < nspace_available) {
			int nspace = (g_music_buf_write < buf_read ? buf_read : end_music_buf) - g_music_buf_write;
			for(int n = 0; n != nspace; ++n) {
				g_music_buf_write[n] = 0.0f;
			}

			for(auto player : players) {
				int nwant = nspace;
				float* pwrite = g_music_buf_write;

				while(nwant > 0) {
					int ngot = player->read(pwrite, nwant);
					if(ngot <= 0) {
						break;
					}

					nwant -= ngot;
					pwrite += ngot;
				}
			}

			g_music_buf_write += nspace;
			nwrite += nspace;
			if(g_music_buf_write == end_music_buf) {
				g_music_buf_write = g_music_buf;
			}
		}

		{
			threading::lock lck(g_music_thread_mutex);
			g_music_buf_nsamples += nwrite;
		}

		SDL_Delay(20);
	}
}

//Function to load an ogg vorbis file into a buffer. Fills 'spec' with the
//specs of the format loaded.
bool loadVorbis(const char* file, SDL_AudioSpec* spec, std::vector<char>& buf)
{
	FILE* f = fopen(file, "rb");

	OggVorbis_File ogg_file;

	int res = ov_open(f, &ogg_file, nullptr, 0);
	if(res != 0) {
		return false;
	}

	vorbis_info* info = ov_info(&ogg_file, -1);

	spec->freq = info->rate;
	spec->channels = info->channels;
	spec->format = AUDIO_S16;
	spec->silence = 0;

	int bit_stream = 0;

	long nbytes = 1;
	while(nbytes > 0) {
		const int buf_len = 4096;
		buf.resize(buf.size() + buf_len);
		nbytes = ov_read(&ogg_file, &buf[0] + buf.size() - buf_len, buf_len, 0, 2, 1, &bit_stream);
		buf.resize(buf.size() - 4096 + nbytes);
	}

	spec->samples = buf.size()/4;
	spec->size = buf.size();
	spec->callback = nullptr;
	spec->userdata = nullptr;

	ov_clear(&ogg_file);

	return true;
}

//function to map a sound filename to a physical path.
std::string map_filename(const std::string& fname)
{
	return module::map_file("sounds/" + fname);
}

//The number of samples we ask SDL to ask for when it calls our callback in the mixing thread.
const int BUFFER_NUM_SAMPLES = 1024;

//In memory represenation of a wave file. We always use 32-bit floats in stereo
struct WaveData {
	explicit WaveData(std::vector<float>* buffer) { buf.swap(*buffer); }
	std::vector<float> buf;
	size_t nsamples() const { return buf.size()/2; }
};

//A cache of loaded sound effects with synchronization.
std::map<std::string, std::shared_ptr<WaveData> > g_wave_cache;
threading::mutex g_wave_cache_mutex;

bool get_cached_wave(const std::string& fname, std::shared_ptr<WaveData>* ptr)
{
	threading::mutex g_wave_cache_mutex;
	auto itor = g_wave_cache.find(fname);
	if(itor == g_wave_cache.end()) {
		return false;
	} else {
		*ptr = itor->second;
		return true;
	}
}


bool g_muted;

float g_sfx_volume = 1.0f, g_user_music_volume = 1.0f, g_engine_music_volume = 1.0f;

//Function which loads a sound effect (can be in wave or ogg format). Blocks while loading,
//and places the effect into our audio cache.
void LoadWaveBlocking(const std::string& fname)
{
	SDL_AudioSpec spec;
	spec.freq = SampleRate;
	spec.format = AUDIO_F32;
	spec.channels = 2;
	spec.silence = 0;
	spec.size = BUFFER_NUM_SAMPLES * sizeof(float) * spec.channels;

	SDL_AudioSpec in_spec = spec;

	Uint8* buf = nullptr;
	Uint32 len = 0;

	SDL_AudioSpec spec_buf;
	SDL_AudioSpec* res_spec = &spec_buf;

	std::vector<char> ogg_buf;

	if(fname.size() > 4 && std::equal(fname.end()-4,fname.end(), ".ogg")) {
		bool res = loadVorbis(fname.c_str(), res_spec, ogg_buf);
		ASSERT_LOG(res, "Could not load ogg: " << fname);
		ASSERT_LOG(ogg_buf.size() > 0, "No ogg data: " << fname);
		buf = reinterpret_cast<Uint8*>(&ogg_buf[0]);
		len = ogg_buf.size();
	} else {
		res_spec = SDL_LoadWAV(fname.c_str(), &in_spec, &buf, &len);
	}

	if(res_spec == nullptr) {
		LOG_ERROR("Could not load sound: " << fname);

		threading::lock lck(g_wave_cache_mutex);
		g_wave_cache[fname] = std::shared_ptr<WaveData>();
	} else {

		SDL_AudioCVT cvt;
		int res = SDL_BuildAudioCVT(&cvt, res_spec->format, res_spec->channels, res_spec->freq, spec.format, spec.channels, spec.freq);
		ASSERT_LOG(res >= 0, "Could not convert audio: " << SDL_GetError());

		std::vector<float> out_buf;

		if(res == 0) {
			out_buf.resize(len/sizeof(float));
			if(!out_buf.empty()) {
				memcpy(&out_buf[0], buf, out_buf.size()*sizeof(float));
			}

		} else {
			cvt.len = len;

			std::vector<Uint8> tmp_buf;
			tmp_buf.resize(cvt.len * cvt.len_mult);

			memcpy(&tmp_buf[0], buf, len);

			cvt.buf = &tmp_buf[0];

			int res = SDL_ConvertAudio(&cvt);
			ASSERT_LOG(res >= 0, "Could not convert audio: " << SDL_GetError());

			out_buf.resize(cvt.len_cvt/sizeof(float));
			memcpy(&out_buf[0], cvt.buf, cvt.len_cvt);
		}

		if(ogg_buf.empty()) {
			SDL_FreeWAV(buf);
		}

		std::shared_ptr<WaveData> data(new WaveData(&out_buf));

		threading::lock lck(g_wave_cache_mutex);
		g_wave_cache[fname] = data;
	}

}

//The loader thread is responsible for loading sound effects and
//placing them in the wave cache.
threading::mutex g_loader_thread_mutex;

//condition used to wake the thread up when new sound effects have
//been added to the queue or when the thread should exit.
threading::condition g_loader_thread_cond;
bool g_loader_thread_exit = false;

//sound effects to be loaded. Care should be taken externally not
//to push duplicate items onto this queue.
std::vector<std::string> g_loader_thread_queue;

void LoaderThread()
{
	for(;;) {
		std::vector<std::string> items;
		{
			threading::lock lck(g_loader_thread_mutex);
			if(g_loader_thread_exit) {
				return;
			}

			if(g_loader_thread_queue.empty() == false) {
				items.swap(g_loader_thread_queue);
			} else {
				g_loader_thread_cond.wait(g_loader_thread_mutex);
			}
		}

		for(const std::string& item : items) {
			LoadWaveBlocking(item);
		}
	}
}

std::shared_ptr<threading::thread> g_loader_thread;
std::shared_ptr<threading::thread> g_music_thread;

//Base class for a sound effect filter which can perform dsp effects on sounds.
class SoundEffectFilter : public game_logic::FormulaCallable
{
public:
	virtual ~SoundEffectFilter() {}
	virtual void MixData(float* output, int nsamples) = 0;

	DECLARE_CALLABLE(SoundEffectFilter);
};

BEGIN_DEFINE_CALLABLE_NOBASE(SoundEffectFilter)
END_DEFINE_CALLABLE(SoundEffectFilter)

//Representation of a sound currently playing. A new instance will be created every time
//a sound effect starts playing, so is reasonably lightweight.
//Instances are created by the game thread but accessed from the mixing thread.
//
//If the underlying data isn't available when this object is created it will wait, polling
//every frame to see if the cache has been populated every frame, and then play as soon
//as the data is loaded.
class PlayingSound : public SoundEffectFilter
{
public:
	PlayingSound(const std::string& fname, const void* obj, float volume, float fade_in) : fname_(fname), pos_(0), obj_(obj), volume_(volume), fade_in_(fade_in), looped_(false), fade_out_(-1.0f), fade_out_current_(0.0f), left_pan_(1.0f), right_pan_(1.0f)
	{
		init();
	}

	virtual ~PlayingSound() {}

	void setLooped() { looped_ = true; }
	bool looped() const { return looped_; }

	void setPanning(float left, float right)
	{
		threading::lock lck(mutex_);
		left_pan_ = left;
		right_pan_ = right;
	}

	void init()
	{
		threading::lock lck(mutex_);
		if(data_) {
			return;
		}
		bool res = get_cached_wave(map_filename(fname_), &data_);
		if(res) {
			ASSERT_LOG(data_.get() != nullptr, "Could not load wave: " << fname_);
		} else {
			preload(fname_);
		}
	}

	void stopPlaying(float fade_time) {
		threading::lock lck(mutex_);
		fade_out_ = fade_time;
		fade_out_current_ = 0.0f;
	}

	//Once finished(), the game thread may remove this from the list of playing sounds.
	bool finished() const
	{
		threading::lock lck(mutex_);
		return (data_.get() != nullptr && !looped_ && pos_ >= data_->nsamples()) || (fade_out_ >= 0.0f && fade_out_current_ >= fade_out_);
	}

	void setVolume(float volume)
	{
		threading::lock lck(mutex_);
		volume_ = volume;
	}

	//Mix data into the output buffer. Can be safely called from the mixing thread.
	virtual void MixData(float* output, int nsamples)
	{
		std::shared_ptr<WaveData> data;
		int pos;
		float fade_out, fade_out_current;
		float volume;
		{
			threading::lock lck(mutex_);
			if(!data_ || (!looped_ && pos_ >= data_->nsamples()) || (fade_out_ >= 0.0f && fade_out_current_ >= fade_out_)) {
				return;
			}
			pos = pos_;
			pos_ += nsamples;
			data = data_;

			fade_out = fade_out_;
			fade_out_current = fade_out_current_;

			if(fade_out_ >= 0.0f) {
				fade_out_current_ += float(std::min<int>(nsamples, data_->nsamples() - pos))/float(SampleRate);
			}

			if(looped_ && pos_ >= data_->nsamples()) {
				pos_ = 0;
				fade_in_ = 0.0f;
			}

			volume = volume_ * g_sfx_volume;
		}

		if(pos < 0) {
			nsamples += pos;
			pos = 0;
			if(nsamples <= 0) {
				return;
			}
		}

		int nmissed = 0;

		const int navail = data->nsamples() - pos;
		if(nsamples > navail) {
			nmissed = nsamples - navail;
			nsamples = navail;
		}

		const float* p = &data->buf[pos*2];

		if(pos < fade_in_*SampleRate || fade_out >= 0.0f) {
			for(int n = 0; n != nsamples*2; ++n) {
				*output++ += *p++ * volume * std::min<float>(1.0f, ((pos+n*2) / (SampleRate*fade_in_))) * (1.0f - (fade_out_current + (n*0.5)/SampleRate)/fade_out);
			}
		} else if(left_pan_ != 1.0f || right_pan_ != 1.0f) {
			for(int n = 0; n != nsamples; ++n) {
				*output++ += *p++ * volume * left_pan_;
				*output++ += *p++ * volume * right_pan_;
			}
		} else {
			for(int n = 0; n != nsamples*2; ++n) {
				*output++ += *p++ * volume;
			}
		}

		if(looped_ && nmissed > 0 && data_->nsamples() > 0) {
			MixData(output, nmissed);
		}
	}

	const void* obj() const { return obj_; }
	const std::string& fname() const { return fname_; }

private:
	DECLARE_CALLABLE(PlayingSound);

	std::string fname_;
	std::shared_ptr<WaveData> data_;

	int pos_;

	threading::mutex mutex_;

	const void* obj_;

	float volume_, fade_in_;

	float fade_out_, fade_out_current_;

	bool looped_;

	float left_pan_, right_pan_;
};

BEGIN_DEFINE_CALLABLE(PlayingSound, SoundEffectFilter)
DEFINE_FIELD(filename, "string")
	return variant(obj.fname_);
END_DEFINE_CALLABLE(PlayingSound)

//List of currently playing sounds. Only the game thread can modify this,
//the mixing thread will read it.
std::vector<boost::intrusive_ptr<PlayingSound> > g_playing_sounds;
threading::mutex g_playing_sounds_mutex;

//Audio callback called periodically by SDL to populate audio data in
//the mixing thread.
void AudioCallback(void* userdata, Uint8* stream, int len)
{
	float sfx_volume = g_sfx_volume;

	float* buf = reinterpret_cast<float*>(stream);
	const int nsamples = len / sizeof(float);

	//Set the buffer to zeroes so we can start mixing.
	for(int n = 0; n < nsamples; ++n) {
		buf[n] = 0.0f;
	}

	if(g_muted) {
		return;
	}

	//Mix all the sound effects.
	{
		threading::lock lck(g_playing_sounds_mutex);
		for(const boost::intrusive_ptr<PlayingSound>& s : g_playing_sounds) {
			s->MixData(buf, nsamples/2);
		}
	}

	//Now mix the music from the music ring buffer.
	const float music_volume = g_engine_music_volume*g_user_music_volume;

	float*	music_read = nullptr;
	int music_nsamples = 0;
	float* end_music_buf = g_music_buf + sizeof(g_music_buf)/sizeof(*g_music_buf);
	{
		threading::lock lck(g_music_thread_mutex);
		music_read = g_music_buf_read;
		music_nsamples = g_music_buf_nsamples;
	}

	int music_starting_samples = music_nsamples;

	int navail = std::min<int>(music_nsamples, end_music_buf - music_read);
	int nmix = std::min<int>(navail, nsamples);

	float* music_write_buf = buf;

	for(int n = 0; n < nmix; ++n) {
		*music_write_buf++ += *music_read++ * music_volume;
	}

	music_nsamples -= nmix;

	if(music_read == end_music_buf) {
		music_read = g_music_buf;
		nmix = std::min<int>(music_nsamples, nsamples - nmix);
		for(int n = 0; n < nmix; ++n) {
			*music_write_buf++ += *music_read++ * music_volume;
		}

		music_nsamples -= nmix;
	}

	{
		threading::lock lck(g_music_thread_mutex);
		g_music_buf_read = music_read;
		g_music_buf_nsamples -= music_starting_samples - music_nsamples;
	}
}

//The ID of our audio device.
SDL_AudioDeviceID g_audio_device;

}

Manager::Manager()
{
	if(preferences::no_sound()) {
		return;
	}

	if(SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if(SDL_InitSubSystem(SDL_INIT_AUDIO) == -1) {
			ASSERT_LOG(false, "Could not init audio: " << SDL_GetError());
		}
	}

	if(g_audio_device > 0) {
		return;
	}

	g_loader_thread.reset(new threading::thread("sound_loader", LoaderThread));
	g_music_thread.reset(new threading::thread("music_mixer", MusicThread));

	SDL_AudioSpec spec;

	spec.freq = SampleRate;
	spec.format = AUDIO_F32;
	spec.channels = 2;
	spec.silence = 0;
	spec.size = BUFFER_NUM_SAMPLES * sizeof(float) * spec.channels;
	spec.samples = BUFFER_NUM_SAMPLES;
	spec.callback = AudioCallback;
	spec.userdata = nullptr;

	g_audio_device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
	SDL_PauseAudioDevice(g_audio_device, 0);

	process();
}

Manager::~Manager()
{
	if(g_audio_device > 0) {
		if(g_music_thread) {
			threading::lock lck(g_music_thread_mutex);
			g_music_thread_exit = true;
		}

		if(g_loader_thread) {
			threading::lock lck(g_loader_thread_mutex);
			g_loader_thread_exit = true;
			g_loader_thread_cond.notify_one();
		}

		if(g_music_thread) {
			g_music_thread->join();
			g_music_thread.reset();
		}

		if(g_loader_thread) {
			g_loader_thread->join();
			g_loader_thread.reset();
		}

		SDL_CloseAudioDevice(g_audio_device);
		g_audio_device = 0;
	}
}

//Load music volumes from the config file.
void init_music(variant node)
{
	for(variant music_node : node["music"].as_list()) {
		const std::string name = music_node["name"].as_string();
		g_music_index[name].volume = music_node["volume"].as_float(1.0f);
	}
}

bool ok()
{
	return g_audio_device != 0;
}

bool muted()
{
	return g_muted;
}

void mute(bool flag)
{
	g_muted = flag;
}

//The game thread, called every frame.
void process()
{
	//Go through the playing sounds list and remove any that are finished.
	{
		threading::lock lck(g_playing_sounds_mutex);
		for(boost::intrusive_ptr<PlayingSound>& s : g_playing_sounds) {
			s->init();

			if(s->finished()) {
				s.reset();
			}
		}

		g_playing_sounds.erase(std::remove(g_playing_sounds.begin(), g_playing_sounds.end(), boost::intrusive_ptr<PlayingSound>()), g_playing_sounds.end());
	}

	//Go through the music players and removed any that are finished.
	{
		threading::lock lck(g_music_thread_mutex);
		for(boost::intrusive_ptr<MusicPlayer>& p : g_music_players) {
			if(p->finished()) {
				if(p == g_music_players.back()) {
					p->restart();
				} else {
					p.reset();
				}
			}
		}

		g_music_players.erase(std::remove(g_music_players.begin(), g_music_players.end(), boost::intrusive_ptr<MusicPlayer>()), g_music_players.end());

	}
}

namespace {
std::set<std::string> g_files_loading;
}

//Game-engine facing audio API implementation below here.

//preload a sound effect in the cache.
void preload(const std::string& fname)
{
	if(g_files_loading.count(fname)) {
		return;
	}

	g_files_loading.insert(fname);

	std::string file = map_filename(fname);

	threading::lock lck(g_loader_thread_mutex);
	g_loader_thread_queue.push_back(file);
	g_loader_thread_cond.notify_one();
}

void change_volume(const void* object, float volume)
{
	for(const boost::intrusive_ptr<PlayingSound>& s : g_playing_sounds) {
		if(s->obj() == object) {
			s->setVolume(volume);
		}
	}
}

//Ways to set the sound and music volumes from the user's perspective.
float get_sound_volume()
{
	return g_sfx_volume;
}

void set_sound_volume(float volume)
{
	g_sfx_volume = volume;
}

float get_music_volume()
{
	return g_user_music_volume;
}

void set_music_volume(float volume)
{
	g_user_music_volume = volume;
}


//Ways to set the music volume from the game engine's perspective.
void set_engine_music_volume(float volume)
{
	g_engine_music_volume = volume;
}

float get_engine_music_volume()
{
	return g_engine_music_volume;
}

namespace {
float g_pan_left = 1.0f, g_pan_right = 1.0f;
}


void set_panning(float left, float right)
{
	g_pan_left = left;
	g_pan_right = right;
}

void update_panning(const void* obj, const std::string& id, float left, float right)
{
	for(const boost::intrusive_ptr<PlayingSound>& s : g_playing_sounds) {
		if((s->obj() == obj && s->fname() == id) || id.empty()) {
			s->setPanning(left, right);
		}
	}
}

//play a sound. 'object' is the object that is playing the sound. It can be
//used later in stop_sound to specify which object is stopping playing
//the sound.
void play(const std::string& file, const void* object, float volume, float fade_in_time)
{
	if(!ok()) {
		return;
	}

	boost::intrusive_ptr<PlayingSound> s(new PlayingSound(file, object, volume, fade_in_time));
	s->setPanning(g_pan_left, g_pan_right);

	threading::lock lck(g_playing_sounds_mutex);
	g_playing_sounds.push_back(s);
}


//stop a sound. object refers to the object that started the sound, and is
//the same as the object in play().
void stop_sound(const std::string& file, const void* object, float fade_out_time)
{
	for(const boost::intrusive_ptr<PlayingSound>& s : g_playing_sounds) {
		if(s->obj() == object && s->fname() == file) {
			s->stopPlaying(fade_out_time);
		}
	}
}


//stop all looped sounds associated with an object; same object as in play()
//intended to be called in all object's destructors
void stop_looped_sounds(const void* object)
{
	for(const boost::intrusive_ptr<PlayingSound>& s : g_playing_sounds) {
		if(s->obj() == object && s->looped()) {
			s->stopPlaying(g_mixer_looped_sounds_fade_time_ms/1000.0f);
		}
	}
}


// function to play a sound effect over and over in a loop. Will return
// a handle to the sound effect.
int play_looped(const std::string& file, const void* object, float volume, float fade_in_time)
{
	if(!ok()) {
		return -1;
	}

	boost::intrusive_ptr<PlayingSound> s(new PlayingSound(file, object, volume, fade_in_time));
	s->setLooped();
	s->setPanning(g_pan_left, g_pan_right);

	threading::lock lck(g_playing_sounds_mutex);
	g_playing_sounds.push_back(s);
	return -1;
}


void play_music(const std::string& file, bool queue, int fade_time)
{
	if(file == g_current_music) {
		return;
	}

	boost::intrusive_ptr<MusicPlayer> player(new MusicPlayer(file.c_str()));

	if(queue && g_music_players.empty() == false) {
		g_music_queue = player;
	} else {
		g_current_music = file;
		threading::lock lck(g_music_thread_mutex);
		for(auto& p : g_music_players) {
			p->stopPlaying(fade_time/60.f);
		}
		g_music_players.push_back(player);
	}
}

void play_music_interrupt(const std::string& file)
{
	if(file == g_current_music) {
		return;
	}

	boost::intrusive_ptr<MusicPlayer> player(new MusicPlayer(file.c_str()));

	g_current_music = file;

	threading::lock lck(g_music_thread_mutex);
	for(auto& p : g_music_players) {
		p->stopPlaying(0.1f);
	}
	g_music_players.push_back(player);
}


const std::string& current_music()
{
	return g_current_music;
}

}

#else

namespace sound 
{
	namespace 
	{
		PREF_BOOL(assert_on_missing_sound, false, "If true, missing sounds will be treated as a fatal error");

		struct MusicInfo {
			MusicInfo() : volume(1.0) {}
			float volume;
		};

		std::map<std::string, MusicInfo> music_index;

		float g_stereo_left = 1.0, g_stereo_right = 1.0;

		float sfx_volume = 1.0;
		float user_music_volume = 1.0;
		float engine_music_volume = 1.0;
		float track_music_volume = 1.0;
		const int SampleRate = 44100;
		// number of allocated channels, 
#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		const size_t NumChannels = 16;
#else
		const size_t NumChannels = 16;
#endif

		#ifdef WIN32
		const size_t BufferSize = 4096;
#else
		const size_t BufferSize = 1024;
#endif

		bool sound_ok = false;
		bool mute_ = false;
		std::string& current_music_name() {
			static std::string name;
			return name;
		}

		struct ScheduledMusicItem {
			ScheduledMusicItem() : fade_time(500)
			{}
			std::string name;
			int fade_time;
		};

		ScheduledMusicItem& next_music() {
			static ScheduledMusicItem item;
			return item;
		}

#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		Mix_Music* current_mix_music = nullptr;
#else
		bool playing_music = false;
#endif

		//function which gets called when music finishes playing. It starts playing
		//of the next scheduled track, if there is one.
		void on_music_finished()
		{
#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
			Mix_FreeMusic(current_mix_music);
			current_mix_music = nullptr;
#else
			playing_music = false;
#endif
			if(next_music().name.empty() == false) {
				play_music(next_music().name, false, next_music().fade_time);
			}
			next_music().name.clear();
		}

		//record which channels sounds are playing on, in case we
		//want to cancel a sound.
		struct sound_playing {
			std::string file;
			const void* object;
			int	loops;		//not strictly boolean.  -1=true, 0=false
			float volume;
			float fade_in_time;
			float time_cnt;
			float fade_out_time;
		};

		std::vector<sound_playing> channels_to_sounds_playing, queued_sounds;

		void on_sound_finished(int channel)
		{
			if(channel >= 0 && static_cast<unsigned>(channel) < channels_to_sounds_playing.size()) {
				channels_to_sounds_playing[channel].object = nullptr;
			}
		}

#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE

		class sound; //so mixer can make pointers to it

		struct mixer
		{
			/* channel array holds information about currently playing sounds */
			struct
			{
				Uint8 *position; /* what is the current position in the buffer of this sound ? */
				Uint32 remaining; /* how many bytes remaining before we're done playing the sound ? */
				Uint32 timestamp; /* when did this sound start playing ? */
				int volume;
				int loops;
				sound *s;
			} channels[NumChannels];
			SDL_AudioSpec outputSpec; /* what audio format are we using for output? */
			int numSoundsPlaying; /* how many sounds are currently playing */
		} mixer;

		class sound
		{
			public:
			std::shared_ptr<Uint8> buffer; /* audio buffer for sound file */
			Uint32 length; /* length of the buffer (in bytes) */
			sound (const std::string& file = "") : length(0)
			{
				if (file == "") return;
				SDL_AudioSpec spec; /* the audio format of the .wav file */
				SDL_AudioCVT cvt; /* used to convert .wav to output format when formats differ */
				Uint8 *tmp_buffer;
#if defined(__ANDROID__)
				if(SDL_LoadWAV_RW(sys::read_sdl_rw_from_asset(module::map_file(file).c_str(), 1, &spec, &tmp_buffer, &length) == nullptr)
#else
				if (SDL_LoadWAV(module::map_file(file).c_str(), &spec, &tmp_buffer, &length) == nullptr)
#endif
				{
					std::cerr << "Could not load sound: " << file << "\n";
					return; //should maybe die
				}
				buffer = std::shared_ptr<Uint8>(tmp_buffer, SDL_free);
				return; // don't convert the audio, assume it's already in the right format
				/* build the audio converter */
				int result = SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
					mixer.outputSpec.format, mixer.outputSpec.channels, mixer.outputSpec.freq);
				if (result == -1)
				{
					std::cerr << "Could not build audio CVT for: " << file << "\n";
					return; //should maybe die
				} else if (result != 0) {
					/* 
					 this happens when the .wav format differs from the output format.
					 we convert the .wav buffer here
					 */
					cvt.buf = (Uint8 *) SDL_malloc(length * cvt.len_mult); /* allocate conversion buffer */
					cvt.len = length; /* set conversion buffer length */
					SDL_memcpy(cvt.buf, buffer.get(), length); /* copy sound to conversion buffer */
					if (SDL_ConvertAudio(&cvt) == -1) /* convert the sound */
					{
						std::cerr << "Could not convert sound: " << file << "\n";
						SDL_free(cvt.buf);
						return; //should maybe die
					}
					buffer = std::shared_ptr<Uint8>(cvt.buf, SDL_free); /* point sound buffer to converted buffer */
					length = cvt.len_cvt; /* set sound buffer's new length */
				}
			}
	
			bool operator==(void *p) {return buffer.get() == p;}
		};
	
		void sdl_stop_channel (int channel)
		{
			if (mixer.channels[channel].position == nullptr) return; // if the sound was playing in the first place
			mixer.channels[channel].position = nullptr;  /* indicates no sound playing on channel anymore */
			mixer.numSoundsPlaying--;
			if (mixer.numSoundsPlaying == 0)
			{
				/* if no sounds left playing, pause audio callback */
				SDL_PauseAudio(1);
			}
		}

		void sdl_audio_callback (void *userdata, Uint8 * stream, int len)
		{
			int i;
			int copy_amt;
			SDL_memset(stream, mixer.outputSpec.silence, len);  /* initialize buffer to silence */
			/* for each channel, mix in whatever is playing on that channel */
			for (i = 0; i < NumChannels; i++)
			{
				if (mixer.channels[i].position == nullptr)
				{
					/* if no sound is playing on this channel */
					continue;           /* nothing to do for this channel */
				}
		
				/* copy len bytes to the buffer, unless we have fewer than len bytes remaining */
				copy_amt = mixer.channels[i].remaining < len ? mixer.channels[i].remaining : len;
		
				/* mix this sound effect with the output */
				SDL_MixAudioFormat(stream, mixer.channels[i].position, mixer.outputSpec.format, copy_amt, sfx_volume * mixer.channels[i].volume);
		
				/* update buffer position in sound effect and the number of bytes left */
				mixer.channels[i].position += copy_amt;
				mixer.channels[i].remaining -= copy_amt;
		
				/* did we finish playing the sound effect ? */
				if (mixer.channels[i].remaining == 0)
				{
					if (mixer.channels[i].loops != 0)
					{
						mixer.channels[i].position = mixer.channels[i].s->buffer.get();
						mixer.channels[i].remaining = mixer.channels[i].s->length;
						if (mixer.channels[i].loops != -1) mixer.channels[i].loops--;
					} else {
						sdl_stop_channel(i);
					}
				}
			}
		}

		int sdl_play_sound (sound *s, int loops)
		{
			/*
			 find an empty channel to play on.
			 if no channel is available, use oldest channel
			 */
			int i;
			int selected_channel = -1;
			int oldest_channel = 0;
	
			if (mixer.numSoundsPlaying == 0) {
				/* we're playing a sound now, so start audio callback back up */
				SDL_PauseAudio(0);
			}
	
			/* find a sound channel to play the sound on */
			for (i = 0; i < NumChannels; i++) {
				if (mixer.channels[i].position == nullptr) {
					/* if no sound on this channel, select it */
					selected_channel = i;
					break;
				}
				/* if this channel's sound is older than the oldest so far, set it to oldest */
				if (mixer.channels[i].timestamp < mixer.channels[oldest_channel].timestamp)
					oldest_channel = i;
			}
	
			/* no empty channels, take the oldest one */
			if (selected_channel == -1)
				selected_channel = oldest_channel;
			else
				mixer.numSoundsPlaying++;
	
			/* point channel data to wav data */
			mixer.channels[selected_channel].position = s->buffer.get();
			mixer.channels[selected_channel].remaining = s->length;
			mixer.channels[selected_channel].timestamp = profile::get_tick_time();
			mixer.channels[selected_channel].volume = SDL_MIX_MAXVOLUME;
			mixer.channels[selected_channel].loops = loops;
			mixer.channels[selected_channel].s = s;
	
			return selected_channel;
		}

#endif
	
#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		typedef std::map<std::string, Mix_Chunk*> cache_map;
#else
		typedef std::map<std::string, sound> cache_map;
#endif
		cache_map cache;

		cache_map threaded_cache;
		threading::mutex cache_mutex;


		bool sound_init = false;

		void thread_load(const std::string& file)
		{
#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
#if defined(__ANDROID__)
			Mix_Chunk* chunk = Mix_LoadWAV_RW(sys::read_sdl_rw_from_asset(module::map_file("sounds/" + file).c_str()),1);
#else
			Mix_Chunk* chunk = Mix_LoadWAV(module::map_file("sounds/" + file).c_str());
#endif
			if(chunk == nullptr) {
				LOG_ERROR("Error loading sound file " << file << ": " << SDL_GetError());
			}

			{
				threading::lock l(cache_mutex);
				threaded_cache[file] = chunk;
			}

#else
			std::string wav_file = file;
			wav_file.replace(wav_file.length()-3, wav_file.length(), "wav");
			sound s("sounds_wav/" + wav_file);

			{
				threading::lock l(cache_mutex);
				threaded_cache[file] = s;
			}
#endif
		}

		std::map<std::string, std::shared_ptr<threading::thread> > loading_threads;
	}

	Manager::Manager()
	{
		sound_init = true;
		if(preferences::no_sound()) {
			return;
		}

		if(SDL_WasInit(SDL_INIT_AUDIO) == 0) {
			if(SDL_InitSubSystem(SDL_INIT_AUDIO) == -1) {
				sound_ok = false;
				LOG_ERROR("failed to init sound!");
				return;
			}
		}

	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE

		if(Mix_OpenAudio(SampleRate, MIX_DEFAULT_FORMAT, 2, BufferSize) == -1) {
			sound_ok = false;
			LOG_ERROR("failed to open audio!");
			return;
		}

		Mix_AllocateChannels(NumChannels);
		sound_ok = true;

		Mix_ChannelFinished(on_sound_finished);
		Mix_HookMusicFinished(on_music_finished);
		Mix_VolumeMusic(MIX_MAX_VOLUME);
	#else
		iphone_init_music(on_music_finished);
		sound_ok = true;
	
		/* initialize the mixer */
		SDL_memset(&mixer, 0, sizeof(mixer));
		/* setup output format */
		mixer.outputSpec.freq = SampleRate;
		mixer.outputSpec.format = AUDIO_S16LSB;
		mixer.outputSpec.channels = 1;
		mixer.outputSpec.samples = 256;
		mixer.outputSpec.callback = sdl_audio_callback;
		mixer.outputSpec.userdata = nullptr;
	
		/* open audio for output */
		if (SDL_OpenAudio(&mixer.outputSpec, nullptr) != 0)
		{
			std::cerr << "Opening audio failed\n";
			sound_ok = false;
		}
	#endif

		set_music_volume(user_music_volume);
	}

	Manager::~Manager()
	{
		if(preferences::no_sound()) {
			return;
		}

		loading_threads.clear();

	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		Mix_HookMusicFinished(nullptr);
		next_music().name.clear();
		Mix_CloseAudio();
	#else
		iphone_kill_music();
	#endif
	}

	void init_music(variant node)
	{
		for(variant music_node : node["music"].as_list()) {
			const std::string name = music_node["name"].as_string();
			music_index[name].volume = music_node["volume"].as_float(1.0f);
		}
	}

	bool ok() { return sound_ok; }
	bool muted() { return mute_; }

	void mute (bool flag)
	{
		mute_ = flag;
	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		Mix_VolumeMusic(static_cast<int>(user_music_volume*track_music_volume*engine_music_volume*MIX_MAX_VOLUME*(!flag)));
	#endif
	}

	void preload(const std::string& file)
	{
		if(!sound_ok) {
			return;
		}

		if(loading_threads.count(file)) {
			return;
		}

		std::shared_ptr<threading::thread> t(new threading::thread("sounds", std::bind(thread_load, file)));
		loading_threads[file] = t;
	}

	namespace {

	int play_internal(const std::string& file, int loops, const void* object, float volume, float fade_in_time)
	{
		if(!sound_ok) {
			return -1;
		}

		if(!cache.count(file)) {
			preload(file);
			queued_sounds.push_back(sound_playing());
			queued_sounds.back().file = file;
			queued_sounds.back().loops = loops;
			queued_sounds.back().object = object;
			queued_sounds.back().volume = volume;
			queued_sounds.back().fade_in_time = fade_in_time;
		
			return -1;
		}

	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		Mix_Chunk* chunk = cache[file];
		if(chunk == nullptr) {
			ASSERT_LOG(!g_assert_on_missing_sound, "FATAL: Sound file: " << file << " missing");
			return -1;
		}

		int result = Mix_PlayChannel(-1, chunk, loops);
		if(result >= 0) {
			Mix_SetPanning(result, static_cast<uint8_t>(255*g_stereo_left), static_cast<uint8_t>(255*g_stereo_right));
		}

	#else
		sound& s = cache[file];
		if(s == nullptr) {
			return -1;
		}
	
		int result = sdl_playSound(&s, loops);
	#endif

		//record which channel the sound is playing on.
		if(result >= 0) {
			if(static_cast<int>(channels_to_sounds_playing.size()) <= result) {
				channels_to_sounds_playing.resize(result + 1);
			}
	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
			if(fade_in_time == 0.0f) {
				Mix_Volume(result, static_cast<int>(volume*sfx_volume*MIX_MAX_VOLUME)); //start sound at full volume
			} else {
				Mix_Volume(result, 0); 
			}
	#endif

			channels_to_sounds_playing[result].file = file;
			channels_to_sounds_playing[result].object = object;
			channels_to_sounds_playing[result].loops = loops;
			channels_to_sounds_playing[result].volume = volume;
			channels_to_sounds_playing[result].fade_in_time = fade_in_time;
			channels_to_sounds_playing[result].time_cnt = 0.0f;
			channels_to_sounds_playing[result].fade_out_time = 0.0f;
		}

		return result;
	}

	}

	void process()
	{
		bool has_items = false;
		{
			threading::lock l(cache_mutex);
			for(cache_map::const_iterator i = threaded_cache.begin(); i != threaded_cache.end(); ++i) {
				cache.insert(*i);
				has_items = true;
				loading_threads.erase(i->first);
			}

			threaded_cache.clear();
		}

		if(has_items) {
			std::vector<sound_playing> sounds;
			sounds.swap(queued_sounds);
			for(const sound_playing& sfx : sounds) {
				play_internal(sfx.file, sfx.loops, sfx.object, sfx.volume, sfx.fade_in_time);
			}
		}

		for(int n = 0; n != channels_to_sounds_playing.size(); ++n) {
	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
			sound_playing& snd = channels_to_sounds_playing[n];
			snd.time_cnt += 0.02f;
			if(snd.fade_in_time > 0.0f) {
				float volume = snd.volume;
				if(snd.time_cnt > snd.fade_in_time) {
					snd.time_cnt = 0.0f;
					snd.fade_in_time = 0.0f;
				} else {
					volume *= snd.time_cnt / snd.fade_in_time;
				}
				//std::cerr << "Sound: " << snd.file << ", " << snd.fade_in_time << " : " << snd.time_cnt << "  " << (volume * sfx_volume) << std::endl;
				Mix_Volume(n, static_cast<int>(volume * sfx_volume * MIX_MAX_VOLUME));
			}
			if(snd.fade_out_time > 0.0f) {
				if(snd.time_cnt >= snd.fade_out_time) {
					snd.fade_out_time = 0.0f;
					snd.object = nullptr;
					Mix_HaltChannel(n);
				} else {
					float volume = snd.volume;
					volume *= (1.0f - snd.time_cnt / snd.fade_out_time);
					Mix_Volume(n, static_cast<int>(volume * sfx_volume * MIX_MAX_VOLUME));
				}
			}
		}
	#endif
	}

	void play(const std::string& file, const void* object, float volume, float fade_in_time)
	{
		if(preferences::no_sound() || mute_) {
			return;
		}

		play_internal(file, 0, object, volume, fade_in_time);
	}

	void stop_sound(const std::string& file, const void* object, float fade_out_time)
	{
		for(int n = 0; n != channels_to_sounds_playing.size(); ++n) {
			if(channels_to_sounds_playing[n].object == object &&
			   channels_to_sounds_playing[n].file == file) {
				if(fade_out_time == 0.0f) {
					channels_to_sounds_playing[n].object = nullptr;
				}
				channels_to_sounds_playing[n].fade_out_time = fade_out_time;
				channels_to_sounds_playing[n].time_cnt = 0.0f;
	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
				if(fade_out_time == 0.0f) {
					Mix_HaltChannel(n);
				}
	#else
				sdl_stop_channel(n);
	#endif
			}
		}

		for(int n = 0; n != queued_sounds.size(); ++n) {
			if(queued_sounds[n].object == object &&
			   queued_sounds[n].file == file) {
				queued_sounds.erase(queued_sounds.begin() + n);
				--n;
			}
		}
	}
	
	void stop_looped_sounds(const void* object)
	{
		for(int n = 0; n != channels_to_sounds_playing.size(); ++n) {
			if(((object == nullptr && channels_to_sounds_playing[n].object != nullptr)
			   || channels_to_sounds_playing[n].object == object) &&
			   (channels_to_sounds_playing[n].loops != 0)) {
	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
				if(g_mixer_looped_sounds_fade_time_ms > 0) {
					channels_to_sounds_playing[n].fade_out_time = g_mixer_looped_sounds_fade_time_ms/1000.0f;
					channels_to_sounds_playing[n].time_cnt = 0.0f;
				} else {
					Mix_HaltChannel(n);
					channels_to_sounds_playing[n].object = nullptr;
				}
	#else
				sdl_stop_channel(n);
				channels_to_sounds_playing[n].object = nullptr;
	#endif
			} else if(channels_to_sounds_playing[n].object == object) {
				//this sound is a looped sound, but make sure it keeps going
				//until it ends, since this function signals that the associated
				//object is going away.
				channels_to_sounds_playing[n].object = nullptr;
			}
		}

		for(int n = 0; n != queued_sounds.size(); ++n) {
			if(((object == nullptr && queued_sounds[n].object != nullptr)
			   || queued_sounds[n].object == object) &&
			   (queued_sounds[n].loops != 0)) {
				queued_sounds.erase(queued_sounds.begin() + n);
				--n;
			} else if(queued_sounds[n].object == object) {
				//this sound is a looped sound, but make sure it keeps going
				//until it ends, since this function signals that the associated
				//object is going away.
				queued_sounds[n].object = nullptr;
			}
		}
	}
	
	int play_looped(const std::string& file, const void* object, float volume, float fade_in_time)
	{
		if(preferences::no_sound() || mute_) {
			return -1;
		}


		const int result = play_internal(file, -1, object, volume, fade_in_time);
		LOG_DEBUG("PLAY: " << object << " " << file << " -> " << result);
		return result;
	}

	void change_volume(const void* object, float volume_f)
	{
		int volume = int(volume_f*128.0);
		//Note - range is 0-128 (MIX_MAX_VOLUME).  Truncate:
		if( volume > 128){
			volume = 128;
		} else if ( volume < 0 ){
			volume = 0;
		}
	
		//find the channel associated with this object.
		for(int n = 0; n != channels_to_sounds_playing.size(); ++n) {
			if(channels_to_sounds_playing[n].object == object) {
	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
				if(channels_to_sounds_playing[n].fade_in_time > 0.0f) {
					channels_to_sounds_playing[n].fade_in_time = 0.0f;
					channels_to_sounds_playing[n].time_cnt = 0.0f;
				}
                channels_to_sounds_playing[n].volume = volume/128.0;
				Mix_Volume(n, static_cast<int>(sfx_volume*volume));
	#else
                channels_to_sounds_playing[n].volume = volume/128.0;
				mixer.channels[n].volume = sfx_volume*volume;
	#endif
			} //else, we just do nothing
		}
	}

	float get_sound_volume()
	{
		return sfx_volume;
	}

	void set_sound_volume(float volume)
	{
		sfx_volume = volume;
	}

	float get_music_volume()
	{
		return user_music_volume;
	}

	namespace 
	{
		void update_music_volume()
		{
			if(sound_init) {
		#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
				Mix_VolumeMusic(static_cast<int>(track_music_volume*user_music_volume*engine_music_volume*MIX_MAX_VOLUME));
		#else
				iphone_set_music_volume(track_music_volume*user_music_volume*engine_music_volume);
		#endif
			}
		}
	}

	void set_music_volume(float volume)
	{
		user_music_volume = volume;
		update_music_volume();
	}

	void set_engine_music_volume(float volume)
	{
		engine_music_volume = volume;
		update_music_volume();
	}

	float get_engine_music_volume()
	{
		return engine_music_volume;
	}

	void set_panning(float left, float right)
	{
		g_stereo_left = left;
		g_stereo_right = right;
	}

	void update_panning(const void* obj, const std::string& id, float left, float right)
	{
		for(int n = 0; n != channels_to_sounds_playing.size(); ++n) {
			const sound_playing& s = channels_to_sounds_playing[n];
			if(s.object == obj && s.file == id) {
				Mix_SetPanning(n, static_cast<uint8_t>(255*left), static_cast<uint8_t>(255*right));
			}
		}
	}

	namespace 
	{
		std::map<std::string,std::string>& get_music_paths() {
			static std::map<std::string,std::string> res;
			return res;
		}
	}

	void load_music_paths() {
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
			#define MUSIC_DIR_NAME "music_aac/"
#else
			#define MUSIC_DIR_NAME "music/"
#endif
		module::get_unique_filenames_under_dir(MUSIC_DIR_NAME, &get_music_paths());
	}

	void play_music(const std::string& file, bool queue, int fade_time)
	{
		if(preferences::no_sound() || preferences::no_music() || !sound_ok) {
			return;
		}

		if(file == current_music_name()) {
			return;
		}

		std::string song_file = file;

	#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
		song_file.replace(song_file.length()-3, song_file.length(), "m4a");
	#endif

		if(get_music_paths().empty()) {
			load_music_paths();
		}

		std::map<std::string, std::string>::const_iterator itor = module::find(get_music_paths(), song_file);
		if(itor == get_music_paths().end()) {
			ASSERT_LOG(!g_assert_on_missing_sound, "FATAL: Music file: " << song_file << " missing");
			LOG_ERROR("FILE NOT FOUND: " << song_file);
			return;
		}
		const std::string& path = itor->second;

	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		if(current_mix_music) {
			next_music().name = file;
			next_music().fade_time = fade_time;

			if(!queue) {
				Mix_FadeOutMusic(fade_time);
			}
			return;
		}

		current_music_name() = file;
	#if defined(__ANDROID__)
		current_mix_music = Mix_LoadMUS_RW(sys::read_sdl_rw_from_asset(path.c_str()));
	#else
		current_mix_music = Mix_LoadMUS(path.c_str());
	#endif
		if(!current_mix_music) {
			LOG_ERROR("Mix_LoadMUS ERROR loading " << path << ": " << Mix_GetError());
			return;
		}

		track_music_volume = music_index[file].volume;
		update_music_volume();

		if(fade_time > 0) {
			Mix_FadeInMusic(current_mix_music, -1, fade_time);
		} else {
			Mix_PlayMusic(current_mix_music, -1);
		}
	#else
		if (playing_music)
		{
			next_music().name = file;
			next_music().fade_time = fade_time;
			iphone_fade_out_music(fade_time);
			return;
		}

		if(file.empty()) {
			return;
		}
	
		current_music_name() = file;
		if (!sys::file_exists(path)) return;
		track_music_volume = music_index[file].volume;
		update_music_volume();
		iphone_play_music((path).c_str(), -1);
		iphone_fade_in_music(350);
		playing_music = true;
	#endif
	}

	void play_music_interrupt(const std::string& file)
	{
		if(preferences::no_sound() || preferences::no_music()) {
			return;
		}

		if(next_music().name.empty() == false) {
			current_music_name() = next_music().name;
			next_music().name.clear();
		}
	
		next_music().name = current_music_name();
	
		std::string song_file = file;
    
	#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
		song_file.replace(song_file.length()-3, song_file.length(), "m4a");
	#endif

		current_music_name() = file;


		if(get_music_paths().empty()) {
			load_music_paths();
		}

		std::map<std::string, std::string>::const_iterator itor = module::find(get_music_paths(), song_file);
		if(itor == get_music_paths().end()) {
			LOG_ERROR("FILE NOT FOUND: " << song_file);
			return;
		}
		const std::string& path = itor->second;

	#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
		//note that calling HaltMusic will result in on_music_finished being
		//called, which releases the current_music pointer.
		Mix_HaltMusic();
		if(file.empty()) {
			return;
		}

	#if defined(__ANDROID__)
		current_mix_music = Mix_LoadMUS_RW(sys::read_sdl_rw_from_asset(path.c_str()));
	#else
		current_mix_music = Mix_LoadMUS(path.c_str());
	#endif

		if(!current_mix_music) {
			LOG_ERROR("Mix_LoadMUS ERROR loading " << file << ": " << Mix_GetError());
			return;
		}

		Mix_PlayMusic(current_mix_music, 1);
	#else
		iphone_play_music((path).c_str(), 0);
		playing_music = true;
	#endif
	}

	const std::string& current_music() 
	{
		return current_music_name();
	}
}

#endif
