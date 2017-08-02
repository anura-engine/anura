#include <assert.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <thread>

#include <vector>

#include <SDL.h>

#include "asserts.hpp"
#include "formula_garbage_collector.hpp"
#include "formula_profiler.hpp"
#include "logger.hpp"
#include "profile_timer.hpp"
#include "sys.hpp"

#include "formula_object.hpp"

#ifdef DEBUG_GARBAGE_COLLECTOR
namespace ffl {
std::vector<IntrusivePtr<reference_counted_object>*> getAllIntrusivePtrDebug();
}

std::set<variant*>& get_all_global_variants();
#endif
	
namespace {
	GarbageCollectible* g_head;
	int g_count;
	int g_threads;
	SDL_mutex* g_gc_mutex;

	struct LockGC {
		LockGC() {
			if(g_gc_mutex) {
				SDL_mutexP(g_gc_mutex);
			}
		}

		~LockGC() {
			if(g_gc_mutex) {
				SDL_mutexV(g_gc_mutex);
			}
		}
	};

}

std::mutex& GarbageCollector::getGlobalMutex()
{
	static std::mutex instance;
	return instance;
}

void GarbageCollectible::getAll(std::vector<GarbageCollectible*>* result)
{
	for(GarbageCollectible* p = g_head; p != nullptr; p = p->next_) {
		result->push_back(p);
	}
}

void GarbageCollectible::incrementWorkerThreads()
{
	++g_threads;

	if(g_gc_mutex == nullptr) {
		g_gc_mutex = SDL_CreateMutex();
	}
}

void GarbageCollectible::decrementWorkerThreads()
{
	--g_threads;
	if(g_threads == 0) {
		SDL_DestroyMutex(g_gc_mutex);
		g_gc_mutex = nullptr;
	}
}

GarbageCollectible* GarbageCollectible::debugGetObject(void* ptr)
{
	LockGC lock;
	for(GarbageCollectible* p = g_head; p != nullptr; p = p->next_) {
		if(p == ptr) {
			return p;
		}
	}

	return NULL;
}

GarbageCollectible::GarbageCollectible() : reference_counted_object(), prev_(nullptr), tenure_(0)
{
	LockGC lock;
	next_ = g_head;
	insertAtHead();
}

GarbageCollectible::GarbageCollectible(const GarbageCollectible& o) : reference_counted_object(o), prev_(nullptr), tenure_(0)
{
	LockGC lock;
	next_ = g_head;
	insertAtHead();
}

GarbageCollectible::GarbageCollectible(GARBAGE_COLLECTOR_EXCLUDE_OPTIONS options)
  : reference_counted_object(), next_(this), prev_(nullptr), tenure_(0)
{
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
	if(next_ == this) {
		return;
	}

	LockGC lock;

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

std::string GarbageCollectible::debugObjectSpew() const
{
	return debugObjectName();
}

#ifdef DEBUG_GARBAGE_COLLECTOR

const size_t GCAllocSize = 2048;
const size_t GCNumObjects = 100000;
void* g_gc_data_pool[(GCAllocSize*GCNumObjects)/sizeof(void*)];
unsigned char* g_gc_begin_data_pool = (unsigned char*)g_gc_data_pool;
unsigned char* g_gc_end_data_pool = g_gc_begin_data_pool + sizeof(g_gc_data_pool);

std::vector<void*> g_gc_alloc_free_slots;
unsigned char* g_gc_next_spot = g_gc_begin_data_pool;

void* GarbageCollectible::operator new(size_t sz)
{
	if(sz > GCAllocSize-sizeof(uint64_t) || (g_gc_alloc_free_slots.empty() && g_gc_next_spot == g_gc_end_data_pool)) {
		return malloc(sz);
	}

	void* result;
	if(!g_gc_alloc_free_slots.empty()) {
		result = g_gc_alloc_free_slots.back();
		memset(result, 0, GCAllocSize);
		g_gc_alloc_free_slots.pop_back();
	} else {
		result = g_gc_next_spot;
		memset(result, 0, GCAllocSize);
		g_gc_next_spot += GCAllocSize;
	}

	uint64_t* p = (uint64_t*)result;
	*p = sz;
	++p;
	return (void*)p;
}

void GarbageCollectible::operator delete(void* ptr) noexcept
{
	if(ptr < g_gc_begin_data_pool || ptr >= g_gc_end_data_pool) {
		free(ptr);
		return;
	}

	uint64_t* p = (uint64_t*)ptr;
	--p;
	g_gc_alloc_free_slots.push_back(p);
}

#endif //DEBUG_GARBAGE_COLLECTOR

GarbageCollector::~GarbageCollector()
{
}

namespace {
	struct ObjectRecord {
		int begin_variant, end_variant, begin_pointer, end_pointer;
	};

	struct PointerPair {
		ffl::IntrusivePtr<GarbageCollectible>* ptr;
		GarbageCollectible* points_to;
	};
}

class GarbageCollectorImpl : public GarbageCollector
{
public:
	GarbageCollectorImpl(int num_gens=-1) : gens_(num_gens)
	{}

	void surrenderVariant(const variant* v, const char* description) override;
	void surrenderPtrInternal(ffl::IntrusivePtr<GarbageCollectible>* ptr, const char* description) override;

	void collect();
	void reap();
	void debugOutputCollected();

private:
	void accumulateAll();
	void performCollection();

	void destroyReferences(GarbageCollectible* item);
	void restoreReferences(GarbageCollectible* item);

	std::vector<variant*> variants_;
	std::vector<PointerPair> pointers_;

	std::map<GarbageCollectible*, ObjectRecord> records_;

	std::vector<GarbageCollectible*> items_, saved_;

	int gens_;
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
		if(std::binary_search(items_.begin(), items_.end(), v->get_addr()) == false) {
			break;
		}

		const_cast<variant*>(v)->release();
		variants_.emplace_back(const_cast<variant*>(v));
		break;
	default:
		break;
	}
}

void GarbageCollectorImpl::surrenderPtrInternal(ffl::IntrusivePtr<GarbageCollectible>* ptr, const char* description)
{
	if(ptr->get() == NULL) {
		return;
	}

	if(std::binary_search(items_.begin(), items_.end(), ptr->get()) == false) {
		return;
	}

	PointerPair p = { ptr, ptr->get() };
	pointers_.emplace_back(p);
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
	LockGC lock;

	LOG_DEBUG("Beginning garbage collection of " << g_count << " items");
	profile::timer timer;

	accumulateAll();
	performCollection();

	LOG_DEBUG("Garbage collection complete in " << static_cast<int>(timer.get_time()) << "us. Collected " << items_.size() << " objects. " << saved_.size() << " objects remaining; variants: " << variants_.size() << "; pointers: " << pointers_.size());
}

void GarbageCollectorImpl::accumulateAll()
{
	items_.reserve(g_count);

	for(GarbageCollectible* p = g_head; p != nullptr; p = p->next_) {
		if(gens_ < 0 || p->tenure_ < gens_) {
			p->add_reference();
			ASSERT_LOG(p->refcount() > 1, "Object with bad refcount: " << p->refcount() << ": " << p->debugObjectName());
			items_.push_back(p);
		} else if(p->tenure_ >= gens_) {
			//the list of objects is sorted in order of tenure,
			//since we always add at the head, so we don't need to continue
			//once we found one already tenured.
			break;
		}
	}

	std::sort(items_.begin(), items_.end());

	pointers_.reserve(items_.size()*2);
	variants_.reserve(items_.size()*2);

	for(GarbageCollectible* p : items_) {
		ObjectRecord& record = records_[p];
		record.begin_variant = variants_.size();
		record.begin_pointer = pointers_.size();
		p->surrenderReferences(this);
		record.end_variant = variants_.size();
		record.end_pointer = pointers_.size();
	}
}

void GarbageCollectorImpl::performCollection()
{
	int nlast = -1;
	while(nlast != items_.size()) {
		nlast = items_.size();

		int save_count = 0;
		for(GarbageCollectible*& item : items_) {
			if(item->refcount() == 1) {
				continue;
			}

			++save_count;

			restoreReferences(item);
			saved_.push_back(item);
			item->tenure_++;
			item = nullptr;
		}

		items_.erase(std::remove(items_.begin(), items_.end(), nullptr), items_.end());
	}

	for(auto item : items_) {
		destroyReferences(item);
	}
}

void GarbageCollectorImpl::reap()
{
	LockGC lock;
	profile::timer timer;

	for(auto item : saved_) {
		item->dec_reference();
	}

	for(auto item : items_) {
		item->dec_reference();
	}

	LOG_DEBUG("Garbage collection reap in " << static_cast<int>(timer.get_time()) << "us.");
}

void GarbageCollectorImpl::debugOutputCollected()
{
	LockGC lock;
	std::map<std::string, int> m;

	LOG_INFO("--DELETE REPORT--\n");

	std::map<std::string, int> obj_counts;
	for(auto item : items_) {
		obj_counts[item->debugObjectName()]++;
	}

	std::vector<std::pair<int, std::string> > obj_counts_sorted;
	for(auto p : obj_counts) {
		obj_counts_sorted.push_back(std::pair<int, std::string>(p.second, p.first));
	}

	int ncount = 0;
	std::sort(obj_counts_sorted.begin(), obj_counts_sorted.end());
	for(auto p : obj_counts_sorted) {
		LOG_INFO("  RELEASE: " << p.first << " x " << p.second);
		ncount += p.first;
	}

	LOG_INFO("DELETED " << ncount << " OBJECTS");
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
	void surrenderPtrInternal(ffl::IntrusivePtr<GarbageCollectible>* ptr, const char* description) override;

private:
	Graph graph_;
	std::vector<GarbageCollectible*> items_;
	std::set<GarbageCollectible*> itemsSet_;
	std::map<const void*, int> itemIndexes_;
	int currentIndex_;

	std::vector<const variant*> currentVariants_;
	std::vector<ffl::IntrusivePtr<GarbageCollectible>*> currentPtrs_;

	std::vector<const variant*> allVariants_;
	std::vector<ffl::IntrusivePtr<GarbageCollectible>*> allPtrs_;
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
		currentVariants_.push_back(v);
		allVariants_.push_back(v);

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

void GarbageCollectorAnalyzer::surrenderPtrInternal(ffl::IntrusivePtr<GarbageCollectible>* ptr, const char* description)
{
	currentPtrs_.push_back(ptr);
	allPtrs_.push_back(ptr);

	auto itor = itemIndexes_.find(ptr->get());
	if(itor != itemIndexes_.end()) {
		graph_.addEdge(currentIndex_, itor->second, description == NULL ? std::string("(variant)") : std::string(description));
	}
}

void GarbageCollectorAnalyzer::run(const char* fname)
{
	LockGC lock;
	FILE* out = fopen(fname, "w");
	graph_ = Graph(g_count+1);

	items_.clear();
	items_.reserve(g_count);

	for(GarbageCollectible* p = g_head; p != nullptr; p = p->next_) {
		graph_.setNode(items_.size(), p->debugObjectName());

		itemIndexes_[p] = items_.size();
		items_.push_back(p);
		itemsSet_.insert(p);
	}

	currentIndex_ = 0;
	for(auto item : items_) {
		currentVariants_.clear();
		currentPtrs_.clear();
		item->surrenderReferences(this);

#ifdef DEBUG_GARBAGE_COLLECTOR
		if((unsigned char*)item >= g_gc_begin_data_pool && (unsigned char*)item < g_gc_end_data_pool) {
			void** p = (void**)item;
			uint64_t* sz = (uint64_t*)item;
			--sz;
			const int usable_size = static_cast<int>(*sz);
			for(int n = 0; n < usable_size/sizeof(void*); ++n, ++p) {
				if(p == (void**)&item->next_ || p == (void**)&item->prev_) {
					continue;
				}

				if(itemsSet_.count(reinterpret_cast<GarbageCollectible*>(*p)) == 0) {
					continue;
				}

				bool found_variant = false;
				for(const variant* v : currentVariants_) {
					if((void*)(p) >= (void*)(v) && (void*)(p) < (void*)(v+1)) {
						found_variant = true;
					}
				}

				if(found_variant) {
					continue;
				}

				for(const ffl::IntrusivePtr<GarbageCollectible>* ptr : currentPtrs_) {
					if((void*)p >= (void*)ptr && (void*)p < (void*)(ptr+1)) {
						found_variant = true;
					}
				}

				if(found_variant) {
					continue;
				}

				for(const variant* v : currentVariants_) {
					const int offset = (const unsigned char*)v - (const unsigned char*)item;
					fprintf(stderr, "  VARIANT OFFSET: %x - %d\n", offset, (int)sizeof(variant));
				}

				for(const ffl::IntrusivePtr<GarbageCollectible>* ptr : currentPtrs_) {
					const int offset = (const unsigned char*)ptr - (const unsigned char*)item;
					fprintf(stderr, "  PTR OFFSET: %d - %d\n", offset, (int)sizeof(variant));
				}

				GarbageCollectible* collectible = reinterpret_cast<GarbageCollectible*>(*p);

				const int offset = n*sizeof(void*);

				fprintf(stderr, "GC UNMARKED REFERENCE: [%s @%p] -> [%s @%p] OFFSET: %d/%d\n", item->debugObjectName().c_str(), item, collectible->debugObjectName().c_str(), collectible, offset, usable_size);
			}
		}
#endif

		++currentIndex_;
	}

	std::sort(allVariants_.begin(), allVariants_.end());
	std::sort(allPtrs_.begin(), allPtrs_.end());

#ifdef DEBUG_GARBAGE_COLLECTOR
	std::vector<ffl::IntrusivePtr<reference_counted_object>*> intrusives = ffl::getAllIntrusivePtrDebug();
	std::map<void*, std::vector<ffl::IntrusivePtr<reference_counted_object>*>> intrusive_dests;
	for(auto p : intrusives) {
		intrusive_dests[p->get()].push_back(p);
	}

	for(auto item : items_) {
		if(intrusive_dests.count(item)) {
			auto& v = intrusive_dests[item];
			for(auto p : v) {
				if(std::binary_search(allPtrs_.begin(), allPtrs_.end(), (ffl::IntrusivePtr<GarbageCollectible>*)p)) {
					continue;
				}

				fprintf(stderr, "GC UNKNOWN INTRUSIVE @%p -> [%s @%p]\n", p, item->debugObjectName().c_str(), item);
			}
		}
	}

	std::set<variant*> all_variants = get_all_global_variants();
	std::map<void*, std::vector<variant*>> variant_dests;

	for(variant* v : all_variants) {
		if(v->is_callable()) {
			variant_dests[(void*)v->as_callable()].push_back(v);
		}
	}

	for(auto item : items_) {
		if(variant_dests.count(item)) {
			auto& v = variant_dests[item];
			for(auto p : v) {
				if(std::binary_search(allVariants_.begin(), allVariants_.end(), p)) {
					continue;
				}

				fprintf(stderr, "GC UNKNOWN VARIANT @%p -> [%s @%p]\n", p, item->debugObjectName().c_str(), item);
			}
		}
	}

	for(auto item : items_) {
		auto& variants = variant_dests[(void*)item];
		auto& ptrs = intrusive_dests[(void*)item];
		if((int)variants.size() + (int)ptrs.size() != item->refcount()) {
			fprintf(stderr, "GC REFCOUNT DISCREPANCY: %d variants, %d pointers != %d refcount (pointers: %d; variants: %d) for %s @%p\n", (int)variants.size(), (int)ptrs.size(), item->refcount(), item->ptr_count_, item->variant_count_, item->debugObjectName().c_str(), item);
		}
	}

#endif

	std::map<std::string, int> obj_counts;
	for(int i = 0; i != g_count; ++i) {
		obj_counts[items_[i]->debugObjectName()]++;
	}

	std::vector<std::pair<int, std::string> > obj_counts_sorted;
	for(auto p : obj_counts) {
		obj_counts_sorted.push_back(std::pair<int, std::string>(p.second, p.first));
	}

	std::sort(obj_counts_sorted.begin(), obj_counts_sorted.end());
	int ncount = 0;
	for(auto p : obj_counts_sorted) {
		fprintf(out, "%4d x %s\n", p.first, p.second.c_str());
		ncount += p.first;
	}

	fprintf(out, "TOTAL OBJECTS: %d\n", ncount);


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
		if(path_itor == paths.end()) {
			fprintf(out, "(UNFOUND)\n");
			continue;
		}
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

namespace {
	std::vector<std::shared_ptr<GarbageCollectorImpl>> g_reapable_gc;
}

void runGarbageCollection(int num_gens, bool mandatory)
{
	if(mandatory) {
		GarbageCollector::getGlobalMutex().lock();
	} else if(GarbageCollector::getGlobalMutex().try_lock() == false) {
		return;
	}

	std::lock_guard<std::mutex> lock(GarbageCollector::getGlobalMutex(), std::adopt_lock_t());
	
	reapGarbageCollection();

	formula_profiler::Instrument instrument("GC");
	std::shared_ptr<GarbageCollectorImpl> gc(new GarbageCollectorImpl(num_gens));
	gc->collect();
	gc->reap();
//	g_reapable_gc.push_back(gc);
}

void reapGarbageCollection()
{
	for(auto gc : g_reapable_gc) {
		formula_profiler::Instrument instrument("GC");
		gc->reap();
	}

	g_reapable_gc.clear();
}

void runGarbageCollectionDebug(const char* fname)
{
	reapGarbageCollection();

	GarbageCollectorImpl gc;
	gc.collect();
	gc.reap();

	GarbageCollectorAnalyzer().run(fname);
}

