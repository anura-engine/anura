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

#pragma once

#if defined(USE_LIBVPX)

#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include "SDL.h"

#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include "Texture.hpp"

#include "widget.hpp"

namespace movie
{
	class vpx : public gui::Widget
	{
	public:
		vpx(const std::string& file, int x, int y, int width, int height, bool loop, bool cancel_on_keypress);
		vpx(const variant& v, game_logic::FormulaCallable* e);
		vpx(const vpx& v);
		gui::WidgetPtr clone() const override;
	protected:
		void init();
		void stop();
		void genTextures();
		void decodeFrame();
	private:
		virtual void handleProcess() override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual void handleDraw() const override;

		std::ifstream file_;
		std::string file_name_;
		bool loop_;
		bool cancel_on_keypress_;
		size_t frame_cnt_;
		vpx_codec_flags_t flags_;
		std::vector<char> file_hdr_;
		std::vector<uint8_t> frame_hdr_;
		int frame_size_;
		std::vector<uint8_t> frame_;
		vpx_codec_err_t res_;

		vpx_codec_ctx_t  codec_;
		vpx_codec_iter_t iter_;
		vpx_image_t* img_;

		bool playing_;

		KRE::TexturePtr texture_;

		unsigned u_tex_[3];
	};
	typedef ffl::IntrusivePtr<vpx> vpx_ptr;
}

#endif
