/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#if defined(USE_LIBVPX)

#include "Canvas.hpp"

#include "module.hpp"
#include "play_vpx.hpp"
#include "preferences.hpp"

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

namespace movie
{
	namespace 
	{
		size_t mem_get_le32(const std::vector<uint8_t>& mem) {
			return (size_t(mem[3]) << 24)|(size_t(mem[2]) << 16)|(size_t(mem[1]) << 8)|size_t(mem[0]);
		}
	}

	vpx::vpx(const std::string& file, int x, int y, int width, int height, bool loop, bool cancel_on_keypress)
		: loop_(loop), 
		  cancel_on_keypress_(cancel_on_keypress), 
		  playing_(false), 
		  flags_(0), 
		  img_(NULL)
	{
		file_name_ = module::map_file(file);
		setLoc(x, y);
		setDim(width, height);
		init();
	}

	vpx::vpx(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v, e), 
		  loop_(false), 
		  cancel_on_keypress_(false), 
		  playing_(false), 
		  flags_(0), 
		  img_(NULL)
	{
		ASSERT_LOG(v.has_key("filename") && v["filename"].is_string(), "Must have at least a 'filename' key or type string");
		file_name_ = module::map_file(v["filename"].as_string());
		if(v.has_key("loop")) {
			loop_ = v["loop"].as_bool();
		}
		if(v.has_key("cancel_on_keypress")) {
			cancel_on_keypress_ = v["cancel_on_keypress"].as_bool();
		}
		init();
	}

	void vpx::init()
	{
		file_.open(file_name_, std::ios::in | std::ios::binary);
		ASSERT_LOG(file_.is_open(), "Unable to open file: " << file_name_);
		file_hdr_.resize(IVF_FILE_HDR_SZ);
		file_.read(&file_hdr_[0], IVF_FILE_HDR_SZ);
		ASSERT_LOG(file_hdr_[0] == 'D' && file_hdr_[1] == 'K' && file_hdr_[2] == 'I' && file_hdr_[3] == 'F', 
			"Unknown file header found: " << std::string(&file_hdr_[0], &file_hdr_[4]));
		frame_hdr_.resize(IVF_FRAME_HDR_SZ);

		auto res = vpx_codec_dec_init(&codec_, vpx_codec_vp8_dx(), NULL, flags_);
		ASSERT_LOG(res == 0, "Codec error: " << vpx_codec_error(&codec_));

		frame_.resize(256 * 1024);
		playing_ = true;
		iter_ = NULL;
	}

	vpx::~vpx() 
	{
	}

	void vpx::genTextures()
	{
		ASSERT_LOG(img_ != NULL, "img_ is null");
		texture_ = KRE::Texture::createTexture(img_->d_w, img_->d_h, KRE::PixelFormat::PF::PIXELFORMAT_YV12);
	}

	void vpx::stop()
	{
		playing_ = false;
	}

	void vpx::decodeFrame()
	{
		if(file_.eof()) {
			// when file_ has been read call vpx_codec_decode with data as NULL and sz as 0
			vpx_codec_decode(&codec_, NULL, 0, NULL, 0);
			if(loop_) {
				file_.clear();
				file_.seekg(0, std::ios::beg);
				file_.read(&file_hdr_[0], IVF_FILE_HDR_SZ);
				ASSERT_LOG(file_hdr_[0] == 'D' && file_hdr_[1] == 'K' && file_hdr_[2] == 'I' && file_hdr_[3] == 'F', 
					"Unknown file header found: " << std::string(&file_hdr_[0], &file_hdr_[4]));
				frame_hdr_.resize(IVF_FRAME_HDR_SZ);

				iter_ = NULL;
				img_ = NULL;
			
				file_.read(reinterpret_cast<char*>(&frame_hdr_[0]), IVF_FRAME_HDR_SZ);
				frame_size_ = mem_get_le32(frame_hdr_);
				++frame_cnt_;

				frame_.resize(frame_size_);
				file_.read(reinterpret_cast<char*>(&frame_[0]), frame_size_);

				auto res = vpx_codec_decode(&codec_, &frame_[0], frame_size_, NULL, 0);
				ASSERT_LOG(res == 0, "Codec error: " << vpx_codec_error(&codec_) << " : " << vpx_codec_error_detail(&codec_));

			} else {
				playing_ = false;
			}
			/// test loop_ here.
		} else {
			file_.read(reinterpret_cast<char*>(&frame_hdr_[0]), IVF_FRAME_HDR_SZ);
			frame_size_ = mem_get_le32(frame_hdr_);
			++frame_cnt_;

			frame_.resize(frame_size_);
			file_.read(reinterpret_cast<char*>(&frame_[0]), frame_size_);

			auto res = vpx_codec_decode(&codec_, &frame_[0], frame_size_, NULL, 0);
			ASSERT_LOG(res == 0, "Codec error: " << vpx_codec_error(&codec_) << " : " << vpx_codec_error_detail(&codec_));
		}
	}

	void vpx::handleProcess()
	{
		if(!playing_) {
			return;
		}

		bool done = false;
		while(playing_ && !done) {
			if(img_ == NULL) {
				decodeFrame();
				iter_ = NULL;
			}
			img_ = vpx_codec_get_frame(&codec_, &iter_);
			if(img_ != NULL) {
				done = true;
			}
		}

		if(texture_ == nullptr) {
			genTextures();
		}
	}

	bool vpx::handleEvent(const SDL_Event& evt, bool claimed)
	{
		if(claimed) {
			return true;
		}
		if(!cancel_on_keypress_) {
			return claimed;
		}

		switch(evt.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				stop();
				claimed = true;
				break;
			
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if(inWidget(evt.button.x, evt.button.y)) {
					stop();
					claimed = true;
				}
				break;
		}
		return claimed;
	}

	void vpx::handleDraw() const
	{
		if(img_ == NULL) {
			return;
		}

		//KRE::Canvas::ShaderManger sm("");
		texture_->update(0, 0, img_->d_w, img_->d_h, img_->stride, static_cast<const void*>(img_->planes));

		KRE::Canvas::getInstance()->blitTexture(texture_, 0, rect(0, 0, width(), height()));
	}
}

#endif
