#pragma once

#include <boost/intrusive_ptr.hpp>

#include "reference_counted_object.hpp"
#include "variant.hpp"

class GarbageCollector;

class GarbageCollectible : public reference_counted_object
{
public:
	static GarbageCollectible* debugGetObject(void* ptr);
	GarbageCollectible();
	GarbageCollectible(const GarbageCollectible& o);
	virtual ~GarbageCollectible();

	GarbageCollectible& operator=(const GarbageCollectible& o);

	virtual void surrenderReferences(GarbageCollector* collector);

	virtual std::string debugObjectName() const;
	virtual std::string debugObjectSpew() const;

	friend class GarbageCollectorImpl;
	friend class GarbageCollectorAnalyzer;
private:
	void insertAtHead();
	GarbageCollectible* next_;
	GarbageCollectible* prev_;
};

class GarbageCollector
{
public:
	virtual ~GarbageCollector();
	virtual void surrenderVariant(const variant* v, const char* description=nullptr) = 0;
	template<typename T>
	void surrenderPtr(const boost::intrusive_ptr<T>* ptr, const char* description=nullptr) {
		boost::intrusive_ptr<const GarbageCollectible> ensureCopyable = *ptr;
		surrenderPtrInternal((boost::intrusive_ptr<GarbageCollectible>*)ptr, description);
	}
private:

	virtual void surrenderPtrInternal(boost::intrusive_ptr<GarbageCollectible>* ptr, const char* description) = 0;
};

void runGarbageCollection();
void runGarbageCollectionDebug(const char* fname);
