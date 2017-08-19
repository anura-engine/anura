/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include "decimal.hpp"
#include "formula_callable_definition.hpp"
#include "formula_callable.hpp"
#include "formula_function.hpp"
#include "geometry.hpp"
#include "variant.hpp"

// Need to forward declare this rather than including level.hpp
class Level;
typedef ffl::IntrusivePtr<Level> LevelPtr;

namespace pathfinding 
{
	template<typename N>
	struct PathfindingException 
	{
		const char* msg;
		const N src;
		const N dest;
	};

	typedef variant_pair graph_edge;
	typedef std::map<variant, std::vector<variant> > graph_edge_list;
	typedef std::map<graph_edge, decimal> edge_weights;

	class DirectedGraph;
	class WeightedDirectedGraph;
	typedef ffl::IntrusivePtr<DirectedGraph> DirectedGraphPtr;
	typedef ffl::IntrusivePtr<WeightedDirectedGraph> WeightedDirectedGraphPtr;

	template<typename N, typename T>
	class GraphNode {
	public:
		typedef std::shared_ptr<GraphNode<N, T>> GraphNodePtr;
		GraphNode(const N& src) 
			: src_(src), 
			f_(T(0)), 
			g_(T(0)), 
			h_(T(0)), 
			parent_(nullptr),
			on_open_list_(false), 
			on_closed_list_(false)
		{}
		GraphNode(const N& src, T g, T h, GraphNodePtr parent) 
			: f_(g+h), 
			g_(g), 
			h_(h), 
			src_(src), 
			parent_(parent), 
			on_open_list_(false), on_closed_list_
			(false)
		{}
		bool operator< (const GraphNode& rhs) const { return f_ < rhs.f_;}
		N getNodeValue() const {return src_;}
		T F() const {return f_;}
		T G() const {return g_;}
		T H() const {return h_;}
		void G(T g) {f_ += g - g_; g_ = g;}
		void H(T h) {f_ += h - h_; h_ = h;}
		void setCost(T g, T h) {
			g_ = g;
			h_ = h;
			f_ = g+h;
		}
		void setParent(GraphNodePtr parent) {parent_ = parent;}
		GraphNodePtr getParent() const {return parent_;}
		void setOnOpenList(const bool val) {on_open_list_ = val;}
		bool isOnOpenList() const {return on_open_list_;}
		void setOnClosedList(const bool val) {on_closed_list_ = val;}
		bool isOnClosedList() const {return on_closed_list_;}
		void resetNode() {
			on_open_list_ = on_closed_list_ = false;
			f_ = T(0);
			g_ = T(0);
			h_ = T(0);
			parent_ = nullptr;
		}
	private:
		T f_, g_, h_;
		N src_;
		GraphNodePtr parent_;
		bool on_open_list_;
		bool on_closed_list_;
	};

	template<typename N, typename T> inline 
	std::ostream& operator<<(std::ostream& out, const GraphNode<N,T>& n) {
		out << "GNODE: " << n.getNodeValue().to_string() << " : cost( " << n.F() << "," << n.G() << "," << n.H() 
			<< ") : parent(" << (n.getParent() == nullptr ? "nullptr" : n.getParent()->getNodeValue().to_string())
			<< ") : (" << n.isOnOpenList() << "," << n.isOnClosedList() << ")" << std::endl;
		return out;
	}

	template<typename N, typename T>
	bool graph_node_cmp(const typename GraphNode<N,T>::graph_node_ptr& lhs, 
		const typename GraphNode<N,T>::GraphNodePtr& rhs);
	template<typename N, typename T> T manhattan_distance(const N& p1, const N& p2);

	typedef std::map<variant, GraphNode<variant, decimal>::GraphNodePtr > vertex_list;

	class DirectedGraph : public game_logic::FormulaCallable 
	{
		DECLARE_CALLABLE(DirectedGraph);
		std::vector<variant> vertices_;
		graph_edge_list edges_;
	public:
		DirectedGraph(std::vector<variant>* vertices, 
			graph_edge_list* edges )
		{
			// Here we pilfer the contents of vertices and the edges.
			vertices_.swap(*vertices);
			edges_.swap(*edges);
		}
		const graph_edge_list* getEdges() const {return &edges_;}
		std::vector<variant>& getVertices() {return vertices_;}
		std::vector<variant> getEdgesFromNode(const variant node) const {
			graph_edge_list::const_iterator e = edges_.find(node);
			if(e != edges_.end()) {
				return e->second;
			}
			return std::vector<variant>();
		}
	};

	class WeightedDirectedGraph : public game_logic::FormulaCallable 
	{
		DECLARE_CALLABLE(WeightedDirectedGraph);
		edge_weights weights_;
		DirectedGraphPtr dg_;
		vertex_list graph_node_list_;
	public:
		WeightedDirectedGraph(DirectedGraphPtr dg, edge_weights* weights) 
			: dg_(dg)
		{
			weights_.swap(*weights);
			for(const variant& v : dg->getVertices()) {
				graph_node_list_[v] = std::make_shared<GraphNode<variant, decimal>>(v);
			}
		}
		std::vector<variant> getEdgesFromNode(const variant node) const {
			return dg_->getEdgesFromNode(node);
		}
		decimal getWeight(const variant& src, const variant& dest) const {
			edge_weights::const_iterator w = weights_.find(graph_edge(src,dest));
			if(w != weights_.end()) {
				return w->second;
			}
			PathfindingException<variant> weighted_graph_error = {"Couldn't find edge weight for nodes.", src, dest};
			throw weighted_graph_error;
		}
		GraphNode<variant, decimal>::GraphNodePtr getGraphNode(const variant& src) {
			vertex_list::const_iterator it = graph_node_list_.find(src);
			if(it != graph_node_list_.end()) {
				return it->second;
			}
			PathfindingException<variant> src_not_found = {
				"weighted_directed_graph::get_graph_node() No node found having a value of ",
				src,
				variant()
			};
			throw src_not_found;
		}
		void resetGraph() {
			for(auto& p : graph_node_list_) {
				p.second->resetNode();
			}
		}
	};

	std::vector<point> get_neighbours_from_rect(const point &mid_xy,
		const int tile_size_x, 
		const int tile_size_y,
		const rect& b,
		const bool allow_diagonals = true);
	variant point_as_variant_list(const point& pt);

	variant a_star_search(WeightedDirectedGraphPtr wg, 
		const variant src_node, 
		const variant dst_node, 
		variant heuristic_fn);

	variant a_star_find_path(LevelPtr lvl, const point& src, 
		const point& dst, 
		game_logic::ExpressionPtr heuristic, 
		game_logic::ExpressionPtr weight_expr, 
		game_logic::MapFormulaCallablePtr callable, 
		const int tile_size_x, 
		const int tile_size_y);

	variant path_cost_search(WeightedDirectedGraphPtr wg, 
		const variant src_node, 
		decimal max_cost );
}
