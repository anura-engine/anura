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
     Usage

     See test_runner.cpp for many examples of constructing and manipulating
     trees.

****/

/****
     Implementation Overview

     A tree is a generalization of a doubly linked list with sentinel node to a
     data structure where each node has three pointers instead of two. For a
     normal node the first two function identically to the pointers of a node
     in a double linked list (previous and next, respectively), and the last
     points to the sentinel node of the node's children, or NULL if the node is
     childless. For a sentinel node, the first pointer (previous) points at the
     last real node in the list, the second pointer (next) points at the
     sentinel node's parent (or the node itself if it has no parent), and the
     third points at the first real node in the list. A tree always contains an
     initial "end" sentinel node that is always on the same level as the root
     of the tree.

     This leads to an invariant that any node x in a non-empty tree is a
     sentinel iff x->next->prev != x. Accordingly, no memory overhead is
     required to discriminate between sentinels and real nodes. 

     Because, excepting the "end" sentinel, internal node are in one-to-one
     correspondence with sentinel nodes, the "structural" memory overhead of a
     tree with n internal nodes and m leaves is exactly 6n + 3m + 1
     pointers. An exception to this is if a child iterator (begin() or end())
     is taken of a leaf. In this case an additional placeholder sentinel node
     will be created for the leaf. Of course, even in the worst case the
     overhead will only be 6(n + m) + 1 pointers. When a tree is copied, such
     leaf sentinel nodes are not copied (and of course their existence does not
     affect equality comparisons.
****/

/****
     Warnings and Injunctions

     Currently, any iterator type may be happily implicitly converted to any
     other. This is often convenient, at the cost of risking some serious
     errors which the compiler would otherwise flag. For example, a valid range
     of pre-order iterators, when converted to child iterators, may become
     invalid. It is the user's responsibility to ensure that ranges passed to
     functions remain valid *after* any implicit conversions have been
     performed.

     If this turns out to be too big a nuisance, future versions may take a
     more restrictive approach to implicit iterator type conversions.
****/

/****
     Iterator Types

     pre_iterator
     const_pre_iterator
     sub_pre_iterator
     const_sub_pre_iterator
     child_iterator
     const_child_iterator
     sub_child_iterator
     const_sub_child_iterator
     post_iterator
     const_post_iterator
     sub_post_iterator
     const_sub_post_iterator
****/

/****
     Comparison Operators

     Trees holding an equality-comparable, less-than-comparable, etc. type
     themselves support the corresponding operators. Comparison occurs
     node-by-node in pre-order, until a structural or content difference is
     encountered, according to the following rules (obeyed sequentially):

     1) If a leaf is compared to an internal node, the leaf is lesser,
        regardless of tree contents.

     2) If two nodes, both being equal, or both being leaves, have unequal
        contents, the tree with the lesser contents is lesser.
     
     3) If two internal nodes with equal contents are compared, having
        differing numbers of children, and all children (that are present) are
        equal, the tree with fewer children is lesser.
****/

#ifndef _TREE_TREE_HPP_
#define _TREE_TREE_HPP_

#include <cstddef>
#include <cassert>
#include <functional>
#include <algorithm>
#include <iterator>
#include <boost/next_prior.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/operators.hpp>
#include <boost/bind.hpp>
#include "iterator_shorthands.hpp"

namespace TREE_TREE_NAMESPACE {

//predeclare to make the compiler happy
template<typename T>
struct const_subtree;
template<typename T>
struct subtree;
template<typename T>
struct tree;

//occasionally useful to have this as a standalone function
template<typename Iterator>
Iterator parent(Iterator i) {
  typename Iterator::base_pointer n=i._node;
  while (n->dereferenceable()) {
    if (n->end==n)
      return n;
    n=n->next;
  }
  return Iterator(n->next);
}

namespace _tree_private {

/////////
// node classes

//briliant evil-genius technique adapted from the gnu stl list
//implementation - node_base is used for sentinel nodes, and static_cast to
//node is used for when we want to access the data - this avoids the overhead
//of polymorphism and the wasted sizeof(T) space of using node for sentinels
struct node_base {
  node_base() : prev(this),next(this),end(this) {}
  node_base(node_base* parent) : prev(this),next(parent),end(this) {}
  node_base(node_base* p,node_base* n) : prev(p),next(n),end(NULL) {}
  node_base(node_base* p,node_base* n,node_base* e) : prev(p),next(n),end(e) {}

  node_base* prev;
  node_base* next;
  node_base* end; 

  node_base* sentinel() {     //creates sentinel if childless
    if (node_base* n=end)
      return n;
    return new_sentinel();
  }
  node_base* new_sentinel() { return end=new node_base(this); }
  node_base* first_child() {  //creates (& returns) sentinel if childless
    if (node_base* n=end)
      return n->end;
    return end=new node_base(this);
  }

  bool childless() const { 
    if (node_base* n=end)
      return n->prev==n;
    return true;
  }
  bool dereferenceable() const { return next->prev==this; }

  void set_first_child(node_base* child) {
    child->next=child->prev=end=new node_base(child,this,child);
  }

  void tie_in(node_base* prev,node_base* next) {
    if (prev->dereferenceable())
      prev->next=this;
    else
      prev->end=this;
    next->prev=this;
  }

  void cut_out() const { 
    left_cut(next);
    right_cut(prev);
  }
  void left_cut(node_base* nxt) const {
  if (prev->dereferenceable())
    prev->next=nxt;
  else
    prev->end=nxt;

  }
  void right_cut(node_base* prv) const { next->prev=prv; }
};

template<typename T>
struct node : public node_base {
  typedef T value_type;
  node(node_base* p,node_base* n,const value_type& d)
      : node_base(p,n),data(d) {}
  node(node_base* e,const value_type& d) : node_base(NULL,NULL,e),data(d) {
    if (e)
      e->next=this;
  }
  value_type data;
};

template<typename NodeBase>
inline void ascend(NodeBase*& n) {
  while (!n->dereferenceable())
    n=n->next->next;
}

template<typename NodeBase>
inline void descend(NodeBase*& n) {
  while (!n->childless())
    n=n->end->prev;
}

/////////
// iterator classes

template<typename BasePointer>
struct iter_base {
 protected:
  typedef BasePointer base_pointer;

  base_pointer _node;

  template<typename,typename>
  friend struct tr;
  template<typename,typename>
  friend struct mutable_tr;
  template<typename>
  friend struct const_subtree;
  template<typename>
  friend struct iter;
  template<typename Iterator>
  friend Iterator TREE_TREE_NAMESPACE::parent(Iterator);
};

template<typename SubtreeT,typename IterBase>
struct sub_iter_base : public IterBase {
  typedef SubtreeT result_type;
  typedef SubtreeT reference;
 protected:
  result_type dereference() const { return result_type(this->_node); }
};
template<typename T,typename TRef,typename NodePointer,typename IterBase>
struct value_iter_base : public IterBase {
  typedef T    result_type;
  typedef TRef reference;
 protected:
  reference dereference() const { 
    return static_cast<NodePointer>(this->_node)->data; 
  }
};

template<typename IterBase>
struct pre_iter_base : public IterBase {
 protected:
  void increment() { 
    if (this->_node->childless()) {
      this->_node=this->_node->next;
      ascend(this->_node);
    } else {
      this->_node=this->_node->end->end;
    }
  }
  void decrement() {
    this->_node=this->_node->prev;
    if (this->_node->dereferenceable())
      descend(this->_node);
    else
      this->_node=this->_node->next;
  }
};
template<typename IterBase>
struct child_iter_base : public IterBase { 
 protected:
  void increment() { this->_node=this->_node->next; } 
  void decrement() { this->_node=this->_node->prev; }
};
template<typename IterBase>
struct post_iter_base : public IterBase {
 protected:
  void increment() {
    this->_node=make_post(this->_node->next);
    while (!this->_node->dereferenceable())
      this->_node=this->_node->next;
  }
  void decrement() {
    if (this->_node->childless()) {
      this->_node=this->_node->prev;
      while (!this->_node->dereferenceable())
        this->_node=this->_node->next->prev;
    } else {
      this->_node=this->_node->end->prev;
    }
  }
};
template<typename NodeBasePtr>
NodeBasePtr make_post(NodeBasePtr n) {
  while (!n->childless() && n->dereferenceable() && n->end!=n)
    n=n->end->end;
  return n;
}
  
template<typename IterBase>
struct iter : public IterBase,
              public boost::iterator_facade<iter<IterBase>,
                                            typename IterBase::result_type,
                                            boost::bidirectional_traversal_tag,
                                            typename IterBase::reference> {
  iter() { this->_node=NULL; }
  template <class OtherIterBase>
  iter(const iter<OtherIterBase>& other) { this->_node=other._node; }
  iter(typename IterBase::base_pointer n) { this->_node=n; }

  typedef typename IterBase::reference reference;
 protected:
  friend class boost::iterator_core_access;

  bool equal(iter rhs) const { return this->_node==rhs._node; }
};

/////////
// tree base classes

template<typename T,typename Tree>
struct tr : boost::equality_comparable<tree<T> >,
            boost::equality_comparable<subtree<T> >,
            boost::equality_comparable<const_subtree<T> >,
            boost::less_than_comparable<tree<T> >,
            boost::less_than_comparable<subtree<T> >,
            boost::less_than_comparable<const_subtree<T> > {
  typedef T                          value_type;
  typedef value_type*                pointer;
  typedef value_type&                reference;
  typedef const value_type&          const_reference;
  typedef std::size_t                size_type;
  typedef std::ptrdiff_t             difference_type;
 protected:
  typedef iter_base<const node_base*> const_node_iter_base;
  typedef sub_iter_base<const_subtree<value_type>,
                        const_node_iter_base>  const_sub_iter;
  typedef value_iter_base<T,const T&,const node<T>*,
                          const_node_iter_base> const_value_iter;
 public:
  typedef iter<pre_iter_base<const_value_iter> > const_pre_iterator;
  typedef iter<pre_iter_base<const_sub_iter> > const_sub_pre_iterator;
  typedef iter<child_iter_base<const_value_iter> > const_child_iterator;
  typedef iter<child_iter_base<const_sub_iter> > const_sub_child_iterator;
  typedef iter<post_iter_base<const_value_iter> > const_post_iterator;
  typedef iter<post_iter_base<const_sub_iter> > const_sub_post_iterator;

  typedef const_pre_iterator const_iterator;

  template<typename OtherTr>
  bool operator==(const OtherTr& rhs) const { 
    return this->equal(rhs,std::equal_to<value_type>());
  }
  template<typename OtherTr,typename NodeEq>
  bool equal(const OtherTr& rhs,NodeEq eq) const {
    if (empty())
      return rhs.empty();
    if (rhs.empty())
      return false;

    for (const_pre_iterator i=this->begin(),j=rhs.begin();;) {
      if (i._node->childless()!=j._node->childless() || !eq(*i++,*j++))
        return false;

      if (i==this->end())
        return j==rhs.end();
      if (j==rhs.end())
        return false;   

      if (i._node->next->dereferenceable()!=j._node->next->dereferenceable())
        return false;
    }
  }

  template<typename OtherTr>
  bool operator<(const OtherTr& rhs) const { 
    return this->less(rhs,std::less<value_type>());
  }

  template<typename OtherTr,typename NodeLess>
  bool less(const OtherTr& rhs,NodeLess lt) const {
    if (empty())
      return !rhs.empty();
    if (rhs.empty())
      return false;

    for (const_pre_iterator i=this->begin(),j=rhs.begin();;) {
      if (i._node->childless()) {
        if (!j._node->childless())
          return true;
      } else if (j._node->childless()) {
        return false;
      }

      if (lt(*j,*i))
        return false;
      else if (lt(*i++,*j++))
        return true;

      if (i==this->end())
        return j!=rhs.end();
      if (j==rhs.end())
        return false;

      if (i._node->next->dereferenceable()) {
        if (!j._node->next->dereferenceable())
          return false;
      } else if (j._node->next->dereferenceable()) {
        return true;
      }
    }
  }

  size_type size() const { return std::distance(begin(),end()); }
  size_type arity() const { return std::distance(begin_child(),end_child()); }

  const_pre_iterator begin() const { return this->root_node(); }
  const_pre_iterator end() const { return this->end_node(); }
  const_child_iterator begin_child() const { 
    if (const node_base* n=this->root_node()->end)
      return n->end;
    return NULL;
  }
  const_child_iterator end_child() const { return this->root_node()->end; }
  const_sub_pre_iterator begin_sub() const { return this->root_node(); }
  const_sub_pre_iterator end_sub() const { return this->end_node(); }
  const_sub_child_iterator begin_sub_child() const { 
    if (const node_base* n=this->root_node()->end)
      return n->end;
    return NULL;
  }
  const_sub_child_iterator end_sub_child() const { 
    return this->root_node()->end; 
  }
  const_post_iterator begin_post() const { 
    return make_post(this->root_node()); 
  }
  const_post_iterator end_post() const { return this->end_node(); }
  const_sub_post_iterator begin_sub_post() const { 
    return make_post(this->root_node()); 
  }
  const_sub_post_iterator end_sub_post() const { return this->end_node(); }

  const value_type& root() const { return *this->begin(); }
  const_subtree<T> root_sub() const { return *this->begin_sub(); } 

  const_subtree<T> operator[](size_type idx) const {
    return *boost::next(this->begin_sub_child(),idx);
  }
  const value_type& front() const { return *this->begin_child(); }
  const value_type& back() const { 
    return static_cast<node<T>*>(this->root_node()->end->prev)->data;
  }
  const_subtree<T> front_sub() const { return *this->begin_sub_child(); }
  const_subtree<T> back_sub() const { 
    return (const TREE_TREE_NAMESPACE::_tree_private::node_base*)
        this->root_node()->end->prev;
  }

  bool childless() const { return this->root_node()->childless(); }
  //a tree is flat iff it consists of a root node with childless children
  bool flat() const { 
    return (!this->childless() && 
            std::find_if(this->begin_sub_child(),this->end_sub_child(),
                         !boost::bind(&const_subtree<T>::childless,_1)
                         )==this->end_sub_child());
  }

  //functionality that gets delegated to the subclass
  bool empty() const { return static_cast<const Tree*>(this)->empty(); }
 protected:
  const node_base* root_node() const { 
    return static_cast<const Tree*>(this)->root_node(); 
  }
  const node_base* end_node() const { 
    return static_cast<const Tree*>(this)->end_node(); 
  }
};

template<typename T,typename Tree>
struct mutable_tr : public tr<T,Tree> {
 protected:
  typedef T value_type;

  typedef iter_base<node_base*> node_iter_base;
  typedef sub_iter_base<subtree<value_type>,node_iter_base>  sub_iter;
  typedef value_iter_base<T,T&,node<T>*,node_iter_base> value_iter;
  typedef tr<T,Tree> super;
 public:
  typedef iter<pre_iter_base<value_iter> > pre_iterator;
  typedef iter<pre_iter_base<sub_iter> > sub_pre_iterator;
  typedef iter<child_iter_base<value_iter> > child_iterator;
  typedef iter<child_iter_base<sub_iter> > sub_child_iterator;
  typedef iter<post_iter_base<value_iter> > post_iterator;
  typedef iter<post_iter_base<sub_iter> > sub_post_iterator;

  typedef pre_iterator iterator;

  typedef typename super::size_type size_type;

  //insertion may operate on any valid iterator i
  template<typename Iterator>
  Iterator insert(Iterator i,const value_type& v) { 
    return this->insert_n(i._node,v); 
  }
  template<typename Iterator>
  Iterator insert(Iterator i,const_subtree<value_type> s) {
    return this->insert_n(i._node,s);
  }
  template<typename InputIterator>
  void insert(node_iter_base i,InputIterator f,InputIterator l) {
    this->insert_n(i._node,f,l);
  }
  void insert(node_iter_base i,size_type n,const value_type& v) {
    this->insert_n(i._node,repeat_it(v),repeat_it(v,n));
  }
  void insert(node_iter_base i,size_type n,const_subtree<value_type> s) {
    this->insert_n(i._node,repeat_it(s),repeat_it(s,n));
  }

  //append and prepend add children to root, which must be dereferenceable
  void append(const value_type& v) {
    this->insert_n(this->root_node()->sentinel(),v);
  }
  void append(const_subtree<value_type> s) {
    this->insert_n(this->root_node()->sentinel(),s);
  }
  template<typename InputIterator>
  void append(InputIterator f,InputIterator l) {
    this->insert_n(this->root_node()->sentinel(),f,l);    
  }
  void append(size_type n,const value_type& v) {
    this->insert_n(this->root_node()->sentinel(),repeat_it(v),repeat_it(v,n));
  }
  void append(size_type n,const_subtree<value_type> s) {
    this->insert_n(this->root_node()->sentinel(),repeat_it(s),repeat_it(s,n));
  }

  void prepend(const value_type& v) {
    this->insert_n(this->root_node()->first_child(),v);
  }
  void prepend(const_subtree<value_type> s) {
    this->insert_n(this->root_node()->first_child(),s);
  }
  template<typename InputIterator>
  void prepend(InputIterator f,InputIterator l) {
    this->insert_n(this->root_node()->first_child(),f,l);    
  }
  void prepend(size_type n,const value_type& v) {
    this->insert_n(this->root_node()->first_child(),
                   repeat_it(v),repeat_it(v,n));
  }
  void prepend(size_type n,const_subtree<value_type> s) {
    this->insert_n(this->root_node()->first_child(),
                   repeat_it(s),repeat_it(s,n));
  }

  //insert_above, and insert_below require dereferenceable iterators
  template<typename Iterator>
  Iterator insert_above(Iterator i,const value_type& v) {
    node_base* p=i._node;
    node_base* n=new node<T>(p->prev,p->next,v);

    n->tie_in(p->prev,p->next);
    n->set_first_child(p);

    return n;
  }

  template<typename Iterator>
  Iterator insert_below(Iterator i,const value_type& v) {
    node_base* n=new node<T>(i._node->end,v);
    i._node->set_first_child(n);
    return n;
  }

  //i's children are moved after i (becoming its siblings); i is returned
  template<typename Iterator>
  Iterator flatten(Iterator i) {
    node_base* n=i._node;
    
    if (node_base* end=n->end) {
      if (end->prev!=end) {
        n->next->prev=end->prev;
        end->prev->next=n->next;
        n->next=end->end;
        end->end->prev=n;
      }

      delete end;
      n->end=NULL;      
    }

    return i;
  }

  //erase return value is only sensible for child and post order iterators
  child_iterator erase(child_iterator i) { return erase_ret(i); }
  sub_child_iterator erase(sub_child_iterator i) { return erase_ret(i); }
  post_iterator erase(post_iterator i) { return erase_ret(i); }
  sub_post_iterator erase(sub_post_iterator i) { return erase_ret(i); }

  template<typename Iterator>
  void erase(Iterator i) { erase_n(i._node); }

  void erase(child_iterator f,child_iterator l) {
    if (f==l)
      return;

    node_base* n=f._node;
    node_base* final=l._node->prev;
    node_base* nprev=n->prev;
    n->left_cut(l._node);
    for (node_base* m=erase_descend(n);m!=final;) {
      if (m->dereferenceable()) {
        n=erase_descend(m->next);
        delete static_cast<node<T>*>(m);
      } else {
        n=m->next;
        delete m;
      }
      m=n;
    }
    delete static_cast<node<T>*>(final);
    l._node->prev=nprev;
  }

  //unfortunately the compiler needs these to resolve overloads properly :p
  typename super::const_pre_iterator begin() const { return super::begin(); }
  typename super::const_pre_iterator end() const { return super::end(); }
  typename super::const_child_iterator begin_child() const { 
    return super::begin_child();
  }
  typename super::const_child_iterator end_child() const { 
    return super::end_child();
  }
  typename super::const_sub_pre_iterator begin_sub() const { 
    return super::begin_sub();
  }
  typename super::const_sub_pre_iterator end_sub() const { 
    return this->super::end_sub();
  }
  typename super::const_sub_child_iterator begin_sub_child() const { 
    return this->super::begin_sub_child();
  }
  typename super::const_sub_child_iterator end_sub_child() const { 
    return this->super::end_sub_child();
  }
  const value_type& root() const { return super::root(); }
  const_subtree<T> root_sub() const { return super::root_sub(); } 
  
  pre_iterator begin() { return this->root_node(); }
  pre_iterator end() { return this->end_node(); }
  child_iterator begin_child() { return this->root_node()->first_child(); }
  child_iterator end_child() { return this->root_node()->sentinel(); }
  sub_pre_iterator begin_sub() { return this->root_node(); }
  sub_pre_iterator end_sub() { return this->end_node(); }
  sub_child_iterator begin_sub_child() { 
    return this->root_node()->first_child();
  }
  sub_child_iterator end_sub_child() { return this->root_node()->sentinel(); }
  post_iterator begin_post() { return make_post(this->root_node()); }
  post_iterator end_post() { return this->end_node(); }
  sub_post_iterator begin_sub_post() { return make_post(this->root_node()); }
  sub_post_iterator end_sub_post() { return this->end_node(); }

  value_type& root() { return *this->begin(); }
  subtree<T> root_sub() { return *this->begin_sub(); } 
  subtree<T> operator[](size_type idx) {
    return *boost::next(this->begin_sub_child(),idx);
  }
  value_type& front() { return *this->begin_child(); }
  value_type& back() { 
    return static_cast<node<T>*>(this->root_node()->end->prev)->data;
  }
  subtree<T> front_sub() { return *this->begin_sub_child(); }
  subtree<T> back_sub() { 
    return (TREE_TREE_NAMESPACE::_tree_private::node_base*)
        this->root_node()->end->prev;
  }

  void prune() { this->erase(begin_child(),end_child()); }

  template<typename Iterator,typename Subtree>
  void splice(Iterator i,Subtree s) {
    node_base* next=i._node;
    node_base* prev=next->prev;
    node_base* n=s._node;

    n->cut_out();
    n->tie_in(next->prev,next);
    n->prev=prev;
    n->next=next;
  }
  template<typename Iterator>
  void splice(Iterator i,tree<T>& tr) { splice(i,tr.root_sub()); }
  void splice(node_iter_base i,sub_child_iterator fi,sub_child_iterator li) {
    if (fi==li)
      return;

    node_base* n=i._node;
    node_base* f=fi._node;
    node_base* l=li._node->prev;

    assert(f->dereferenceable());
    assert(l->dereferenceable());

    f->left_cut(l->next);
    l->right_cut(f->prev);
    f->prev=n->prev;
    l->next=n;

    n->left_cut(f);
    n->prev=l;
  }
 protected:
  //functionality that gets delegated to the subclass
  node_base* root_node() { return static_cast<Tree*>(this)->root_node(); }
  node_base* end_node() { return static_cast<Tree*>(this)->end_node(); }

 private:
  node_base* create_n(node_base* prev,node_base* next,const value_type& v) {
    return new node<T>(prev,next,v);
  }
  node_base* create_n(node_base* prev,node_base* next,const_subtree<T> s) {
    node<T>* n=new node<T>(prev,next,s.root());
    if (!s.childless())
      this->insert_n(n->new_sentinel(),s.begin_sub_child(),s.end_sub_child());
    return n;
  }

  node_base* insert_n(node_base* next,const value_type& v) {
    node_base* prev=next->prev;
    node_base* n=new node<T>(prev,next,v);

    n->tie_in(prev,next);

    return n;
  }
  node_base* insert_n(node_base* next,const_subtree<T> s) {
    node_base* n=insert_n(next,s.root());
    if (!s.childless()) //only create a sentinel node if we must
      insert_n(n->new_sentinel(),s.begin_sub_child(),s.end_sub_child());
    return n;
  }
  template<typename Iterator>
  void insert_n(node_base* next,Iterator f,Iterator l) {
    if (f==l)
      return;

    node_base* prev=this->insert_n(next,*f++);
    while (f!=l)
      prev=prev->next=create_n(prev,next,*f++);
    next->prev=prev;
  }

  template<typename Iterator>
  Iterator erase_ret(Iterator i) {
    Iterator tmp=i;
    ++tmp;
    erase_n(i._node);
    return tmp;
  }
  
  node_base* erase_descend(node_base* n) {
    while (n->end!=NULL && n->dereferenceable())
      n=n->end->end;
    return n;
  }
  void erase_n(node_base* n) {
    assert(n->dereferenceable());

    for (node_base* m=erase_descend(n);m!=n;) {
      node_base* tmp=m->next;
      if (m->dereferenceable()) {
        tmp=erase_descend(tmp);
        delete static_cast<node<T>*>(m);
      } else {
        delete m;
      }
      m=tmp;
    }

    n->cut_out();
    delete static_cast<node<T>*>(n);
  }
};

template<typename T,typename Tree>
struct const_node_policy : public tr<T,Tree> {
  typedef T                  value_type;
  typedef const node_base*   node_base_pointer;
  typedef const node<T>*     node_pointer;
};
template<typename T,typename Tree>
struct mutable_node_policy : public mutable_tr<T,Tree> {
  typedef T                  value_type;
  typedef node_base* const   node_base_pointer;
  typedef node<T>* const     node_pointer;
};

template<typename NodePolicy>
struct subtr : public NodePolicy {
  typedef typename NodePolicy::value_type value_type;
  typedef typename NodePolicy::node_base_pointer node_base_pointer;
  typedef typename NodePolicy::node_pointer      node_pointer;

  subtr(node_base_pointer r) : _node(static_cast<node_pointer>(r)) {}

  bool empty() const { return false; }
 protected:
  template<typename,typename>
  friend struct tr;
  template<typename,typename>
  friend struct mutable_tr;

  node_pointer _node;

  const node_base* root_node() const { return _node; }
  const node_base* end_node() const { 
    const node_base* n=this->_node->next;
    ascend(n);
    return n;
  }
};

} //namespace _tree_private

// Note: a const_subtree encapsulates a mutable pointer to a constant subtree,
// whereas a subtree encaspulates a constant pointer to a mutable subtree (and
// acts as a reference). So assignment to a const_subtree makes it point at
// something else, whereas assignment to a subtree changes the underlying tree.

template<typename T>
struct const_subtree
    : public _tree_private::subtr<_tree_private::const_node_policy
                                 <T,const_subtree<T> > > {
  
  typedef _tree_private::subtr<_tree_private::const_node_policy
                              <T,const_subtree<T> > > super;

  //for convenience
  typedef typename super::const_pre_iterator pre_iterator;
  typedef typename super::const_sub_pre_iterator sub_pre_iterator;
  typedef typename super::const_child_iterator child_iterator;
  typedef typename super::const_sub_child_iterator sub_child_iterator;
  typedef typename super::const_post_iterator post_iterator;
  typedef typename super::const_sub_post_iterator sub_post_iterator;
  typedef pre_iterator iterator;

  template<typename OtherTr>
  const_subtree(const OtherTr& other) : super(other.root_node()) { 
    assert(!other.empty());
  }
  const_subtree(const _tree_private::node_base* n) : super(n) {}

  template<typename OtherTr>
  const_subtree& operator=(const OtherTr& rhs) {
    assert(!rhs.empty());
    this->_node=rhs.root_node();
  }
  
 protected:
  template<typename,typename>
  friend struct _tree_private::sub_iter_base;
};

template<typename T>
struct subtree : 
      public _tree_private::subtr<_tree_private::mutable_node_policy
                                  <T,subtree<T> > > {
  
  typedef _tree_private::subtr<_tree_private::mutable_node_policy
                              <T,subtree<T> > > super;

  template<typename OtherTr>
  subtree(OtherTr& other) : super(other.root_node()) {}

  template<typename OtherTr>
  subtree& operator=(const OtherTr& rhs) {
    assert(!rhs.empty());
    if (static_cast<const void*>(&rhs)!=static_cast<const void*>(this)) {
      this->root()=rhs.root();
      this->prune();
      this->append(rhs.begin_sub_child(),rhs.end_sub_child());
    }
    return *this;
  }

  subtree& operator=(subtree rhs) { return this->operator=<subtree>(rhs); }

  subtree& operator=(const T& t) { 
    this->prune();
    this->root()=t;
    return *this;
  }

  void swap(tree<T>& rhs) { swap(subtree(rhs)); }
  void swap(subtree rhs) {
    assert (!rhs.empty());
    if (this->root_node()!=rhs.root_node()) {
      std::swap(this->root(),rhs.root());
      typename super::sub_child_iterator tmp=this->begin_sub_child();
      this->splice(tmp,rhs.begin_sub_child(),rhs.end_sub_child());
      rhs.splice(rhs.begin_sub_child(),tmp,this->end_sub_child());
    }
  }
 protected:
  template<typename,typename>
  friend struct _tree_private::tr;
  template<typename,typename>
  friend struct _tree_private::mutable_tr;
  template<typename,typename>
  friend struct _tree_private::sub_iter_base;
  template<typename>
  friend struct const_subtree;

  typedef _tree_private::node_base node_base;

  subtree(node_base* n) : super(n) {}

  node_base* root_node() { return this->_node; }
  node_base* end_node() { 
    node_base* n=this->_node->next;
    ascend(n);
    return n;
  }
  const node_base* root_node() const { return super::root_node(); }
  const node_base* end_node() const { return super::end_node(); }
};

template<typename T>
struct tree : public _tree_private::mutable_tr<T,tree<T> > {
  typedef T value_type;

  tree() : _end() {}
  explicit tree(const T& t) : 
      _end(new _tree_private::node<T>(&_end,&_end,t)) { 
    _end.prev=_end.next; 
  }
  template<typename OtherTr>
  explicit tree(const OtherTr& other) : _end() { init(other); }
  tree(const tree& other) : _end() { init(other); }
  ~tree() { clear(); }

  tree& operator=(const tree& rhs) {
    if (&rhs!=this) {
      this->clear();
      if (!rhs.empty()) {
        this->insert(this->end(),rhs.root());
        this->append(rhs.begin_sub_child(),rhs.end_sub_child());
      }
    }
    return *this;
  }

  void swap(subtree<T> rhs) { rhs.swap(*this); }
  void swap(tree& rhs) { 
    if (this->empty()) {
      if (!rhs.empty()) {
        rhs._end.next->next=rhs._end.next->prev=&this->_end;
        this->_end.next=rhs._end.next;
        this->_end.prev=rhs._end.prev;
        rhs._end.next=rhs._end.prev=&rhs._end;
      }
    } else if (rhs.empty()) {
      this->_end.next->next=this->_end.next->prev=&rhs._end;
      rhs._end.next=this->_end.next;
      rhs._end.prev=this->_end.prev;
      this->_end.next=this->_end.prev=&this->_end;
    } else {
      std::swap(this->_end.next->next,rhs._end.next->next);
      std::swap(this->_end.next->prev,rhs._end.next->prev);
      std::swap(this->_end.prev,rhs._end.prev);
      std::swap(this->_end.next,rhs._end.next);
    }
  }

  bool empty() const { return this->_end.next==&this->_end; } 

  void clear() { 
    if (!this->empty()) 
      this->erase(this->begin()); 
  }

 protected:
  friend struct _tree_private::tr<T,tree<T> >;
  friend struct _tree_private::mutable_tr<T,tree<T> >;
  friend struct const_subtree<T>;
  friend struct subtree<T>;
  typedef _tree_private::node_base node_base;

  node_base _end;

  const node_base* root_node() const { return this->_end.next; }
  const node_base* end_node() const { return &this->_end; }
  node_base* root_node() { return this->_end.next; }
  node_base* end_node() { return &this->_end; }

  template<typename OtherTr>
  void init(const OtherTr& other) {
    if (!other.empty())
      insert(this->end(),other.root_sub());
  }
};

template<typename T>
struct tree_placeholder : public tree<T> {
  tree_placeholder(const T& t) : tree<T>(t) {}
  template<typename OtherTr>
  explicit tree_placeholder(const OtherTr& t) : tree<T>(t) {}

 protected:
  typedef const tree_placeholder& ctp;
  tree_placeholder& x(ctp c) { 
    this->prepend(const_subtree<T>(c));
    return *this; 
  }
 public:
  tree_placeholder& operator()(ctp c1) { return x(c1); }

  /**
    for higher arity n run this awk script, and paste the output below

    awk 'BEGIN { n=10; for (i=2;i<=n;++i) {
           printf("  tree_placeholder& operator()(");
           for (j=1;j<i;++j) printf("ctp c%i,",j);
           print "ctp c"i") {";
           printf("    this->operator()(");
           for (j=2;j<i;++j) printf("c%i,",j);
           print "c"i");";
           print "    return x(c1);\n  }"; } }'
  **/
  tree_placeholder& operator()(ctp c1,ctp c2) {
    this->operator()(c2);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3) {
    this->operator()(c2,c3);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4) {
    this->operator()(c2,c3,c4);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4,ctp c5) {
    this->operator()(c2,c3,c4,c5);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4,ctp c5,ctp c6) {
    this->operator()(c2,c3,c4,c5,c6);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4,
                               ctp c5,ctp c6,ctp c7) {
    this->operator()(c2,c3,c4,c5,c6,c7);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4,
                               ctp c5,ctp c6,ctp c7,ctp c8) {
    this->operator()(c2,c3,c4,c5,c6,c7,c8);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4,ctp c5,
                               ctp c6,ctp c7,ctp c8,ctp c9) {
    this->operator()(c2,c3,c4,c5,c6,c7,c8,c9);
    return x(c1);
  }
  tree_placeholder& operator()(ctp c1,ctp c2,ctp c3,ctp c4,ctp c5,
                               ctp c6,ctp c7,ctp c8,ctp c9,ctp c10) {
    this->operator()(c2,c3,c4,c5,c6,c7,c8,c9,c10);
    return x(c1);
  }
};

template<typename T>
tree_placeholder<T> tree_of(const T t) { return tree_placeholder<T>(t); }

template<typename Subtree>
struct child_adapter {
  typedef typename Subtree::child_iterator iterator;
  typedef typename Subtree::child_iterator const_iterator;
  child_adapter(Subtree t) : _t(t) {}
  iterator begin() const { return _t.begin_child(); }
  iterator end() const { return _t.end_child(); }
 protected:
  mutable Subtree _t;
};

template<typename Subtree>
struct sub_child_adapter {
  typedef typename Subtree::sub_child_iterator iterator;
  typedef typename Subtree::sub_child_iterator const_iterator;
  sub_child_adapter(Subtree t) : _t(t) {}
  iterator begin() const { return _t.begin_sub_child(); }
  iterator end() const { return _t.end_sub_child(); } 
 protected:
  mutable Subtree _t;
};

template<typename T>
child_adapter<subtree<T> > children(subtree<T> t) { 
  return child_adapter<subtree<T> >(t);
}
template<typename T>
sub_child_adapter<subtree<T> > sub_children(subtree<T> t) { 
  return sub_child_adapter<subtree<T> >(t);
}
template<typename T>
child_adapter<subtree<T> > children(tree<T>& t) { 
  return child_adapter<subtree<T> >(t);
}
template<typename T>
sub_child_adapter<subtree<T> > sub_children(tree<T>& t) { 
  return sub_child_adapter<subtree<T> >(t);
}

template<typename T>
child_adapter<const_subtree<T> > children(const_subtree<T> t) { 
  return child_adapter<const_subtree<T> >(t);
}
template<typename T>
sub_child_adapter<const_subtree<T> > sub_children(const_subtree<T> t) { 
  return sub_child_adapter<const_subtree<T> >(t);
}
template<typename T>
child_adapter<const_subtree<T> > children(const tree<T>& t) { 
  return child_adapter<const_subtree<T> >(t);
}
template<typename T>
sub_child_adapter<subtree<T> > sub_children(const tree<T>& t) { 
  return sub_child_adapter<const_subtree<T> >(t);
}

template<typename Subtree>
bool childless(Subtree t) { return t.childless(); }
template<typename Subtree>
typename Subtree::value_type root(Subtree t) { return t.root(); }

template<typename Subtree>
struct sub_leaf_adapter {
  typedef boost::filter_iterator<bool(*)(Subtree),
                                 typename Subtree::sub_post_iterator> iterator;
  typedef iterator const_iterator;

  sub_leaf_adapter(Subtree t) : _t(t) {}
  iterator begin() const { 
    return filter_it(_t.begin_sub_post(),_t.end_sub_post(),
                     &childless<Subtree>); 
  }
  iterator end() const { 
    return filter_it(_t.end_sub_post(),_t.end_sub_post(),
                     &childless<Subtree>);
  }
 protected:
  mutable Subtree _t;
};

template<typename Subtree>
struct leaf_adapter {
  typedef boost::transform_iterator<
    typename Subtree::value_type(*)(Subtree),
    typename sub_leaf_adapter<Subtree>::iterator> iterator;
  typedef iterator const_iterator;

  leaf_adapter(Subtree t) : _t(t) {}
  iterator begin() const { 
    return transform_it(filter_it(_t.begin_sub_post(),_t.end_sub_post(),
                                  &childless<Subtree>),&root<Subtree>); 
  }
  iterator end() const { 
    return transform_it(filter_it(_t.end_sub_post(),_t.end_sub_post(),
                                  &childless<Subtree>),&root<Subtree>);
  }
 protected:
  mutable Subtree _t;
};

template<typename T>
leaf_adapter<subtree<T> > leaves(subtree<T> t) { 
  return leaf_adapter<subtree<T> >(t);
}
template<typename T>
sub_leaf_adapter<subtree<T> > sub_leaves(subtree<T> t) { 
  return sub_leaf_adapter<subtree<T> >(t);
}
template<typename T>
leaf_adapter<subtree<T> > leaves(tree<T>& t) { 
  return leaf_adapter<subtree<T> >(t);
}
template<typename T>
sub_leaf_adapter<subtree<T> > sub_leaves(tree<T>& t) { 
  return sub_leaf_adapter<subtree<T> >(t);
}

template<typename T>
leaf_adapter<const_subtree<T> > leaves(const_subtree<T> t) { 
  return leaf_adapter<const_subtree<T> >(t);
}
template<typename T>
sub_leaf_adapter<const_subtree<T> > sub_leaves(const_subtree<T> t) { 
  return sub_leaf_adapter<const_subtree<T> >(t);
}
template<typename T>
leaf_adapter<const_subtree<T> > leaves(const tree<T>& t) { 
  return leaf_adapter<const_subtree<T> >(t);
}
template<typename T>
sub_leaf_adapter<subtree<T> > sub_leaves(const tree<T>& t) { 
  return sub_leaf_adapter<const_subtree<T> >(t);
}

} //namespace TREE_TREE_NAMESPACE

namespace std {
template<typename T>
void swap(TREE_TREE_NAMESPACE::subtree<T> l,
          TREE_TREE_NAMESPACE::subtree<T> r) { l.swap(r); }
template<typename T>
void swap(TREE_TREE_NAMESPACE::subtree<T> l,
          TREE_TREE_NAMESPACE::tree<T>& r)   { l.swap(r); }
template<typename T>
void swap(TREE_TREE_NAMESPACE::tree<T>& l,
          TREE_TREE_NAMESPACE::subtree<T> r) { l.swap(r); }
template<typename T>
void swap(TREE_TREE_NAMESPACE::tree<T>& l,
          TREE_TREE_NAMESPACE::tree<T>& r)   { l.swap(r); }
} //namespace std
#endif //_TREE_TREE_HPP_
