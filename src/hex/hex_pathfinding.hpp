/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <tuple>
#include <boost/graph/adjacency_list.hpp>

#include "geometry.hpp"
//#include "game_state.hpp"
#include "hex_logical_fwd.hpp"

namespace hex
{
	typedef float cost;
	typedef point node_type;
	typedef boost::adjacency_list<boost::listS, boost::vecS, boost::undirectedS, boost::no_property, boost::property<boost::edge_weight_t, cost>> hex_graph;
	typedef boost::property_map<hex_graph, boost::edge_weight_t>::type WeightMap;
	typedef hex_graph::vertex_descriptor vertex;
	typedef hex_graph::edge_descriptor edge_descriptor;
	typedef std::pair<point, point> edge;

	struct graph_t
	{
		graph_t(size_t size) : graph(size) {}
		hex_graph graph;
		std::map<point, int> reverse_map;
		std::vector<point> vertices;
	};
	typedef std::shared_ptr<graph_t> hex_graph_ptr;

	typedef std::vector<point> result_path;

	//hex_graph_ptr create_cost_graph(const game::state& gs, const point& src, float max_cost);
	//hex_graph_ptr create_graph(const game::state& gs, int x=0, int y=0, int w=0, int h=0);
	result_list find_available_moves(hex_graph_ptr graph, const point& src, float max_cost);
	result_path find_path(hex_graph_ptr graph, const point& src, const point& dst);
}
