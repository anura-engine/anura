// Copyright 2007-2008 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an AS IS BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: madscience@google.com (Moshe Looks)

/****
     This header mainly exists because the boost iterator-creation helpers have
     intolerably long names, e.g.:
     boost::make_permutation_iterator(e,i) vs. the::permute_it(e,i).
     It also adds repeat and pair iterators.
****/


#ifndef _TREE_ITERATOR_SHORTHANDS_HPP_
#define _TREE_ITERATOR_SHORTHANDS_HPP_

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/permutation_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/reverse_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>

#ifndef TREE_TREE_NAMESPACE
#define TREE_TREE_NAMESPACE the
#endif

namespace TREE_TREE_NAMESPACE {

template<typename Incrementable>
inline boost::counting_iterator<Incrementable> 
count_it(Incrementable i) { return boost::make_counting_iterator(i); }

template<typename ElementIterator,typename IndexIterator>
inline boost::permutation_iterator<ElementIterator,IndexIterator>
permute_it(ElementIterator e,IndexIterator i) {
  return boost::make_permutation_iterator(e,i);
}

template<typename UnaryFunction,typename Iterator>
inline boost::transform_iterator<UnaryFunction,Iterator>
transform_it(Iterator i,UnaryFunction fun) { 
  return boost::make_transform_iterator(i,fun);
}

template<typename Iterator>
inline boost::indirect_iterator<Iterator> 
indirect_it(Iterator i) { return boost::make_indirect_iterator(i); }

template<typename BidirectionalIterator>
inline boost::reverse_iterator<BidirectionalIterator>
reverse_it(BidirectionalIterator i) { return boost::make_reverse_iterator(i); }

template<typename Predicate,typename Iterator>
inline boost::filter_iterator<Predicate,Iterator>
filter_it(Iterator i,Iterator j,Predicate p) { 
  return boost::make_filter_iterator(p,i,j); 
}

template<typename Value>
struct repetition_iterator
    : public boost::iterator_facade<repetition_iterator<Value>,Value,
                                    boost::random_access_traversal_tag,
                                    const Value&> {
  repetition_iterator(const Value& v,std::size_t i) : _v(v),_i(i) {}
 protected:
  Value _v;
  std::size_t _i;

  friend class boost::iterator_core_access;
  const Value& dereference() const { return _v; }
  bool equal(const repetition_iterator& rhs) const { return this->_i==rhs._i; }
  void advance(std::ptrdiff_t d) { _i+=d; }
  void increment() { ++_i; }
  void decrement() { --_i; }
  std::ptrdiff_t distance_to(const repetition_iterator& rhs) const { 
    return rhs._i-this->_i; 
  }
};
template<typename Value>
inline repetition_iterator<Value>
repeat_it(const Value& v,std::size_t i=0) {
  return repetition_iterator<Value>(v,i);
}

template<typename First,typename Second>
struct pair_iterator
    : public boost::iterator_facade<pair_iterator<First,Second>,
                                    std::pair<
                                      typename std::iterator_traits<
                                        First>::value_type,
                                      typename std::iterator_traits<
                                        Second>::value_type>,
                                    boost::random_access_traversal_tag,
                                    std::pair<
                                      typename std::iterator_traits<
                                        First>::value_type,
                                      typename std::iterator_traits<
                                        Second>::value_type> > {
  typedef std::pair<typename std::iterator_traits<First>::value_type,
                    typename std::iterator_traits<Second>::value_type> pair;
  pair_iterator(First f,Second s) : _f(f),_s(s) {}
 protected:
  First _f;
  Second _s;

  friend class boost::iterator_core_access;
  pair dereference() const { return make_pair(*_f,*_s); }
  bool equal(const pair_iterator& rhs) const { 
    return (this->_f==rhs._f || this->_s==rhs._s); 
  }
  void advance(std::ptrdiff_t d) { _f+=d; _s+=d; }
  void increment() { ++_f; ++_s; }
  void decrement() { --_f; --_s; }
  std::ptrdiff_t distance_to(const pair_iterator& rhs) const { 
    return std::min(rhs._f-this->_f,rhs._s-this->_s);
  }
};
template<typename First,typename Second>
inline pair_iterator<First,Second>
pair_it(First f,Second s) { return pair_iterator<First,Second>(f,s); }

} //namespace TREE_TREE_NAMESPACE
#endif //_TREE_ITERATOR_SHORTHANDS_HPP_
