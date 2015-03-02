/*
   Copyright 2014 Kristina Simpson <sweet.kristas@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include "Texture.hpp"

namespace KRE
{
	struct SurfaceAreas
	{
		explicit SurfaceAreas(SurfacePtr s, const std::vector<rect>& rs) : surface(s), rects(rs) {}
		explicit SurfaceAreas(SurfacePtr s) : surface(s), rects() {}
		void addRect(const rect& r) { rects.emplace_back(r); }
		void addRect(int x, int y, int w, int h) { rects.emplace_back(x, y, w, h); }
		SurfacePtr surface;
		std::vector<rect> rects;
	};

	class Packer
	{
	public:
		typedef std::vector<rect>::const_iterator const_iterator;

		Packer(const std::vector<SurfaceAreas>& inp, int max_width, int max_height);
		
		SurfacePtr getOutputSurface() { return outp_; }

		const_iterator begin() const { return out_rects_.begin(); }
		const_iterator end() const { return out_rects_.end(); }
	private:
		std::vector<rect> out_rects_;
		SurfacePtr outp_;
	};
}
