/*
	Copyright (C) 2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "SurfaceScale.hpp"

namespace KRE
{
	namespace scale
	{
		const int scale_hard_minimum = 1;
		const int scale_hard_maximum = 10000;

		namespace
		{
			SurfacePtr check_input(const SurfacePtr& input_surf, const int scale)
			{
				ASSERT_LOG(scale >= scale_hard_minimum, "A scale value can not be less than " << scale_hard_minimum << ". " << scale << " was specified.");
				ASSERT_LOG(scale <= scale_hard_maximum, "A scale value can not be greater than " << scale_hard_maximum << ". " << scale << " was specified.");

				SurfacePtr inp = input_surf;
				// Convert surface to ARGB format if needed
				if(input_surf->getPixelFormat()->getFormat() != PixelFormat::PF::PIXELFORMAT_ARGB8888) {
					inp = input_surf->convert(PixelFormat::PF::PIXELFORMAT_ARGB8888); 
				}
				return inp;
			}
		}

		// scale is a value from 1 to 10000, such that a value of 100 is a scale factor of 1 (i.e. not scaled).
		// A value less than 100 makes the image smaller, a value larger than 100 makes the image bigger.
		SurfacePtr nearest_neighbour(const SurfacePtr& input_surf, const int scale)
		{
			if(scale == 100) {
				return input_surf;
			}

			SurfacePtr inp = check_input(input_surf, scale);

			const int old_image_width = inp->width();
			const double ratio_x = 100.0 / static_cast<double>(scale);
			const double ratio_y = ratio_x;
			const int new_image_width = static_cast<int>(inp->width() / ratio_x);
			const int new_image_height = static_cast<int>(inp->height() / ratio_y);

			ASSERT_LOG(new_image_width > 0 && new_image_height > 0, "New image size would be less than 0 pixels: " << new_image_width << "x" << new_image_height);

			std::unique_ptr<uint32_t[]> new_pixels(new uint32_t[new_image_width * new_image_height]);
			const uint32_t* old_pixels = static_cast<const uint32_t*>(inp->pixels());

			for(int y = 0; y != new_image_height; ++y) {
				for(int x = 0; x != new_image_width; ++x) {
					const int px = static_cast<int>(ratio_x * x);
					const int py = static_cast<int>(ratio_y * y);
					new_pixels[y * new_image_width + x] = old_pixels[py * old_image_width + px];
				}
			}

			return Surface::create(new_image_width, new_image_height, 32, 4*new_image_width, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, new_pixels.get());
		}

#define BILINEAR_ALPHA(a,b,c,d,xd,yd)	\
	(static_cast<int>(((a)>>24)*(1.0-(xd))*(1.0-(yd))) + \
	static_cast<int>(((b)>>24)*(xd)*(1.0-(yd))) + \
	static_cast<int>(((c)>>24)*(1.0-(xd))*(yd)) + \
	static_cast<int>(((d)>>24)*(xd)*(yd)))
#define BILINEAR_RED(a,b,c,d,xd,yd)	\
	(static_cast<int>(((((a)>>16)&0xff))*(1.0-(xd))*(1.0-(yd))) + \
	static_cast<int>(((((b)>>16)&0xff))*(xd)*(1.0-(yd))) + \
	static_cast<int>(((((c)>>16)&0xff))*(1.0-(xd))*(yd)) + \
	static_cast<int>(((((d)>>16)&0xff))*(xd)*(yd)))
#define BILINEAR_GREEN(a,b,c,d,xd,yd)	\
	(static_cast<int>(((((a)>>8)&0xff))*(1.0-(xd))*(1.0-(yd))) + \
	static_cast<int>(((((b)>>8)&0xff))*(xd)*(1.0-(yd))) + \
	static_cast<int>(((((c)>>8)&0xff))*(1.0-(xd))*(yd)) + \
	static_cast<int>(((((d)>>8)&0xff))*(xd)*(yd)))
#define BILINEAR_BLUE(a,b,c,d,xd,yd)	\
	(static_cast<int>((((a)&0xff))*(1.0-(xd))*(1.0-(yd))) + \
	static_cast<int>((((b)&0xff))*(xd)*(1.0-(yd))) + \
	static_cast<int>((((c)&0xff))*(1.0-(xd))*(yd)) + \
	static_cast<int>((((d)&0xff))*(xd)*(yd)))


		SurfacePtr bilinear(const SurfacePtr& input_surf, const int scale)
		{
			if(scale == 100) {
				return input_surf;
			}

			SurfacePtr inp = check_input(input_surf, scale);

			const int old_image_width = inp->width();
			double ratio_x = 100.0 / static_cast<double>(scale);
			double ratio_y = ratio_x;
			const int new_image_width = static_cast<int>(inp->width() / ratio_x);
			const int new_image_height = static_cast<int>(inp->height() / ratio_y);

			ratio_x = (inp->width()-1.0) / new_image_width;
			ratio_y = (inp->height()-1.0) / new_image_height;

			ASSERT_LOG(new_image_width > 0 && new_image_height > 0, "New image size would be less than 0 pixels: " << new_image_width << "x" << new_image_height);

			std::unique_ptr<uint32_t[]> new_pixels(new uint32_t[new_image_width * new_image_height]);
			const uint32_t* old_pixels = static_cast<const uint32_t*>(inp->pixels());

			for(int y = 0; y != new_image_height; ++y) {
				for(int x = 0; x != new_image_width; ++x) {
					const int px = static_cast<int>(ratio_x * x);
					const int py = static_cast<int>(ratio_y * y);
					const double xd = ratio_x * x - px;
					const double yd = ratio_y * y - py;
					const int pix_index = py * old_image_width + px;
					const uint32_t a = old_pixels[pix_index];
					const uint32_t b = old_pixels[pix_index+1];
					const uint32_t c = old_pixels[pix_index+old_image_width];
					const uint32_t d = old_pixels[pix_index+old_image_width+1];

					const uint8_t alpha = BILINEAR_ALPHA(a, b, c, d, xd, yd);
					const uint8_t red = BILINEAR_RED(a, b, c, d, xd, yd);
					const uint8_t green = BILINEAR_GREEN(a, b, c, d, xd, yd);
					const uint8_t blue = BILINEAR_BLUE(a, b, c, d, xd, yd);

					new_pixels[y * new_image_width + x] 
						= (static_cast<uint32_t>(alpha) << 24) 
						+ (static_cast<uint32_t>(red) << 16) 
						+ (static_cast<uint32_t>(green) << 8) 
						+ static_cast<uint32_t>(blue);
				}
			}

			return Surface::create(new_image_width, new_image_height, 32, 4*new_image_width, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, new_pixels.get());
		}

		namespace
		{
			double cubic_hermite(double a, double b, double c, double d, double t)
			{
				const double a0 = -a / 2.0 + (3.0 * b) / 2.0 - (3.0 * c) / 2.0 + d / 2.0;
				const double b0 = a - (5.0 * b) / 2.0 + 2.0 * c - d / 2.0;
				const double c0 = -a / 2.0 + c / 2.0;
				const double d0 = b; 
			
				return t * ((a0 * t + b0) * t + c0) + d0;
			}

			std::array<double, 4> cubic_hermite4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, double t)
			{
				std::array<double, 4> ret_val;
				double au[4];
				double bu[4];
				double cu[4];
				double du[4]; 

				for(int n = 0; n != 4; ++n) {
					const double A = a & 0xff;
					const double B = b & 0xff;
					const double C = c & 0xff;
					const double D = d & 0xff;
					a >>= 8; b >>= 8; c >>= 8; d >>= 8;
					au[n] = -A / 2.0 + (3.0 * B) / 2.0 - (3.0 * C) / 2.0 + D / 2.0;
					bu[n] =  A - (5.0 * B) / 2.0 + 2.0 * C - D / 2.0;
					cu[n] = -A / 2.0 + C / 2.0;
					du[n] = B;

					ret_val[n] = t * ((au[n] * t + bu[n]) * t + cu[n]) + du[n];
				}
				return ret_val;
			}
		}

#define CLAMP_XY(x, y, w, h)	(((x) < 0 ? 0 : (x) > (w)-1 ? (w)-1 : (x)) + ((y) < 0 ? 0 : (y) > (h)-1 ? (h)-1 : (y)) * (w))

		SurfacePtr bicubic(const SurfacePtr& input_surf, const int scale)
		{
			if(scale == 100) {
				return input_surf;
			}

			SurfacePtr inp = check_input(input_surf, scale);

			const int old_image_width = inp->width();
			const int old_image_height = inp->height();
			double ratio_x = 100.0 / static_cast<double>(scale);
			double ratio_y = ratio_x;
			const int new_image_width = static_cast<int>(inp->width() / ratio_x);
			const int new_image_height = static_cast<int>(inp->height() / ratio_y);

			ratio_x = (inp->width()-1.0) / new_image_width;
			ratio_y = (inp->height()-1.0) / new_image_height;

			ASSERT_LOG(new_image_width > 0 && new_image_height > 0, "New image size would be less than 0 pixels: " << new_image_width << "x" << new_image_height);

			std::unique_ptr<uint32_t[]> new_pixels(new uint32_t[new_image_width * new_image_height]);
			const uint32_t* old_pixels = static_cast<const uint32_t*>(inp->pixels());

			for(int y = 0; y != new_image_height; ++y) {
				for(int x = 0; x != new_image_width; ++x) {
					const int px = static_cast<int>(ratio_x * x);
					const int py = static_cast<int>(ratio_y * y);
					const double xd = ratio_x * x - px;
					const double yd = ratio_y * y - py;
					const int pix_index = py * old_image_width + px;

					uint32_t pix[4][4];

					for(int j = 0; j != 4; ++j) {
						for(int i = 0; i != 4; ++i) {
							pix[j][i] = old_pixels[CLAMP_XY(px+i-1, py+j-1, old_image_width, old_image_height)];
						}
					}

					const auto col0 = cubic_hermite4(pix[0][0], pix[1][0], pix[2][0], pix[3][0], xd);
					const auto col1 = cubic_hermite4(pix[0][1], pix[1][1], pix[2][1], pix[3][1], xd);
					const auto col2 = cubic_hermite4(pix[0][2], pix[1][2], pix[2][2], pix[3][2], xd);
					const auto col3 = cubic_hermite4(pix[0][3], pix[1][3], pix[2][3], pix[3][3], xd);
					uint32_t pix_value = 0;
					for(int n = 0; n != 4; n++) {
						const double value = cubic_hermite(col0[n], col1[n], col2[n], col3[n], yd);
						pix_value >>= 8;
						pix_value |= (value < 0 ? 0 : value > 255 ? 255 : static_cast<uint8_t>(value)) << 24;
					}

					new_pixels[y * new_image_width + x] = pix_value;
				}
			}

			return Surface::create(new_image_width, new_image_height, 32, 4*new_image_width, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, new_pixels.get());
		}

		SurfacePtr epx(const SurfacePtr& input_surf)
		{
			SurfacePtr inp = check_input(input_surf, 200);

			const int old_image_width = inp->width();
			const int old_image_height = inp->height();
			double ratio_x = 0.5;
			double ratio_y = ratio_x;
			const int new_image_width = static_cast<int>(inp->width() / ratio_x);
			const int new_image_height = static_cast<int>(inp->height() / ratio_y);

			ratio_x = (inp->width()-1.0) / new_image_width;
			ratio_y = (inp->height()-1.0) / new_image_height;

			ASSERT_LOG(new_image_width > 0 && new_image_height > 0, "New image size would be less than 0 pixels: " << new_image_width << "x" << new_image_height);

			std::unique_ptr<uint32_t[]> new_pixels(new uint32_t[new_image_width * new_image_height]);
			const uint32_t* old_pixels = static_cast<const uint32_t*>(inp->pixels());

			for(int y = 0; y != new_image_height; y += 2) {
				for(int x = 0; x != new_image_width; x += 2) {
					const int px = static_cast<int>(ratio_x * x);
					const int py = static_cast<int>(ratio_y * y);
					const int pix_index = py * old_image_width + px;

					const uint32_t P = old_pixels[CLAMP_XY(px+1-1, py+1-1 ,old_image_width, old_image_height)];

					const uint32_t A = old_pixels[CLAMP_XY(px+1-1, py+0-1 ,old_image_width, old_image_height)];
					const uint32_t B = old_pixels[CLAMP_XY(px+2-1, py+1-1 ,old_image_width, old_image_height)];
					const uint32_t C = old_pixels[CLAMP_XY(px+0-1, py+1-1 ,old_image_width, old_image_height)];
					const uint32_t D = old_pixels[CLAMP_XY(px+1-1, py+2-1 ,old_image_width, old_image_height)];

					/*
						  A    --\ 1 2
						C P B  --/ 3 4
						  D
						1=P; 2=P; 3=P; 4=P;
						IF C==A AND C!=D AND A!=B => 1=A
						IF A==B AND A!=C AND B!=D => 2=B
						IF B==D AND B!=A AND D!=C => 4=D
						IF D==C AND D!=B AND C!=A => 3=C
					*/
					uint32_t outp[4] = { P, P, P, P };
					if(C == A && C != D && A != B) {
						outp[0] = A;
					}
					if(A == B && A != C && B != D) {
						outp[1] = B;
					}
					if(B == D && B != A && D != C) {
						outp[3] = D;
					}
					if(D == C && D != B && C != A) {
						outp[2] = C;
					}
					new_pixels[y * new_image_width + x]		  = outp[0];
					new_pixels[y * new_image_width + x + 1]   = outp[1];
					new_pixels[(y+1) * new_image_width + x]   = outp[2];
					new_pixels[(y+1) * new_image_width + x+1] = outp[3];
				}
			}

			return Surface::create(new_image_width, new_image_height, 32, 4*new_image_width, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, new_pixels.get());
		}
	}
}
