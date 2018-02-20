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

#include <assert.h>

#include <iostream>
#include <map>
#include <cmath>
#include <vector>

#include "SDL_mixer.h"
#include "SDL_loadso.h"

#include <vorbis/vorbisfile.h>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "thread.hpp"
#include "unit_test.hpp"
#include "utils.hpp"

#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#include "iphone_sound.h"
#endif

#include "variant_utils.hpp"

PREF_INT(mixer_looped_sounds_fade_time_ms, 100, "Number of milliseconds looped sounds should fade for");
PREF_INT(audio_cache_size_mb, 30, "Audio data cache size in megabytes");

PREF_BOOL(debug_visualize_audio, false, "Show a graph of audio data");

namespace sound 
{
	namespace 
	{
		typedef int (*ov_clear_type)(OggVorbis_File *vf);
		typedef vorbis_info *(*ov_info_type)(OggVorbis_File *vf,int link);
		typedef int (*ov_fopen_type)(const char *path,OggVorbis_File *vf);
		typedef int (*ov_open_type)(FILE *f,OggVorbis_File *vf,const char *initial,long ibytes);
		typedef int (*ov_open_callbacks_type)(void *datasource, OggVorbis_File *vf, const char *initial, long ibytes, ov_callbacks callbacks);
		typedef ogg_int64_t (*ov_pcm_total_type)(OggVorbis_File *vf,int i);
		typedef long (*ov_read_type)(OggVorbis_File *vf,char *buffer,int length, int bigendianp,int word,int sgned,int *bitstream);
		typedef int (*ov_time_seek_type)(OggVorbis_File *vf,double pos);
		typedef double (*ov_time_total_type)(OggVorbis_File *vf,int i);
		typedef double (*ov_time_tell_type)(OggVorbis_File *vf);

	//The sample rate we use for all sounds.
	const int SampleRate = 44100;
	const double SampleRateDouble = double(SampleRate);
	const int NumChannels = 2;

	struct MusicInfo 
	{
		MusicInfo() : volume(1.0f) {}
		float volume;
	};

	//Name of the current music being played. Accessed only by the game thread.
	std::string g_current_music;

	std::map<std::string,std::string>& get_music_paths() 
	{
		static std::map<std::string,std::string> res;
		if(res.empty()) {
			module::get_unique_filenames_under_dir("music/", &res);
		}
		return res;
	}


	struct VorbisDynamicLoad
	{
		VorbisDynamicLoad()
			: loaded(0),
			  handle(nullptr),
			  ov_clear(nullptr),
			  ov_info(nullptr),
			  ov_fopen(nullptr),
			  ov_open(nullptr),
			  ov_open_callbacks(nullptr),
			  ov_pcm_total(nullptr),
			  ov_read(nullptr),
			  ov_time_seek(nullptr),
			  ov_time_total(nullptr),
			  ov_time_tell(nullptr)
		{
#if defined(OGG_DYNAMIC)
			if(loaded == 0) {
				handle = SDL_LoadObject(OGG_DYNAMIC);
				if(handle == nullptr) {
					return;
				}
				ov_clear = static_cast<ov_clear_type>(SDL_LoadFunction(handle, "ov_clear"));
				if(ov_clear == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_info = static_cast<ov_info_type>(SDL_LoadFunction(handle, "ov_info"));
				if(ov_info == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_fopen = static_cast<ov_fopen_type>(SDL_LoadFunction(handle, "ov_fopen"));
				if(ov_fopen == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_open = static_cast<ov_open_type>(SDL_LoadFunction(handle, "ov_open"));
				if(ov_open == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_open_callbacks = static_cast<ov_open_callbacks_type>(SDL_LoadFunction(handle, "ov_open_callbacks"));
				if(ov_open_callbacks == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_pcm_total = static_cast<ov_pcm_total_type>(SDL_LoadFunction(handle, "ov_pcm_total"));
				if(ov_pcm_total == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_read = static_cast<ov_read_type>(SDL_LoadFunction(handle, "ov_read"));
				if(ov_read == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_time_seek = static_cast<ov_time_seek_type>(SDL_LoadFunction(handle, "ov_time_seek"));
				if(ov_time_seek == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_time_total = static_cast<ov_time_total_type>(SDL_LoadFunction(handle, "ov_time_total"));
				if(ov_time_total == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
				ov_time_tell = static_cast<ov_time_tell_type>(SDL_LoadFunction(handle, "ov_time_tell"));
				if(ov_time_tell == nullptr) {
					SDL_UnloadObject(handle);
					return;
				}
			}
			++loaded;
#else
			ov_clear = ::ov_clear;
			ov_info = ::ov_info;
			ov_fopen = ::ov_fopen;
			ov_open = ::ov_open;
			ov_open_callbacks = ::ov_open_callbacks;
			ov_pcm_total = ::ov_pcm_total;
			ov_read = ::ov_read;
			ov_time_seek = ::ov_time_seek;
			ov_time_total = ::ov_time_total;
			ov_time_tell = ::ov_time_tell;
#endif
		}

		~VorbisDynamicLoad()
		{
			if(loaded > 0) {
				--loaded;
				if(loaded == 0) {
					SDL_UnloadObject(handle);
					handle = nullptr;
				}
			}
		}

		int loaded;
		void *handle;
		ov_clear_type ov_clear;
		ov_info_type ov_info;
		ov_fopen_type ov_fopen;
		ov_open_type ov_open;
		ov_open_callbacks_type ov_open_callbacks;
		ov_pcm_total_type ov_pcm_total;
		ov_read_type ov_read;
		ov_time_seek_type ov_time_seek;
		ov_time_total_type ov_time_total;
		ov_time_tell_type ov_time_tell;
	};
	
	VorbisDynamicLoad& vorbis()
	{
		static VorbisDynamicLoad res;
		return res;
	}

	//Object capable of decoding ogg music.
	class MusicPlayer : public game_logic::FormulaCallable
	{
	public:

		//Should be constructed in the main thread.
		explicit MusicPlayer(const std::string& file, variant options)
		   : fname_(file), bit_stream_(0), volume_(options["volume"].as_float(1.0f)), finished_(false), fadeout_time_(-1.0f), fadeout_current_(-1.0f),
			 loop_(options["loop"].as_bool(false)),
			 loop_point_(options["loop_point"].as_double(0.0)),
			 loop_from_(options["loop_from"].as_double(-1.0)),
			 on_complete_(options["on_complete"]) {
			auto itor = module::find(get_music_paths(), file);
			ASSERT_LOG(itor != get_music_paths().end(), "Could not find path for file: " << file);
			const int res = vorbis().ov_fopen(itor->second.c_str(), &file_);
			ASSERT_LOG(res == 0, "Failed to read vorbis file: " << file);

			info_ = vorbis().ov_info(&file_, -1);
			ASSERT_LOG(info_->channels == 1 || info_->channels == 2, "Ogg file " << file << " has unsupported number of channels: " << info_->channels << ". Only support mono and stereo");

			variant pos = options["pos"];
			if(pos.is_decimal()) {
				int res = vorbis().ov_time_seek(&file_, pos.as_double());
				ASSERT_LOG(res == 0, "Failed to seek music: " << res);
			}
		}

		~MusicPlayer() {
			vorbis().ov_clear(&file_);
		}

		void onComplete() {
			executeCommand(on_complete_);
		}

		bool stopping() const {
			threading::lock lck(mutex_);
			return fadeout_time_ >= 0.0f;
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

			long nbytes;
		
			float fadeout_time = 0.0f;
			float fadeout_current = 0.0f;
			int nsamples;
			{
				threading::lock lck(mutex_);

				bool force_seek = false;

				if(loop_ && loop_from_ > 0.0) {
					double cur = vorbis().ov_time_tell(&file_);
					if(cur <= loop_from_) {
						double time_until_loop = loop_from_ - cur;
						int samples_until_loop = static_cast<int>(time_until_loop*SampleRate);
						if(samples_until_loop <= 0) {
							int res = vorbis().ov_time_seek(&file_, loop_point_);
							ASSERT_LOG(res == 0, "Failed to seek music: " << res << " seek to " << loop_point_);
						} else if(samples_until_loop < out_nsamples) {
							out_nsamples = samples_until_loop;
							force_seek = true;
						}
					}
				
				}

				int max_needed = out_nsamples*sizeof(short)/(numChannels() == 1 ? 2 : 1);

				int nread = std::min<int>(sizeof(buf), max_needed);

				nbytes = vorbis().ov_read(&file_, buf, nread, 0, 2, 1, &bit_stream_);

				if(nbytes == 0 && loop_) {
					int res = vorbis().ov_time_seek(&file_, loop_point_);
					ASSERT_LOG(res == 0, "Failed to seek music: " << res << " seek to " << loop_point_);
					nbytes = vorbis().ov_read(&file_, buf, nread, 0, 2, 1, &bit_stream_);
				} else if(force_seek && nbytes == nread) {
					int res = vorbis().ov_time_seek(&file_, loop_point_);
					ASSERT_LOG(res == 0, "Failed to seek music: " << res << " seek to " << loop_point_);
				}
		
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
						fadeout_current += 1.0f/float(SampleRate);
					}
				} else {
					assert(nsamples*2 <= out_nsamples);
					for(int n = 0; n != nsamples; ++n) {
						*out++ += (1.0f - fadeout_current/fadeout_time)*volume_*static_cast<float>(data[n])/SHRT_MAX;
						*out++ += (1.0f - fadeout_current/fadeout_time)*volume_*static_cast<float>(data[n])/SHRT_MAX;
						fadeout_current += 1.0f/float(SampleRate);
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

		const std::string& fname() const { return fname_; }

		//Length of the track in seconds
		double timeLength() const {
			threading::lock lck(mutex_);
			return vorbis().ov_time_total(const_cast<OggVorbis_File*>(&file_), -1);
		}

		//current position in the track in seconds.
		double timeCurrent() const {
			threading::lock lck(mutex_);
			return vorbis().ov_time_tell(const_cast<OggVorbis_File*>(&file_));
		}

		//Move the current play position. Can be called from any thread.
		void seekTime(double t) {
			threading::lock lck(mutex_);
			int res = vorbis().ov_time_seek(&file_, t);
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

		std::string fname_;

		OggVorbis_File file_;
		vorbis_info* info_;
		int bit_stream_;

		threading::mutex mutex_;

		float volume_;

		//will only be set by the music thread, after which it won't touch the object again.
		//then the music player can be released.
		bool finished_;

		float fadeout_time_, fadeout_current_;

		bool loop_;

		double loop_point_, loop_from_;

		variant on_complete_;
	};


	//Current music being played. This will usually have one music player in it
	//but may have more if we are crossfading to different music.
	//Access is controlled by g_music_thread_mutex. Only the game thread will
	//modify this, so it can read from it without locking the mutex.
	std::vector<ffl::IntrusivePtr<MusicPlayer> > g_music_players;

	//The current member of g_music_players which is playing and not currently being
	//faded out. This is accessed only from the game thread and is safe to access from
	//there without fear it might change at any moment.
	ffl::IntrusivePtr<MusicPlayer> g_current_player;

	//Flag to tell the music thread to exit. Access controlled by g_music_thread_mutex.
	bool g_music_thread_exit = false;

	//Mutex used to control access to g_music_players and g_music_thread_exit.
	threading::mutex g_music_thread_mutex;

	//Next music track to  play. Only accessed from the game thread.
	ffl::IntrusivePtr<MusicPlayer> g_music_queue;


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

				for(const ffl::IntrusivePtr<MusicPlayer>& p : g_music_players) {
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

	BEGIN_DEFINE_CALLABLE_NOBASE(MusicPlayer)
	DEFINE_FIELD(filename, "string")
		return variant(obj.fname_);
	DEFINE_FIELD(loop, "bool")
		return variant::from_bool(obj.loop_);
	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		obj.loop_ = value.as_bool();

	DEFINE_FIELD(loop_point, "decimal|null")
		if(obj.loop_point_ <= 0) {
			return variant();
		} else {
			return variant(obj.loop_point_);
		}
	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		if(value.is_null()) {
			obj.loop_point_ = 0;
		} else {
			obj.loop_point_ = value.as_double();
		}

	DEFINE_FIELD(loop_from, "decimal|null")
		if(obj.loop_from_ <= 0) {
			return variant();
		} else {
			return variant(obj.loop_from_);
		}
	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		if(value.is_null()) {
			obj.loop_from_ = 0;
		} else {
			obj.loop_from_ = value.as_double();
		}

	DEFINE_FIELD(duration, "decimal")
		return variant(obj.timeLength());

	DEFINE_FIELD(on_complete, "commands|function()->commands")
		return obj.on_complete_;

	DEFINE_SET_FIELD
		obj.on_complete_ = value;

	DEFINE_FIELD(pos, "decimal")
		return variant(obj.timeCurrent());

	DEFINE_SET_FIELD
		obj.seekTime(value.as_double());


	BEGIN_DEFINE_FN(play, "()->commands")
		ffl::IntrusivePtr<MusicPlayer> player(const_cast<MusicPlayer*>(&obj));
		return variant(new game_logic::FnCommandCallable("sound::play", [=]() {
			threading::lock lck(g_music_thread_mutex);
			for(auto& p : g_music_players) {
				if(player == p) {
					return;
				}
			}

			for(auto& p : g_music_players) {
				if(!p->stopping()) {
					p->stopPlaying(1.0f);
				}
			}
			g_music_players.push_back(player);
			g_current_player = player;
		}));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(stop, "(decimal=1.0)->commands")
		float fade_time = 1.0f;
		if(NUM_FN_ARGS > 0) {
			fade_time = FN_ARG(0).as_float();
		}
		ffl::IntrusivePtr<MusicPlayer> ptr(const_cast<MusicPlayer*>(&obj));
		return variant(new game_logic::FnCommandCallable("sound::stop", [=]() {
			ptr->stopPlaying(fade_time);
		}));
	END_DEFINE_FN
	END_DEFINE_CALLABLE(MusicPlayer)

	//Function to load an ogg vorbis file into a buffer. Fills 'spec' with the
	//specs of the format loaded.
	bool loadVorbis(const char* file, SDL_AudioSpec* spec, std::vector<char>& buf)
	{
		OggVorbis_File ogg_file;

		int res = vorbis().ov_fopen(file, &ogg_file);
		if(res != 0) {
			return false;
		}

		vorbis_info* info = vorbis().ov_info(&ogg_file, -1);

		spec->freq = info->rate;
		spec->channels = info->channels;
		spec->format = AUDIO_S16;
		spec->silence = 0;

		int bit_stream = 0;

		long nbytes = 1;
		while(nbytes > 0) {
			const int buf_len = 4096;
			buf.resize(buf.size() + buf_len);
			nbytes = vorbis().ov_read(&ogg_file, &buf[0] + buf.size() - buf_len, buf_len, 0, 2, 1, &bit_stream);
			buf.resize(buf.size() - 4096 + nbytes);
		}

		spec->samples = buf.size()/4;
		spec->size = buf.size();
		spec->callback = nullptr;
		spec->userdata = nullptr;

		vorbis().ov_clear(&ogg_file);

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
		WaveData(const std::string& filename, std::vector<short>* buf, int nchan) : fname(filename), buffer(new short [buf->size()]), buffer_size(buf->size()), nchannels(nchan) {
			memcpy(buffer, &(*buf)[0], buffer_size*sizeof(short));
		}
		~WaveData() {
			delete [] buffer;
		}
		std::string fname;
		short* buffer;
		size_t buffer_size;
		int nchannels;
		size_t nsamples() const { return buffer_size/nchannels; }

		size_t memoryUsage() const { return buffer_size*sizeof(short); }
	};

	//set of files that are loading or loaded along with a mutex to control
	//them. A file will not be loaded if it is included in this set since it's
	//assumed already loaded.
	threading::mutex g_files_loading_mutex;
	std::set<std::string> g_files_loading;

	//A cache of loaded sound effects with synchronization.
	typedef std::list<std::shared_ptr<WaveData>> WaveCacheLRUList;
	WaveCacheLRUList g_wave_cache_lru;
	std::map<std::string, WaveCacheLRUList::iterator> g_wave_cache;
	size_t g_wave_cache_size;
	threading::mutex g_wave_cache_mutex;

	bool get_cached_wave(const std::string& fname, std::shared_ptr<WaveData>* ptr)
	{
		threading::lock lck(g_wave_cache_mutex);
		auto itor = g_wave_cache.find(fname);
		if(itor == g_wave_cache.end()) {
			return false;
		} else {
			if(itor->second == g_wave_cache_lru.end()) {
				ptr->reset();
			} else {
				if(itor->second != g_wave_cache_lru.begin()) {
					g_wave_cache_lru.splice(g_wave_cache_lru.begin(), g_wave_cache_lru, itor->second);
				}
				*ptr = *itor->second;
			}
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
		spec.format = AUDIO_S16;
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
			LOG_ERROR("Could not load sound: " << fname << " " << SDL_GetError());

			threading::lock lck(g_wave_cache_mutex);
			g_wave_cache[fname] = g_wave_cache_lru.end();
		} else {

			SDL_AudioCVT cvt;
			spec.channels = res_spec->channels;
			int res = SDL_BuildAudioCVT(&cvt, res_spec->format, res_spec->channels, res_spec->freq, spec.format, spec.channels, spec.freq);
			ASSERT_LOG(res >= 0, "Could not convert audio: " << SDL_GetError());

			std::vector<short> out_buf;

			if(res == 0) {
				out_buf.resize(len/sizeof(short));
				if(!out_buf.empty()) {
					memcpy(&out_buf[0], buf, out_buf.size()*sizeof(short));
				}

			} else {
				cvt.len = len;

				std::vector<Uint8> tmp_buf;
				tmp_buf.resize(cvt.len * cvt.len_mult);

				memcpy(&tmp_buf[0], buf, len);

				cvt.buf = &tmp_buf[0];

				int res = SDL_ConvertAudio(&cvt);
				ASSERT_LOG(res >= 0, "Could not convert audio: " << SDL_GetError());

				out_buf.resize(cvt.len_cvt/sizeof(short));
				memcpy(&out_buf[0], cvt.buf, cvt.len_cvt);
			}

			if(ogg_buf.empty()) {
				SDL_FreeWAV(buf);
			}

			std::shared_ptr<WaveData> data(new WaveData(fname, &out_buf, spec.channels));

			threading::lock lck(g_wave_cache_mutex);
			g_wave_cache_lru.push_front(data);
			g_wave_cache[fname] = g_wave_cache_lru.begin();
			g_wave_cache_size += data->memoryUsage();

			int nlive = 0;
			int nactive = 0;
			for(auto& p : g_wave_cache_lru) {
				if(p.unique() == false) {
					nlive += p->memoryUsage();
					++nactive;
				}
			}

			LOG_INFO("Added wave: " << fname << " Have " << g_wave_cache_lru.size() << " items in cache, size " << (g_wave_cache_size/(1024*1024)) << "MB, " << nactive << " items live, " << (nlive/(1024*1024)) << "MB\n");

			while(g_wave_cache_size >= static_cast<size_t>(g_audio_cache_size_mb*1024*1024)) {
				assert(!g_wave_cache_lru.empty());

				for(int n = 0; n < int(g_wave_cache_lru.size()) && g_wave_cache_lru.back().unique() == false; ++n) {
					g_wave_cache_lru.splice(g_wave_cache_lru.begin(), g_wave_cache_lru, std::prev(g_wave_cache_lru.end()));
				}

				if(g_wave_cache_lru.back().unique() == false) {
					LOG_ERROR("Audio cache size exceeded but all " << g_wave_cache_lru.size() << " items in use cannot evict");
					break;
				}

				g_wave_cache_size -= g_wave_cache_lru.back()->memoryUsage();

				{
					threading::lock lck(g_files_loading_mutex);
					g_files_loading.erase(g_wave_cache_lru.back()->fname);
				}

				g_wave_cache.erase(g_wave_cache_lru.back()->fname);
				g_wave_cache_lru.erase(std::prev(g_wave_cache_lru.end()));

			}
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

	class SoundSource : public game_logic::FormulaCallable
	{
	public:
		virtual ~SoundSource() {}
		virtual void MixData(float* output, int nsamples) = 0;

		virtual bool finished() const = 0;

		DECLARE_CALLABLE(SoundSource);
	};

	BEGIN_DEFINE_CALLABLE_NOBASE(SoundSource)
	END_DEFINE_CALLABLE(SoundSource)

	class SoundEffectFilter : public SoundSource
	{
	public:
		explicit SoundEffectFilter(variant node) : source_(nullptr) {
		}
		virtual ~SoundEffectFilter() {}

		void setSource(SoundSource* source) { source_ = source; }

		bool finished() const override {
			return source_->finished();
		}

		virtual SoundEffectFilter* clone() const = 0;

		virtual std::string debug_description() const = 0;

	protected:
		void GetData(float* output, int nsamples) {
			if(source_ != nullptr) {
				source_->MixData(output, nsamples);
			}
		}
	private:
		DECLARE_CALLABLE(SoundEffectFilter);
		SoundSource* source_;

		variant userdata_;
	};

	BEGIN_DEFINE_CALLABLE(SoundEffectFilter, SoundSource)
	DEFINE_FIELD(userdata, "any")
		return obj.userdata_;
	DEFINE_SET_FIELD
		obj.userdata_ = value;
	END_DEFINE_CALLABLE(SoundEffectFilter)

	enum BiquadFilterType {
		bq_type_lowpass = 0,
		bq_type_highpass,
		bq_type_bandpass,
		bq_type_notch,
		bq_type_peak,
		bq_type_lowshelf,
		bq_type_highshelf
	};

	class Biquad {
	public:
		Biquad(BiquadFilterType type, variant node);
		~Biquad();
		void setType(BiquadFilterType type);
		void setQ(float Q);
		void setFc(float Fc);
		void setPeakGain(float peakGainDB);
		void setBiquad(BiquadFilterType type, float Fc, float Q, float peakGain);
		float process(int nchannel, float in);
		
	protected:
		void calcBiquad(void);

		BiquadFilterType type_;
		float a0, a1, a2, b1, b2;
		float Fc, Q, peakGain;

		float z1_[NumChannels], z2_[NumChannels];
	};

	inline float Biquad::process(int nchannel, float in) {
		float out = in * a0 + z1_[nchannel];
		z1_[nchannel] = in * a1 + z2_[nchannel] - b1 * out;
		z2_[nchannel] = in * a2 - b2 * out;
		return out;
	}

	Biquad::Biquad(BiquadFilterType t, variant node) {
		setBiquad(t, node["fc"].as_double(4000.0)/SampleRate, node["q"].as_double(0.707), node["peak_gain"].as_double(1.0));
		for(int n = 0; n != NumChannels; ++n) {
			z1_[n] = z2_[n] = 0.0;
		}
	}

	Biquad::~Biquad() {
	}

	void Biquad::setType(BiquadFilterType type) {
		type_ = type;
		calcBiquad();
	}

	void Biquad::setQ(float Q) {
		this->Q = Q;
		calcBiquad();
	}

	void Biquad::setFc(float Fc) {
		this->Fc = Fc;
		calcBiquad();
	}

	void Biquad::setPeakGain(float peakGainDB) {
		this->peakGain = peakGainDB;
		calcBiquad();
	}
		
	void Biquad::setBiquad(BiquadFilterType type, float Fc, float Q, float peakGainDB) {
		this->type_ = type;
		this->Q = Q;
		this->Fc = Fc;
		setPeakGain(peakGainDB);
	}

	void Biquad::calcBiquad(void) {
		float norm;
		float V = pow(10, fabs(peakGain) / 20.0);
		float K = tan(M_PI * Fc);
		switch (this->type_) {
			case bq_type_lowpass:
				norm = 1 / (1 + K / Q + K * K);
				a0 = K * K * norm;
				a1 = 2 * a0;
				a2 = a0;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - K / Q + K * K) * norm;
				break;
				
			case bq_type_highpass:
				norm = 1 / (1 + K / Q + K * K);
				a0 = 1 * norm;
				a1 = -2 * a0;
				a2 = a0;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - K / Q + K * K) * norm;
				break;
				
			case bq_type_bandpass:
				norm = 1 / (1 + K / Q + K * K);
				a0 = K / Q * norm;
				a1 = 0;
				a2 = -a0;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - K / Q + K * K) * norm;
				break;
				
			case bq_type_notch:
				norm = 1 / (1 + K / Q + K * K);
				a0 = (1 + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = a0;
				b1 = a1;
				b2 = (1 - K / Q + K * K) * norm;
				break;
				
			case bq_type_peak:
				if (peakGain >= 0) {    // boost
					norm = 1 / (1 + 1/Q * K + K * K);
					a0 = (1 + V/Q * K + K * K) * norm;
					a1 = 2 * (K * K - 1) * norm;
					a2 = (1 - V/Q * K + K * K) * norm;
					b1 = a1;
					b2 = (1 - 1/Q * K + K * K) * norm;
				}
				else {    // cut
					norm = 1 / (1 + V/Q * K + K * K);
					a0 = (1 + 1/Q * K + K * K) * norm;
					a1 = 2 * (K * K - 1) * norm;
					a2 = (1 - 1/Q * K + K * K) * norm;
					b1 = a1;
					b2 = (1 - V/Q * K + K * K) * norm;
				}
				break;
			case bq_type_lowshelf:
				if (peakGain >= 0) {    // boost
					norm = 1 / (1 + sqrt(2) * K + K * K);
					a0 = (1 + sqrt(2*V) * K + V * K * K) * norm;
					a1 = 2 * (V * K * K - 1) * norm;
					a2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
					b1 = 2 * (K * K - 1) * norm;
					b2 = (1 - sqrt(2) * K + K * K) * norm;
				}
				else {    // cut
					norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
					a0 = (1 + sqrt(2) * K + K * K) * norm;
					a1 = 2 * (K * K - 1) * norm;
					a2 = (1 - sqrt(2) * K + K * K) * norm;
					b1 = 2 * (V * K * K - 1) * norm;
					b2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
				}
				break;
			case bq_type_highshelf:
				if (peakGain >= 0) {    // boost
					norm = 1 / (1 + sqrt(2) * K + K * K);
					a0 = (V + sqrt(2*V) * K + K * K) * norm;
					a1 = 2 * (K * K - V) * norm;
					a2 = (V - sqrt(2*V) * K + K * K) * norm;
					b1 = 2 * (K * K - 1) * norm;
					b2 = (1 - sqrt(2) * K + K * K) * norm;
				}
				else {    // cut
					norm = 1 / (V + sqrt(2*V) * K + K * K);
					a0 = (1 + sqrt(2) * K + K * K) * norm;
					a1 = 2 * (K * K - 1) * norm;
					a2 = (1 - sqrt(2) * K + K * K) * norm;
					b1 = 2 * (K * K - V) * norm;
					b2 = (V - sqrt(2*V) * K + K * K) * norm;
				}
				break;
		}
		
		return;
	}

	class BiQuadSoundEffectFilter : public SoundEffectFilter
	{
	public:
		BiQuadSoundEffectFilter(BiquadFilterType t, variant node) : SoundEffectFilter(node), filter_(t, node)
		{}

		void MixData(float* output, int nsamples) override
		{
			threading::lock lck(mutex_);

			std::vector<float> input;
			input.resize(nsamples*NumChannels);
			GetData(&input[0], nsamples);

			const float* in = &input[0];

			for(int n = 0; n < nsamples; ++n) {
				float left = filter_.process(0, *in++);
				float right = filter_.process(1, *in++);
				*output++ += left;
				*output++ += right;
			}
		}

		SoundEffectFilter* clone() const override { return new BiQuadSoundEffectFilter(*this); }

		std::string debug_description() const override { return "BiQuad"; }
	private:
		threading::mutex mutex_;
		Biquad filter_;
		DECLARE_CALLABLE(BiQuadSoundEffectFilter);
	};

	BEGIN_DEFINE_CALLABLE(BiQuadSoundEffectFilter, SoundEffectFilter)
	END_DEFINE_CALLABLE(BiQuadSoundEffectFilter)

	class SpeedSoundEffectFilter : public SoundEffectFilter
	{
	public:
		explicit SpeedSoundEffectFilter(variant options) : SoundEffectFilter(options), speed_(options["speed"].as_float(1.0f))
		{}

		void MixData(float* output, int nsamples) override
		{
			threading::lock lck(mutex_);
			int source_nsamples = int(nsamples*speed_);

			if(source_nsamples <= 0) {
				return;
			}

			std::vector<float> buf;
			buf.resize(source_nsamples*NumChannels);
			GetData(&buf[0], source_nsamples);
			for(int n = 0; n != nsamples; ++n) {
				const float point = n*speed_;
				const int a = util::clamp<int>(static_cast<int>(floor(point)), 0, source_nsamples - 1);
				const int b = util::clamp<int>(static_cast<int>(ceil(point)), 0, source_nsamples - 1);
				const float ratio = n*speed_ - floor(point);

				output[n*2] += util::mix<float>(buf[a*2], buf[b*2], ratio);
				output[n*2+1] += util::mix<float>(buf[a*2+1], buf[b*2+1], ratio);
			}
		}

		SoundEffectFilter* clone() const override { return new SpeedSoundEffectFilter(*this); }

		std::string debug_description() const override { return (formatter() << "Speed(" << speed_ << ")"); }
	private:
		threading::mutex mutex_;
		float speed_;
		DECLARE_CALLABLE(SpeedSoundEffectFilter);
	};

	BEGIN_DEFINE_CALLABLE(SpeedSoundEffectFilter, SoundEffectFilter)
	DEFINE_FIELD(speed, "decimal")
		return variant(obj.speed_);
	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		obj.speed_ = value.as_float();
	END_DEFINE_CALLABLE(SpeedSoundEffectFilter)

	class BinauralDelaySoundEffectFilter : public SoundEffectFilter
	{
	public:
		explicit BinauralDelaySoundEffectFilter(variant options) : SoundEffectFilter(options), delay_(options["delay"].as_float())
		{}

		SoundEffectFilter* clone() const override { return new BinauralDelaySoundEffectFilter(*this); }

		bool finished() const override {
			threading::lock lck(mutex_);
			return buf_.empty() && SoundEffectFilter::finished();
		}

		void MixData(float* output, int nsamples) override
		{
			threading::lock lck(mutex_);

			std::vector<float> buffer;
			buffer.resize(nsamples*NumChannels);
			GetData(&buffer[0], nsamples);

			const bool left_channel = delay_ < 0.0f;

			//output the unaffected channel
			{
				float* in = &buffer[0];
				float* out = output;
				if(left_channel) {
					++out;
					++in;
				}

				for(int n = 0; n != nsamples; ++n) {
					*out += *in;
					out += 2;
					in += 2;
				}

			}

            const int nsamples_delay = int(std::abs<float>(delay_)*SampleRate);

			//The delayed channel
			{
				float* in = &buffer[0];
				float* out = output;
				float* end_out = output + nsamples*NumChannels;
				if(!left_channel) {
					++out;
					++in;
				}

				buf_.resize(buf_.size() + nsamples);
				float* out_buf = &buf_[0] + buf_.size() - nsamples;

				for(int n = 0; n != nsamples; ++n) {
					*out_buf = *in;
					in += 2;
					out_buf++;
				}

				if(int(buf_.size()) > nsamples_delay) {
					float* in = &buf_[0];
					const int ncopy = std::min<int>(nsamples, int(buf_.size()) - nsamples_delay);
					out += (nsamples - ncopy)*NumChannels;

					for(int n = 0; n != ncopy; ++n) {
						*out += *in;
						++in;
						out += NumChannels;
					}

					buf_.erase(buf_.begin(), buf_.begin() + ncopy);
				}
			}
		}

		std::string debug_description() const override { return (formatter() << "BinauralDelay(" << delay_ << ")"); }
	private:
		threading::mutex mutex_;
		float delay_;
		std::vector<float> buf_;
		DECLARE_CALLABLE(BinauralDelaySoundEffectFilter);
	};

	BEGIN_DEFINE_CALLABLE(BinauralDelaySoundEffectFilter, SoundEffectFilter)
	DEFINE_FIELD(delay, "decimal")
		return variant(obj.delay_);
	END_DEFINE_CALLABLE(BinauralDelaySoundEffectFilter)

	class RawPlayingSound : public SoundSource
	{
	public:
		RawPlayingSound(const std::string& fname, float volume, float fade_in) : fname_(fname), pos_(0), volume_(volume), volume_target_(0.0), volume_target_time_(-1.0), fade_in_(fade_in), looped_(false), loop_point_(0), loop_from_(0), fade_out_(-1.0f), fade_out_current_(0.0f), left_pan_(1.0f), right_pan_(1.0f)
		{
			init();
		}

		RawPlayingSound(const std::string& fname, variant options) : fname_(fname), pos_(int(options["pos"].as_double(0.0)*SampleRate)), volume_(options["volume"].as_float(1.0f)), volume_target_(0.0), volume_target_time_(-1.0), fade_in_(options["fade_in"].as_float(0.0f)), looped_(options["loop"].as_bool(false)), loop_point_(int(options["loop_point"].as_float(0.0f)*SampleRate)), loop_from_(int(options["loop_from"].as_float(0.0f)*SampleRate)), fade_out_(-1.0f), fade_out_current_(0.0f), left_pan_(1.0f), right_pan_(1.0f)
		{
			variant panning = options["pan"];
			if(panning.is_list()) {
				left_pan_ = panning[0].as_float();
				right_pan_ = panning[1].as_float();
			}

			init();
		}

		virtual ~RawPlayingSound() {}

		void setLooped(bool value) { looped_ = value; }
		bool looped() const { return looped_; }

		int loopPoint() const { return loop_point_; }
		void setLoopPoint(int value) { loop_point_ = value; }

		int loopFrom() const { return loop_from_; }
		void setLoopFrom(int value) { loop_from_ = value; }

		float leftPan() const { return left_pan_; }
		float rightPan() const { return right_pan_; }

		void setPanning(float left, float right)
		{
			left_pan_ = left;
			right_pan_ = right;
		}

		bool loaded() const
		{
			return data_.get() != nullptr;
		}

		void init()
		{
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

		const std::string& fname() const { return fname_; }
		void setFilename(const std::string& f) {
			if(fname_ == f) {
				return;
			}

			fname_ = f;
			data_.reset();
			init();
		}

		void stopPlaying(float fade_time) {
			fade_out_ = fade_time;
			fade_out_current_ = 0.0f;
		}

		//Once finished(), the game thread may remove this from the list of playing sounds.
		bool finished() const override 
		{
			return (data_.get() != nullptr && !looped_ && pos_ >= int(data_->nsamples())) || (fade_out_ >= 0.0f && fade_out_current_ >= fade_out_);
		}

		void setVolume(float volume, float nseconds=0.0)
		{
			if(nseconds <= 0.0) {
				volume_ = volume;
				volume_target_time_ = -1.0;
			} else {
				volume_target_ = volume;
				volume_target_time_ = nseconds;
			}
		}

		float getVolume() const
		{
			return volume_;
		}

		//Mix data into the output buffer. Can be safely called from the mixing thread.
		virtual void MixData(float* output, int nsamples) override
		{
			if(!data_ || nsamples <= 0 || (!looped_ && pos_ >= int(data_->nsamples())) || (fade_out_ >= 0.0f && fade_out_current_ >= fade_out_)) {
				return;
			}
			int pos = pos_;
			pos_ += nsamples;
			std::shared_ptr<WaveData> data = data_;

			float fade_out = fade_out_;
			float fade_out_current = fade_out_current_;

			int endpoint = !looped_ || loop_from_ <= 0 || loop_from_ > static_cast<int>(data_->nsamples()) ? data_->nsamples() : loop_from_;

			if(fade_out_ >= 0.0f) {
				fade_out_current_ += float(std::min<int>(nsamples, endpoint - pos))/float(SampleRate);
			}

			if(looped_ && pos_ >= endpoint) {
				pos_ = loop_point_;
				fade_in_ = 0.0f;
			}

			float looped = looped_;

			if(pos < 0) {
				nsamples += pos;
				pos = 0;
				if(nsamples <= 0) {
					return;
				}
			}

			int nmissed = 0;

			const int navail = endpoint - pos;
			if(nsamples > navail) {
				nmissed = nsamples - navail;
				nsamples = navail;
			}

			const short* p = &data->buffer[pos*data->nchannels];

			float volume = volume_ * g_sfx_volume;

			if(pos < fade_in_*SampleRate || fade_out >= 0.0f) {
				for(int n = 0; n != nsamples; ++n) {
					*output++ += (float(*p)/SHRT_MAX) * volume * std::min<float>(1.0f, ((pos+n*2) / (SampleRate*fade_in_))) * (1.0f - (fade_out_current + (n*0.5f)/SampleRate)/fade_out);
					if(data->nchannels > 1) {
						++p;
					}
					*output++ += (float(*p)/SHRT_MAX) * volume * std::min<float>(1.0f, ((pos+n*2) / (SampleRate*fade_in_))) * (1.0f - (fade_out_current + (n*0.5f)/SampleRate)/fade_out);
					++p;
				}
			} else if(volume_target_time_ > 0.0f) {
				const float begin_volume = volume;
				float ntime = nsamples/44100.0f;
				if(ntime > volume_target_time_) {
					ntime = volume_target_time_;
				}

				float ratio = ntime/volume_target_time_;

				float end_volume = (1.0-ratio)*begin_volume + volume_target_*ratio*g_sfx_volume;

				for(int n = 0; n != nsamples; ++n) {
					float r = float(n)/float(nsamples);
					float volume = (begin_volume*(1.0-r) + end_volume*r);
					*output++ += (float(*p)/SHRT_MAX) * volume * left_pan_;
					if(data->nchannels > 1) {
						++p;
					}
					*output++ += (float(*p++)/SHRT_MAX) * volume * right_pan_;
				}

				volume_target_time_ -= ntime;
				volume_ = (1.0-ratio)*volume_ + volume_target_*ratio;
				if(volume_target_time_ <= 0.001) {
					volume_target_time_ = 0.0f;
					volume_ = volume_target_;
				}
			} else if(left_pan_ != 1.0f || right_pan_ != 1.0f) {
				for(int n = 0; n != nsamples; ++n) {
					*output++ += (float(*p)/SHRT_MAX) * volume * left_pan_;
					if(data->nchannels > 1) {
						++p;
					}
					*output++ += (float(*p++)/SHRT_MAX) * volume * right_pan_;
				}
			} else if(data->nchannels == 1) {
				for(int n = 0; n != nsamples; ++n) {
					*output++ += (float(*p)/SHRT_MAX) * volume;
					*output++ += (float(*p)/SHRT_MAX) * volume;
					++p;
				}
			} else {
				for(int n = 0; n != nsamples*2; ++n) {
					*output++ += (float(*p++)/SHRT_MAX) * volume;
				}
			}

			if(looped && nmissed > 0 && endpoint > 0) {
				MixData(output, nmissed);
			}
		}

		int pos() const { return pos_; }

		std::shared_ptr<WaveData> data() const { return data_; }

	private:

		std::string fname_;

		std::shared_ptr<WaveData> data_;

		int pos_;

		float volume_, volume_target_, volume_target_time_, fade_in_;

		float fade_out_, fade_out_current_;

		bool looped_;
		int loop_point_, loop_from_;

		float left_pan_, right_pan_;
	};

	//Representation of a sound currently playing. A new instance will be created every time
	//a sound effect starts playing, so is reasonably lightweight.
	//Instances are created by the game thread but accessed from the mixing thread.
	//
	//If the underlying data isn't available when this object is created it will wait, polling
	//every frame to see if the cache has been populated every frame, and then play as soon
	//as the data is loaded.
	class PlayingSound : public SoundSource
	{
	public:
		PlayingSound(const std::string& fname, const void* obj, float volume, float fade_in) : obj_(obj), source_(new RawPlayingSound(fname, volume, fade_in)), actual_volume_(-1.0f)
		{
			first_filter_ = source_;
		}

		PlayingSound(const std::string& fname, const void* obj, variant options) : obj_(obj), source_(new RawPlayingSound(fname, options)), userdata_(options["userdata"]), actual_volume_(-1.0f)
		{
			first_filter_ = source_;

			variant f = options["filters"];
			if(f.is_list()) {
				std::vector<ffl::IntrusivePtr<SoundEffectFilter>> filters;
				for(auto v : f.as_list()) {
					auto p = v.try_convert<SoundEffectFilter>();
					ASSERT_LOG(p, "Failed to convert to sound effect filter");
					filters.push_back(ffl::IntrusivePtr<SoundEffectFilter>(p));
				}

				setFilters(filters);
			}
		}

		virtual ~PlayingSound() {}

		void setFilename(const std::string& f) {
			threading::lock lck(mutex_);
			source_->setFilename(f);
		}

		void setObj(const void* obj) { obj_ = obj; }

		void setLooped(bool value) { source_->setLooped(value); }
		bool looped() const { return source_->looped(); }

		int loopPoint() const { return source_->loopPoint(); }
		void setLoopPoint(int value) {
			threading::lock lck(mutex_);
			source_->setLoopPoint(value);
		}

		int loopFrom() const { return source_->loopFrom(); }
		void setLoopFrom(int value) {
			threading::lock lck(mutex_);
			source_->setLoopFrom(value);
		}

		float leftPan() const { return source_->leftPan(); }
		float rightPan() const { return source_->rightPan(); }
		void setPanning(float left, float right)
		{
			threading::lock lck(mutex_);
			source_->setPanning(left, right);
		}

		void init()
		{
			threading::lock lck(mutex_);
			source_->init();
		}

		void stopPlaying(float fade_time) {
			threading::lock lck(mutex_);
			source_->stopPlaying(fade_time);
		}

		//Once finished(), the game thread may remove this from the list of playing sounds.
		bool finished() const override
		{
			threading::lock lck(mutex_);
			return first_filter_->finished();
		}

		void setVolume(float volume, float nseconds=0.0)
		{
			threading::lock lck(mutex_);
			source_->setVolume(volume, nseconds);
		}

		float getVolume()
		{
			if(actual_volume_ >= 0.0f) {
				return actual_volume_;
			}

			threading::lock lck(mutex_);
			return source_->getVolume();
		}

		//Mix data into the output buffer. Can be safely called from the mixing thread.
		virtual void MixData(float* output, int nsamples) override
		{
			threading::lock lck(mutex_);
			if(source_->loaded()) {
				actual_volume_ = source_->getVolume();
				first_filter_->MixData(output, nsamples);
			}
		}

		const void* obj() const { return obj_; }
		const std::string& fname() const { return source_->fname(); }

		const std::vector<ffl::IntrusivePtr<SoundEffectFilter> >& getFilters() const {
			return filters_;
		}

		void setFilters(std::vector<ffl::IntrusivePtr<SoundEffectFilter> > filters) {
			threading::lock lck(mutex_);
			filters_.clear();
			for(auto f : filters) {
				filters_.push_back(ffl::IntrusivePtr<SoundEffectFilter>(f->clone()));
			}
			for(size_t n = 0; n != filters_.size(); ++n) {
				if(n == 0) {
					filters_[n]->setSource(source_.get());
				} else {
					filters_[n]->setSource(filters_[n-1].get());
				}
			}

			if(filters_.empty()) {
				first_filter_ = source_;
			} else {
				first_filter_ = filters_.back();
			}
		}

		ffl::IntrusivePtr<RawPlayingSound> src() const { return source_; }

	private:
		DECLARE_CALLABLE(PlayingSound);

		threading::mutex mutex_;

		const void* obj_;

		ffl::IntrusivePtr<RawPlayingSound> source_;

		ffl::IntrusivePtr<SoundSource> first_filter_;

		std::vector<ffl::IntrusivePtr<SoundEffectFilter> > filters_;

		variant userdata_;

		float actual_volume_;
	};

	//List of currently playing sounds. Only the game thread can modify this,
	//the mixing thread will read it.
	std::vector<ffl::IntrusivePtr<PlayingSound> > g_playing_sounds;
	threading::mutex g_playing_sounds_mutex;

	BEGIN_DEFINE_CALLABLE(PlayingSound, SoundSource)
	DEFINE_FIELD(filename, "string")
		return variant(obj.fname());
	DEFINE_SET_FIELD
		obj.setFilename(value.as_string());
	DEFINE_FIELD(userdata, "any")
		return obj.userdata_;
	DEFINE_FIELD(pos, "decimal")
		return variant(obj.src()->pos()/SampleRateDouble);
	DEFINE_FIELD(duration, "decimal|null")
		if(obj.src()->data()) {
			return variant(obj.src()->data()->nsamples()/SampleRateDouble);
		}

		return variant();
	DEFINE_FIELD(loop, "bool")
		return variant::from_bool(obj.looped());
	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		obj.setLooped(value.as_bool());

	DEFINE_FIELD(loop_point, "decimal|null")
		if(obj.loopPoint() <= 0) {
			return variant();
		} else {
			return variant(float(obj.loopPoint()) / float(SampleRate));
		}
	DEFINE_SET_FIELD
		if(value.is_null()) {
			obj.setLoopPoint(0);
		} else {
			obj.setLoopPoint(int(value.as_float()*SampleRate));
		}

	DEFINE_FIELD(loop_from, "decimal|null")
		if(obj.loopFrom() <= 0) {
			return variant();
		} else {
			return variant(float(obj.loopFrom()) / float(SampleRate));
		}
	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		if(value.is_null()) {
			obj.setLoopFrom(0);
		} else {
			obj.setLoopFrom(int(value.as_float()*SampleRate));
		}
	
	DEFINE_FIELD(volume, "decimal")
		return variant(obj.source_->getVolume());
	
	BEGIN_DEFINE_FN(set_volume, "(decimal,decimal)->commands")
		float vol = FN_ARG(0).as_float();
		float t = FN_ARG(1).as_float();
		ffl::IntrusivePtr<PlayingSound> ptr(const_cast<PlayingSound*>(&obj));
		return variant(new game_logic::FnCommandCallable("sound::set_volume", [=]() {
			ptr->setVolume(vol, t);
		}));
	END_DEFINE_FN

	DEFINE_FIELD(pan, "[decimal,decimal]")
		std::vector<variant> v;
		v.push_back(variant(obj.leftPan()));
		v.push_back(variant(obj.rightPan()));
		return variant(&v);

	DEFINE_SET_FIELD
		threading::lock lck(obj.mutex_);
		std::vector<decimal> d = value.as_list_decimal();
		ASSERT_LOG(d.size() == 2, "Incorrect pan arg");
		obj.setPanning(d[0].as_float32(), d[1].as_float32());

	DEFINE_FIELD(filters, "[builtin sound_effect_filter]")
		auto v = obj.getFilters();
		std::vector<variant> result;
		for(auto p : v) {
			result.push_back(variant(p.get()));
		}

		return variant(&result);

	DEFINE_SET_FIELD
		std::vector<ffl::IntrusivePtr<SoundEffectFilter>> filters;
		for(auto v : value.as_list()) {
			auto p = v.try_convert<SoundEffectFilter>();
			ASSERT_LOG(p, "Failed to convert to sound effect filter");
			filters.push_back(ffl::IntrusivePtr<SoundEffectFilter>(p));
		}

		obj.setFilters(filters);

	BEGIN_DEFINE_FN(play, "()->commands")
		ffl::IntrusivePtr<PlayingSound> ptr(const_cast<PlayingSound*>(&obj));
		return variant(new game_logic::FnCommandCallable("sound::play", [=]() {
			threading::lock lck(g_playing_sounds_mutex);
			if(std::find(g_playing_sounds.begin(), g_playing_sounds.end(), ptr) != g_playing_sounds.end()) {
				return;
			} else {
				g_playing_sounds.push_back(ptr);
			}
		}));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(stop, "(decimal=0.1)->commands")
		float fade_time = 0.1f;
		if(NUM_FN_ARGS > 0) {
			fade_time = FN_ARG(0).as_float();
		}
		ffl::IntrusivePtr<PlayingSound> ptr(const_cast<PlayingSound*>(&obj));
		return variant(new game_logic::FnCommandCallable("sound::stop", [=]() {
			ptr->stopPlaying(fade_time);
		}));
	END_DEFINE_FN

	END_DEFINE_CALLABLE(PlayingSound)

	std::vector<Uint8> g_debug_audio_stream;
	threading::mutex g_debug_audio_stream_mutex;

	int g_audio_callback_done_fade_out = 0;
	bool g_audio_callback_fade_out = false;

	//Audio callback called periodically by SDL to populate audio data in
	//the mixing thread.
	void AudioCallback(void* userdata, Uint8* stream, int len)
	{
		if(g_audio_callback_fade_out) {
			++g_audio_callback_done_fade_out;
		}

		float sfx_volume = g_sfx_volume;

		float* buf = reinterpret_cast<float*>(stream);
		const int nsamples = len / sizeof(float);

		//Set the buffer to zeroes so we can start mixing.
		for(int n = 0; n < nsamples; ++n) {
			buf[n] = 0.0f;
		}

		if(g_muted || g_audio_callback_done_fade_out > 1) {
			return;
		}

		//Mix all the sound effects.
		{
			threading::lock lck(g_playing_sounds_mutex);
			for(const ffl::IntrusivePtr<PlayingSound>& s : g_playing_sounds) {
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

		if(g_audio_callback_fade_out) {
			float* buf = reinterpret_cast<float*>(stream);
			const int nsamples = len / sizeof(float);
			for(int n = 0; n != nsamples; ++n) {
				float ratio = 1.0 - float(n)/float(nsamples);
				buf[n] *= ratio;
			}
		}

		if(g_debug_visualize_audio) {
			threading::lock lck(g_debug_audio_stream_mutex);
			const int MaxData = 44100*32;
			const int new_data_size = g_debug_audio_stream.size() + len;
			if(new_data_size > MaxData) {
				g_debug_audio_stream.erase(g_debug_audio_stream.begin(), g_debug_audio_stream.begin() + (new_data_size - MaxData));
			}

			g_debug_audio_stream.resize(g_debug_audio_stream.size() + len);
			memcpy(&g_debug_audio_stream[0] + g_debug_audio_stream.size() - len, stream, len);
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

		SDL_LockAudio();
		g_audio_callback_fade_out = true;
		while(g_audio_callback_done_fade_out < 2) {
			SDL_UnlockAudio();
			SDL_Delay(1);
			SDL_LockAudio();
		}
		SDL_UnlockAudio();

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

		g_music_players.clear();
		g_current_player.reset();

		SDL_CloseAudioDevice(g_audio_device);
		g_audio_device = 0;
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
		for(ffl::IntrusivePtr<PlayingSound>& s : g_playing_sounds) {
			s->init();

			if(s->finished()) {
				s.reset();
			}
		}

		g_playing_sounds.erase(std::remove(g_playing_sounds.begin(), g_playing_sounds.end(), ffl::IntrusivePtr<PlayingSound>()), g_playing_sounds.end());
	}

	//Go through the music players and removed any that are finished.
	{
		threading::lock lck(g_music_thread_mutex);
		g_current_player.reset();
		std::vector<ffl::IntrusivePtr<MusicPlayer> > players = g_music_players;
		for(ffl::IntrusivePtr<MusicPlayer>& p : players) {
			if(p->finished()) {
				p->onComplete();
			}
		}

		for(ffl::IntrusivePtr<MusicPlayer>& p : g_music_players) {
			if(p->finished()) {

				if(p == g_music_players.back()) {
					p->restart();
				} else {
					p.reset();
				}
			} else if(!p->stopping()) {
				g_current_player = p;
			}
		}

		g_music_players.erase(std::remove(g_music_players.begin(), g_music_players.end(), ffl::IntrusivePtr<MusicPlayer>()), g_music_players.end());

	}
}

//Game-engine facing audio API implementation below here.

//preload a sound effect in the cache.
void preload(const std::string& fname)
{
	std::string file = map_filename(fname);

	{
		threading::lock lck(g_files_loading_mutex);
		if(g_files_loading.count(file)) {
			return;
		}

		g_files_loading.insert(file);
	}

	threading::lock lck(g_loader_thread_mutex);
	g_loader_thread_queue.push_back(file);
	g_loader_thread_cond.notify_one();
}

void change_volume(const void* object, float volume, float nseconds)
{
	for(const ffl::IntrusivePtr<PlayingSound>& s : g_playing_sounds) {
		if(s->obj() == object) {
			s->setVolume(volume, nseconds);
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
	for(const ffl::IntrusivePtr<PlayingSound>& s : g_playing_sounds) {
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

	ffl::IntrusivePtr<PlayingSound> s(new PlayingSound(file, object, volume, fade_in_time));
	s->setPanning(g_pan_left, g_pan_right);

	threading::lock lck(g_playing_sounds_mutex);
	g_playing_sounds.push_back(s);
}


//stop a sound. object refers to the object that started the sound, and is
//the same as the object in play().
void stop_sound(const std::string& file, const void* object, float fade_out_time)
{
	for(const ffl::IntrusivePtr<PlayingSound>& s : g_playing_sounds) {
		if(s->obj() == object && s->fname() == file) {
			s->stopPlaying(fade_out_time);
		}
	}
}


//stop all looped sounds associated with an object; same object as in play()
//intended to be called in all object's destructors
void stop_looped_sounds(const void* object)
{
	for(const ffl::IntrusivePtr<PlayingSound>& s : g_playing_sounds) {
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

	ffl::IntrusivePtr<PlayingSound> s(new PlayingSound(file, object, volume, fade_in_time));
	s->setLooped(true);
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

	std::map<variant,variant> options;
	ffl::IntrusivePtr<MusicPlayer> player(new MusicPlayer(file.c_str(), variant(&options)));

	if(queue && g_music_players.empty() == false) {
		g_music_queue = player;
	} else {
		g_current_music = file;
		threading::lock lck(g_music_thread_mutex);
		for(auto& p : g_music_players) {
			p->stopPlaying(fade_time/60.f);
		}
		g_music_players.push_back(player);
		g_current_player = player;
	}
}

void play_music_interrupt(const std::string& file)
{
	if(file == g_current_music) {
		return;
	}

	std::map<variant,variant> options;
	ffl::IntrusivePtr<MusicPlayer> player(new MusicPlayer(file.c_str(), variant(&options)));

	g_current_music = file;

	threading::lock lck(g_music_thread_mutex);
	for(auto& p : g_music_players) {
		p->stopPlaying(0.1f);
	}
	g_music_players.push_back(player);
	g_current_player = player;
}


const std::string& current_music()
{
	return g_current_music;
}

AudioEngine::AudioEngine(ffl::IntrusivePtr<const CustomObject> obj) : obj_(obj)
{
}

BEGIN_DEFINE_CALLABLE_NOBASE(AudioEngine)
	DEFINE_FIELD(status, "string")
		
		std::ostringstream s;
		{
			threading::lock lck(g_music_thread_mutex);
			for(auto m : g_music_players) {
				s << "  MUSIC: " << m->fname() << ": " << m->timeCurrent() << "/" << m->timeLength() << "\n";
			}
		}

		{
			threading::lock lck(g_wave_cache_mutex);
			threading::lock lck2(g_files_loading_mutex);
			s << "Cached sounds: " << g_wave_cache_lru.size() << "/" << g_wave_cache.size() << " entries, " << (g_wave_cache_size/(1024*1024)) << "/" << g_audio_cache_size_mb << "MB; files loaded: " << g_files_loading.size() << "\n";
		}

		{
			threading::lock lck(g_playing_sounds_mutex);
			s << g_playing_sounds.size() << " sounds playing\n";

			for(auto p : g_playing_sounds) {
				s << "  " << p->fname() << ": " << (p->src()->loaded() == false ? "loading" : (p->finished() ? "finished" : "")) << " vol: " << p->getVolume() << " (stereo pan: " << p->src()->leftPan() << "/" << p->src()->rightPan() << ")";
				if(p->src()->looped()) {
					s << " (looped)";
				}
				if(p->src()->data()) {
					s << " " << p->src()->pos()/44100.0f << "/" << p->src()->data()->nsamples()/44100.0f;
				}

				if(p->getFilters().empty() == false) {
					s << " Filters: ";
					for(auto f : p->getFilters()) {
						s << f->debug_description() << " ";
					}
				}

				s << "\n";
			}
		}

		return variant(s.str());

	BEGIN_DEFINE_FN(sound, "(string, {userdata: null|any, volume: decimal|null, pan: [decimal,decimal]|null, loop: bool|null, loop_point: decimal|null, loop_from: decimal|null, fade_in: decimal|null, pos: decimal|null, filters: null|[builtin sound_effect_filter]}|null=null) ->builtin playing_sound")
		const std::string& name = FN_ARG(0).as_string();
		variant options;
		std::map<variant,variant> options_buf;
		if(NUM_FN_ARGS > 1) {
			options = FN_ARG(1);
			if(options.is_null()) {
				options = variant(&options_buf);
			}
		} else {
			options = variant(&options_buf);
		}

		return variant(new PlayingSound(name, obj.obj_.get(), options));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(preload, "(string)->commands")
		const std::string name = FN_ARG(0).as_string();
		return variant(new game_logic::FnCommandCallable("sound::preload", [=]() {
			preload(name);
		}));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(music, "(string, {volume: decimal|null, loop: bool|null, loop_point: decimal|null, pos: decimal|null, loop_from: decimal|null, on_complete: null|commands|function()->commands}|null=null) ->builtin music_player")
		const std::string& name = FN_ARG(0).as_string();
		variant options;
		std::map<variant,variant> options_buf;
		if(NUM_FN_ARGS > 1) {
			options = FN_ARG(1);
		} else {
			options = variant(&options_buf);
		}

		return variant(new MusicPlayer(name, options));

	END_DEFINE_FN

	
	DEFINE_FIELD(current_music, "null|builtin music_player")
		return variant(g_current_player.get());
	
	DEFINE_FIELD(current_sounds, "[builtin playing_sound]")
		std::vector<variant> res;

		threading::lock lck(g_playing_sounds_mutex);

		for(auto p : g_playing_sounds) {
			res.push_back(variant(p.get()));
		}

		return variant(&res);
	
	BEGIN_DEFINE_FN(low_pass_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_lowpass, FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(high_pass_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_highpass, FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(band_pass_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_bandpass, FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(notch_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_notch, FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(peak_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_peak, FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(low_shelf_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_lowshelf, FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(high_shelf_filter, "(map) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BiQuadSoundEffectFilter(bq_type_highshelf, FN_ARG(0)));
	END_DEFINE_FN
	
	BEGIN_DEFINE_FN(speed_filter, "({ speed: decimal }) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new SpeedSoundEffectFilter(FN_ARG(0)));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(binaural_delay_filter, "({ delay: decimal }) ->builtin sound_effect_filter")
		game_logic::Formula::failIfStaticContext();
		return variant(new BinauralDelaySoundEffectFilter(FN_ARG(0)));
	END_DEFINE_FN
END_DEFINE_CALLABLE(AudioEngine)

MemoryUsageInfo get_memory_usage_info()
{
	threading::lock lck(g_wave_cache_mutex);
	MemoryUsageInfo info;
	info.cache_usage = g_wave_cache_size;
	info.max_cache_usage = g_audio_cache_size_mb*1024*1024;
	info.nsounds_cached = static_cast<int>(g_wave_cache_lru.size());
	return info;
}

void get_debug_audio_stream(std::vector<float>& res)
{
	threading::lock lck(g_debug_audio_stream_mutex);
	res.resize(g_debug_audio_stream.size()/sizeof(float));
	if(res.empty() == false) {
		memcpy(&res[0], &g_debug_audio_stream[0], g_debug_audio_stream.size());
	}
}

}

//Outputs names of any wave files it fails to load.
COMMAND_LINE_UTILITY(validate_waves)
{
	std::multimap<std::string,std::string> paths;
	module::get_all_filenames_under_dir("sounds/", &paths, module::MODULE_NO_PREFIX);
	for(auto p : paths) {
		std::string fname = module::map_file(p.second);

		if(p.first.size() <= 4 || std::equal(p.first.end()-4, p.first.end(), ".wav") == false) {
			continue;
		}

		SDL_AudioSpec spec;
		spec.freq = 44100;
		spec.format = AUDIO_S16;
		spec.channels = 2;
		spec.silence = 0;
		spec.size = 512;

		Uint8* buf = nullptr;
		Uint32 len = 0;
		auto res = SDL_LoadWAV(fname.c_str(), &spec, &buf, &len);
		if(res == nullptr) {
			printf("%s\n", fname.c_str());
		}
	}
}
