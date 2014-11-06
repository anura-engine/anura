/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef COLOR_CHART_HPP_INCLUDED
#define COLOR_CHART_HPP_INCLUDED

#include <map>

#include "graphics.hpp"

namespace graphics {

const SDL_Color& get_color_from_name(std::string name);

const SDL_Color& color_black();
const SDL_Color& color_white();
const SDL_Color& color_red();
const SDL_Color& color_green();
const SDL_Color& color_blue();
const SDL_Color& color_yellow();
const SDL_Color& color_grey();
const SDL_Color& color_snow();
const SDL_Color& color_snow_2();
const SDL_Color& color_snow_3();
const SDL_Color& color_snow_4();
const SDL_Color& color_ghost_white();
const SDL_Color& color_white_smoke();
const SDL_Color& color_gainsboro();
const SDL_Color& color_floral_white();
const SDL_Color& color_old_lace();
const SDL_Color& color_linen();
const SDL_Color& color_antique_white();
const SDL_Color& color_antique_white_2();
const SDL_Color& color_antique_white_3();
const SDL_Color& color_antique_white_4();
const SDL_Color& color_papaya_whip();
const SDL_Color& color_blanched_almond();
const SDL_Color& color_bisque();
const SDL_Color& color_bisque_2();
const SDL_Color& color_bisque_3();
const SDL_Color& color_bisque_4();
const SDL_Color& color_peach_puff();
const SDL_Color& color_peach_puff_2();
const SDL_Color& color_peach_puff_3();
const SDL_Color& color_peach_puff_4();
const SDL_Color& color_navajo_white();
const SDL_Color& color_moccasin();
const SDL_Color& color_cornsilk();
const SDL_Color& color_cornsilk_2();
const SDL_Color& color_cornsilk_3();
const SDL_Color& color_cornsilk_4();
const SDL_Color& color_ivory();
const SDL_Color& color_ivory_2();
const SDL_Color& color_ivory_3();
const SDL_Color& color_ivory_4();
const SDL_Color& color_lemon_chiffon();
const SDL_Color& color_seashell();
const SDL_Color& color_seashell_2();
const SDL_Color& color_seashell_3();
const SDL_Color& color_seashell_4();
const SDL_Color& color_honeydew();
const SDL_Color& color_honeydew_2();
const SDL_Color& color_honeydew_3();
const SDL_Color& color_honeydew_4();
const SDL_Color& color_mint_cream();
const SDL_Color& color_azure();
const SDL_Color& color_alice_blue();
const SDL_Color& color_lavender();
const SDL_Color& color_lavender_blush();
const SDL_Color& color_misty_rose();
const SDL_Color& color_dark_slate_gray();
const SDL_Color& color_dim_gray();
const SDL_Color& color_slate_gray();
const SDL_Color& color_light_slate_gray();
const SDL_Color& color_gray();
const SDL_Color& color_light_gray();
const SDL_Color& color_midnight_blue();
const SDL_Color& color_navy();
const SDL_Color& color_cornflower_blue();
const SDL_Color& color_dark_slate_blue();
const SDL_Color& color_slate_blue();
const SDL_Color& color_medium_slate_blue();
const SDL_Color& color_light_slate_blue();
const SDL_Color& color_medium_blue();
const SDL_Color& color_royal_blue();
const SDL_Color& color_dodger_blue();
const SDL_Color& color_deep_sky_blue();
const SDL_Color& color_sky_blue();
const SDL_Color& color_light_sky_blue();
const SDL_Color& color_steel_blue();
const SDL_Color& color_light_steel_blue();
const SDL_Color& color_light_blue();
const SDL_Color& color_powder_blue();
const SDL_Color& color_pale_turquoise();
const SDL_Color& color_dark_turquoise();
const SDL_Color& color_medium_turquoise();
const SDL_Color& color_turquoise();
const SDL_Color& color_cyan();
const SDL_Color& color_light_cyan();
const SDL_Color& color_cadet_blue();
const SDL_Color& color_medium_aquamarine();
const SDL_Color& color_aquamarine();
const SDL_Color& color_dark_green();
const SDL_Color& color_dark_olive_green();
const SDL_Color& color_dark_sea_green();
const SDL_Color& color_sea_green();
const SDL_Color& color_medium_sea_green();
const SDL_Color& color_light_sea_green();
const SDL_Color& color_pale_green();
const SDL_Color& color_spring_green();
const SDL_Color& color_lawn_green();
const SDL_Color& color_chartreuse();
const SDL_Color& color_medium_spring_green();
const SDL_Color& color_green_yellow();
const SDL_Color& color_lime_green();
const SDL_Color& color_yellow_green();
const SDL_Color& color_forest_green();
const SDL_Color& color_olive_drab();
const SDL_Color& color_dark_khaki();
const SDL_Color& color_khaki();
const SDL_Color& color_pale_goldenrod();
const SDL_Color& color_light_goldenrod_yellow();
const SDL_Color& color_light_yellow();
const SDL_Color& color_gold();
const SDL_Color& color_light_goldenrod();
const SDL_Color& color_goldenrod();
const SDL_Color& color_dark_goldenrod();
const SDL_Color& color_rosy_brown();
const SDL_Color& color_indian_red();
const SDL_Color& color_saddle_brown();
const SDL_Color& color_sienna();
const SDL_Color& color_peru();
const SDL_Color& color_burlywood();
const SDL_Color& color_beige();
const SDL_Color& color_wheat();
const SDL_Color& color_sandy_brown();
const SDL_Color& color_tan();
const SDL_Color& color_chocolate();
const SDL_Color& color_firebrick();
const SDL_Color& color_brown();
const SDL_Color& color_dark_salmon();
const SDL_Color& color_salmon();
const SDL_Color& color_light_salmon();
const SDL_Color& color_orange();
const SDL_Color& color_dark_orange();
const SDL_Color& color_coral();
const SDL_Color& color_light_coral();
const SDL_Color& color_tomato();
const SDL_Color& color_orange_red();
const SDL_Color& color_hot_pink();
const SDL_Color& color_deep_pink();
const SDL_Color& color_pink();
const SDL_Color& color_light_pink();
const SDL_Color& color_pale_violet_red();
const SDL_Color& color_maroon();
const SDL_Color& color_medium_violet_red();
const SDL_Color& color_violet_red();
const SDL_Color& color_violet();
const SDL_Color& color_plum();
const SDL_Color& color_orchid();
const SDL_Color& color_medium_orchid();
const SDL_Color& color_dark_orchid();
const SDL_Color& color_dark_violet();
const SDL_Color& color_blue_violet();
const SDL_Color& color_purple();
const SDL_Color& color_medium_purple();
const SDL_Color& color_thistle();
const SDL_Color& color_crimson();

}

#endif
