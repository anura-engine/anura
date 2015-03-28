#include <assert.h>
#include <algorithm>
#include <map>
#include <vector>

#include "asserts.hpp"
#include "formula_garbage_collector.hpp"
#include "logger.hpp"
#include "profile_timer.hpp"

#include "formula_object.hpp"

	
namespace {
	GarbageCollectible* g_head;
	int g_count;
}

GarbageCollectible::GarbageCollectible() : reference_counted_object(), next_(g_head), prev_(nullptr)
{
	insertAtHead();
}

GarbageCollectible::GarbageCollectible(const GarbageCollectible& o) : reference_counted_object(o), next_(g_head), prev_(nullptr)
{
	insertAtHead();
}

void GarbageCollectible::insertAtHead()
{
	++g_count;
	if(g_head != nullptr) {
		g_head->prev_ = this;
	}

	g_head = this;
}

GarbageCollectible& GarbageCollectible::operator=(const GarbageCollectible& o)
{
	return *this;
}

GarbageCollectible::~GarbageCollectible()
{
	--g_count;
	if(prev_ != nullptr) {
		prev_->next_ = next_;
	}

	if(next_ != nullptr) {
		next_->prev_ = prev_;
	}

	if(g_head == this) {
		g_head = next_;
	}
}

void GarbageCollectible::surrenderReferences(GarbageCollector* collector)
{
}

std::string GarbageCollectible::debugObjectName() const
{
	return typeid(*this).name();
}

GarbageCollector::~GarbageCollector()
{
}

namespace {
	struct ObjectRecord {
		int begin_variant, end_variant, begin_pointer, end_pointer;
	};

	struct PointerPair {
		boost::intrusive_ptr<GarbageCollectible>* ptr;
		GarbageCollectible* points_to;
	};
}

class GarbageCollectorImpl : public GarbageCollector
{
public:

	void surrenderVariant(const variant* v, const char* description) override;
	void surrenderPtrInternal(boost::intrusive_ptr<GarbageCollectible>* ptr, const char* description) override;

	void collect();

private:

	void destroyReferences(GarbageCollectible* item);
	void restoreReferences(GarbageCollectible* item);

	std::vector<variant*> variants_;
	std::vector<PointerPair> pointers_;

	std::map<GarbageCollectible*, ObjectRecord> records_;
};

void GarbageCollectorImpl::surrenderVariant(const variant* v, const char* description)
{
	switch(v->type_ ) {
	case variant::VARIANT_TYPE_LIST:
	case variant::VARIANT_TYPE_MAP:
	case variant::VARIANT_TYPE_CALLABLE:
	case variant::VARIANT_TYPE_FUNCTION:
	case variant::VARIANT_TYPE_GENERIC_FUNCTION:
	case variant::VARIANT_TYPE_MULTI_FUNCTION:
		const_cast<variant*>(v)->release();
		variants_.push_back(const_cast<variant*>(v));
		break;
	default:
		break;
	}
}

void GarbageCollectorImpl::surrenderPtrInternal(boost::intrusive_ptr<GarbageCollectible>* ptr, const char* description)
{
	if(ptr->get() == NULL) {
		return;
	}

	PointerPair p = { ptr, ptr->get() };
	pointers_.push_back(p);
	ptr->reset();
}

void GarbageCollectorImpl::destroyReferences(GarbageCollectible* item)
{
	auto itor = records_.find(item);
	ASSERT_LOG(itor != records_.end(), "Could not find item in GC");
	const ObjectRecord& record = itor->second;
	for(int n = record.begin_variant; n != record.end_variant; ++n) {
		variants_[n]->increment_refcount();
		*variants_[n] = variant();
	}
}


void GarbageCollectorImpl::restoreReferences(GarbageCollectible* item)
{
	auto itor = records_.find(item);
	ASSERT_LOG(itor != records_.end(), "Could not find item in GC");
	const ObjectRecord& record = itor->second;
	for(int n = record.begin_variant; n != record.end_variant; ++n) {
		variants_[n]->increment_refcount();
	}

	for(int n = record.begin_pointer; n != record.end_pointer; ++n) {
		pointers_[n].ptr->reset(pointers_[n].points_to);
	}
}

void GarbageCollectorImpl::collect()
{
	LOG_INFO("Beginning garbage collection of " << g_count << " items");
	profile::timer timer;

	std::vector<GarbageCollectible*> items, saved;
	items.reserve(g_count);

	int count = 0;
	for(GarbageCollectible* p = g_head; p != nullptr; p = p->next_) {
		p->add_ref();
		ASSERT_LOG(p->refcount() > 1, "Object with bad refcount: " << p->refcount());
		items.push_back(p);
		++count;
	}

	for(GarbageCollectible* p : items) {
		ObjectRecord& record = records_[p];
		record.begin_variant = variants_.size();
		record.begin_pointer = pointers_.size();
		p->surrenderReferences(this);
		record.end_variant = variants_.size();
		record.end_pointer = pointers_.size();
	}

	int nlast = -1;
	while(nlast != items.size()) {
		nlast = items.size();

		int save_count = 0;
		for(GarbageCollectible*& item : items) {
			if(item->refcount() == 1) {
				continue;
			}

			++save_count;

			restoreReferences(item);
			saved.push_back(item);
			item = nullptr;
		}

		items.erase(std::remove(items.begin(), items.end(), nullptr), items.end());
	}

	for(auto item : items) {
		destroyReferences(item);
	}

	for(auto item : items) {
		item->dec_ref();
	}

	for(auto item : saved) {
		item->dec_ref();
	}

	LOG_INFO("Garbage collection complete in " << static_cast<int>(timer.get_time()) << "us. Collected " << items.size() << " objects. " << saved.size() << " objects remaining");
}

namespace {
	struct Node {
		std::string id;
		std::vector<int> out_edges, in_edges;
	};

	struct Edge {
		std::string id;
		int from, to;
	};

	class Graph {
	public:
		explicit Graph(int num_nodes) : nodes_(num_nodes)
		{}

		void setNode(int node, const std::string& label) {
			nodes_[node].id = label;
		}

		void addEdge(int from, int to, const std::string& label) {
			const int edge_id = edges_.size();
			nodes_[from].out_edges.push_back(edge_id);
			nodes_[to].in_edges.push_back(edge_id);
			Edge edge = { label, from, to };
			edges_.push_back(edge);
		}

		const Node& getNode(int index) const { assert(index < nodes_.size()); return nodes_[index]; }
		const Edge& getEdge(int index) const { assert(index < edges_.size()); return edges_[index]; }
	private:
		std::vector<Node> nodes_;
		std::vector<Edge> edges_;
	};

	void breadthFirstSearch(const Graph& graph, int start, std::map<int, std::vector<int> >& paths)
	{
		std::set<int> dead, working, next;

		next.insert(start);

		paths[start] = std::vector<int>();

		while(next.empty() == false) {
			for(int node : working) {
				dead.insert(node);
			}

			working = next;
			next.clear();

			for(int node : working) {
				auto path_itor = paths.find(node);
				assert(path_itor != paths.end());
				const Node& v = graph.getNode(node);
				for(int e : v.out_edges) {
					const Edge& edge = graph.getEdge(e);
					if(dead.count(edge.to) || working.count(edge.to) || next.count(edge.to)) {
						continue;
					}

					std::vector<int>& path = paths[edge.to];
					path = path_itor->second;
					path.push_back(e);

					next.insert(edge.to);
				}
			}
		}
	}
}

class GarbageCollectorAnalyzer : public GarbageCollector
{
public:
	GarbageCollectorAnalyzer() : graph_(0), currentIndex_(-1)
	{}
	void run(const char* fname);
	void surrenderVariant(const variant* v, const char* description=nullptr) override;
	void surrenderPtrInternal(boost::intrusive_ptr<GarbageCollectible>* ptr, const char* description) override;

private:
	Graph graph_;
	std::vector<GarbageCollectible*> items_;
	std::map<const void*, int> itemIndexes_;
	int currentIndex_;
};

void GarbageCollectorAnalyzer::surrenderVariant(const variant* v, const char* description)
{
	switch(v->type_ ) {
	case variant::VARIANT_TYPE_LIST:
	case variant::VARIANT_TYPE_MAP:
	case variant::VARIANT_TYPE_CALLABLE:
	case variant::VARIANT_TYPE_FUNCTION:
	case variant::VARIANT_TYPE_GENERIC_FUNCTION:
	case variant::VARIANT_TYPE_MULTI_FUNCTION: {
		auto itor = itemIndexes_.find(v->get_addr());
		if(itor != itemIndexes_.end()) {
			graph_.addEdge(currentIndex_, itor->second, description == NULL ? std::string("(variant)") : std::string(description));
		}
	}
	break;
	default:
		break;
	}
}

void GarbageCollectorAnalyzer::surrenderPtrInternal(boost::intrusive_ptr<GarbageCollectible>* ptr, const char* description)
{
	auto itor = itemIndexes_.find(ptr->get());
	if(itor != itemIndexes_.end()) {
		graph_.addEdge(currentIndex_, itor->second, description == NULL ? std::string("(variant)") : std::string(description));
	}
}

void GarbageCollectorAnalyzer::run(const char* fname)
{
	FILE* out = fopen(fname, "w");
	graph_ = Graph(g_count+1);

	items_.clear();
	items_.reserve(g_count);

	for(GarbageCollectible* p = g_head; p != nullptr; p = p->next_) {
		graph_.setNode(items_.size(), p->debugObjectName());

		itemIndexes_[p] = items_.size();
		items_.push_back(p);

	}

	currentIndex_ = 0;
	for(auto item : items_) {
		item->surrenderReferences(this);
		++currentIndex_;
	}


	int root_node = g_count;
	graph_.setNode(root_node, "(root)");
	for(int i = 0; i != g_count; ++i) {
		const int refcount = items_[i]->refcount();
		int refs = graph_.getNode(i).in_edges.size();
		while(refs < refcount) {
			graph_.addEdge(root_node, i, "root");
			++refs;
		}
	}

	std::map<int, std::vector<int> > paths;
	breadthFirstSearch(graph_, root_node, paths);

	for(int i = 0; i != g_count; ++i) {
		fprintf(out, "REFS: ");
		fprintf(out, "[%s @%p (%d)] ", items_[i]->debugObjectName().c_str(), items_[i], items_[i]->refcount());

		auto path_itor = paths.find(i);
		assert(path_itor != paths.end());

		std::vector<int> path = path_itor->second;
		std::reverse(path.begin(), path.end());

		for(int j : path) {
			int from = graph_.getEdge(j).from;

			if(from == items_.size()) {
				fprintf(out, " <--- [ENGINE]\n");
				break;
			}

			assert(from >= 0 && from < items_.size());
			fprintf(out, " <--%s-- [%s @%p (%d)] ", graph_.getEdge(j).id.c_str(), items_[from]->debugObjectName().c_str(), items_[from], items_[from]->refcount());
		}
	}

	fclose(out);
}

void runGarbageCollection()
{
	GarbageCollectorImpl gc;
	gc.collect();
}

void runGarbageCollectionDebug(const char* fname)
{
	GarbageCollectorImpl gc;
	gc.collect();

	GarbageCollectorAnalyzer().run(fname);
}

