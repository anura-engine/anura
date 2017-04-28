#pragma once

#include <mutex>

#include "intrusive_ptr.hpp"

#include "reference_counted_object.hpp"
#include "variant.hpp"

enum GARBAGE_COLLECTOR_EXCLUDE_OPTIONS { GARBAGE_COLLECTOR_EXCLUDE };

class GarbageCollector;

class GarbageCollectible : public reference_counted_object
{
public:
	static void getAll(std::vector<GarbageCollectible*>* result);
	static void incrementWorkerThreads();
	static void decrementWorkerThreads();
	static GarbageCollectible* debugGetObject(void* ptr);
	GarbageCollectible();
	GarbageCollectible(const GarbageCollectible& o);
	explicit GarbageCollectible(GARBAGE_COLLECTOR_EXCLUDE_OPTIONS option);
	virtual ~GarbageCollectible();

	GarbageCollectible& operator=(const GarbageCollectible& o);

	virtual void surrenderReferences(GarbageCollector* collector);

	virtual std::string debugObjectName() const;
	virtual std::string debugObjectSpew() const;

	friend class GarbageCollectorImpl;
	friend class GarbageCollectorAnalyzer;

#ifdef DEBUG_GARBAGE_COLLECTOR
	void* operator new(size_t sz);
	void operator delete(void* ptr) noexcept;
#endif
private:
	void insertAtHead();
	GarbageCollectible* next_;
	GarbageCollectible* prev_;

	int tenure_;
};

class GarbageCollector
{
public:
	static std::mutex& getGlobalMutex();
	virtual ~GarbageCollector();
	virtual void surrenderVariant(const variant* v, const char* description=nullptr) = 0;
	template<typename T>
	void surrenderPtr(const ffl::IntrusivePtr<T>* ptr, const char* description=nullptr) {
		ffl::IntrusivePtr<const GarbageCollectible> ensureCopyable = *ptr;
		surrenderPtrInternal((ffl::IntrusivePtr<GarbageCollectible>*)ptr, description);
	}

private:

	virtual void surrenderPtrInternal(ffl::IntrusivePtr<GarbageCollectible>* ptr, const char* description) = 0;
};

void runGarbageCollection(int num_gens=-1, bool mandatory=true);
void reapGarbageCollection();
void runGarbageCollectionDebug(const char* fname);
