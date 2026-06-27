/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Boost.ICL SWO fix (centralized)
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_DETAIL_INTERVAL_LOOKUP_HPP_JOFA_260610
#define BOOST_ICL_DETAIL_INTERVAL_LOOKUP_HPP_JOFA_260610

/*-----------------------------------------------------------------------------+
| THE SINGLE HOME FOR THE STRICT-WEAK-ORDERING FIX.                            |
|                                                                              |
| The underlying std::set/std::map is ALWAYS pairwise-disjoint. On disjoint    |
| intervals exclusive_less_than is a strict total order, so every comparison   |
| the container performs between STORED elements is a valid SWO: erase,        |
| iteration, rebalancing, etc. are never at risk.                             |
|                                                                              |
| The ordering is violated only by an *argument* that overlaps stored          |
| elements: such an argument is transiently equivalent to several ordered      |
| stored elements, which breaks transitivity of equivalence. Where that        |
| actually bites depends on the operation:                                     |
|                                                                              |
|   * lower_bound / upper_bound / equal_range -- a tree descent guided by the  |
|     overlapping argument can return a wrong (but not rejected) iterator, and |
|     ICL then operates on the wrong range. libc++ >= 22 (PR #155245) uses a   |
|     single-descent search that assumes a SWO, so this is the real, observable|
|     SWO bug. The accessors fix it by never handing the tree an overlapping   |
|     interval: anchor every lookup on a single point, whose equivalence class |
|     over disjoint intervals has size <= 1 (a genuine SWO), then apply at most|
|     one O(1) correction for an open-bound "kiss".                            |
|                                                                              |
|   * the add insert (un-hinted AND hinted) -- formally violates the SWO       |
|     precondition too, but is benign on every standard library tested. A      |
|     unique-key insert does a single root-to-leaf descent that stops on the   |
|     first equivalent node, and because the intervals overlapping the argument|
|     form a contiguous in-order run, that descent always lands on the run and |
|     rejects the insert {pos,false} -- exactly the collision signal ICL's     |
|     add() relies on. The hinted overload behaves the same: for an overlapping|
|     argument the O(1)-hint neighbour check fails and the library falls back  |
|     to the full search, which also rejects. Routing the add inserts through  |
|     interval_lookup::insert only removes the formal UB; it does not fix an   |
|     observed failure. (Residual/gap inserts whose arguments are             |
|     disjoint-by-construction stay raw -- they are never overlapping.)        |
|                                                                              |
| PERFORMANCE: for discrete / dynamic-bound intervals the anchor is a CLOSED   |
| SINGLETON [v,v] -- a key_type value -- so the query takes the HOMOGENEOUS    |
| lower_bound/upper_bound overload (libc++ 22's fast descent). Only            |
| static-bounds continuous intervals (no singleton()) need a heterogeneous     |
| point query (C++14) and fall back to a raw call in C++11.                    |
|                                                                              |
| This helper is the ONLY place that logic lives. interval_base_set and        |
| interval_base_map call it with a key-extraction functor (identity for a set, |
| ".first" for a map), so the set and map sides share one implementation       |
| instead of carrying two copies; both base classes point back here for the    |
| full rationale instead of repeating it.                                      |
+-----------------------------------------------------------------------------*/

#include <utility>
#include <boost/config.hpp>                    // BOOST_CXX_VERSION
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/or.hpp>

#include <boost/icl/type_traits/is_discrete.hpp>
#include <boost/icl/type_traits/is_continuous.hpp>
#include <boost/icl/type_traits/is_numeric.hpp>
#include <boost/icl/type_traits/is_interval.hpp>          // has_static_bounds / has_dynamic_bounds
#include <boost/icl/interval_traits.hpp>
#include <boost/icl/concept/interval.hpp>                 // icl::lower/upper/is_empty/singleton

namespace boost{ namespace icl{ namespace detail
{

// value_type -> const key_type& extractors.
struct identity_key
{
    template<class T> const T& operator()(const T& x) const { return x; }
};
struct pair_first_key
{
    template<class Pair>
    auto operator()(const Pair& x) const -> decltype((x.first)) { return x.first; }
};

struct interval_lookup
{
    // Two stored/queried intervals overlap iff icl::intersects holds. The call
    // sites below always pass two non-empty intervals, for which intersects is
    // exactly the comparator equivalence !comp(a,b) && !comp(b,a) over
    // exclusive_less_than -- so we use the canonical helper and stay in sync
    // with any future change to ICL's overlap/touch semantics.

    //=========================================================================
    //= lower_bound
    //=========================================================================

    // discrete OR dynamic-bound: HOMOGENEOUS singleton anchor (fast path).
    template<class Cont, class It, class Kov,
             class KeyT = typename Cont::key_type,
             typename enable_if<mpl::or_<
                 has_dynamic_bounds<KeyT>,
                 is_discrete<typename interval_traits<KeyT>::domain_type> >, int>::type = 0>
    static It lower_bound(Cont& c, const KeyT& interval, Kov kov)
    {
        typedef typename interval_traits<KeyT>::domain_type    domain_type;
        typedef typename interval_traits<KeyT>::domain_compare domain_compare;
        if(icl::is_empty(interval)) return c.end();
        const typename Cont::key_compare comp = typename Cont::key_compare();
        It it_;
        if(numeric_minimum<domain_type, domain_compare, is_numeric<domain_type>::value>
               ::is_less_than(icl::lower(interval)))
            it_ = c.lower_bound(icl::singleton<KeyT>(icl::lower(interval)));   // homogeneous
        else
            it_ = c.begin();
        if(it_ != c.end() && !icl::intersects(kov(*it_), interval) && !comp(interval, kov(*it_)))
            ++it_;                                                             // one fwd correction
        return it_;
    }

    // static-bounds continuous: HETEROGENEOUS point anchor (C++14), raw in C++11.
    template<class Cont, class It, class Kov,
             class KeyT = typename Cont::key_type,
             typename enable_if<mpl::and_<
                 has_static_bounds<KeyT>,
                 is_continuous<typename interval_traits<KeyT>::domain_type> >, int>::type = 0>
    static It lower_bound(Cont& c, const KeyT& interval, Kov kov)
    {
#       if (BOOST_CXX_VERSION >= 201402L)
        if(icl::is_empty(interval)) return c.end();
        const typename Cont::key_compare comp = typename Cont::key_compare();
        It it_ = c.lower_bound(icl::lower(interval));                          // heterogeneous point
        if(it_ != c.end() && !icl::intersects(kov(*it_), interval) && !comp(interval, kov(*it_)))
            ++it_;
        return it_;
#       else
        // C++11 LIMITATION (static-bounds continuous intervals only):
        // there is no heterogeneous point query available here, so we fall
        // back to handing the tree the full OVERLAPPING interval. This is the
        // exact SWO violation this header otherwise avoids: under a
        // single-descent search (libc++ >= 22) the comparator is not a strict
        // weak order for an overlapping argument, so this call CAN RETURN A
        // WRONG ITERATOR and silently produce incorrect query results.
        // Build with C++14 or newer to get the point-anchored, SWO-safe path.
        (void)kov;
        return c.lower_bound(interval);
#       endif
    }

    //=========================================================================
    //= upper_bound
    //=========================================================================

    // discrete OR dynamic-bound: HOMOGENEOUS singleton anchor (fast path).
    template<class Cont, class It, class Kov,
             class KeyT = typename Cont::key_type,
             typename enable_if<mpl::or_<
                 has_dynamic_bounds<KeyT>,
                 is_discrete<typename interval_traits<KeyT>::domain_type> >, int>::type = 0>
    static It upper_bound(Cont& c, const KeyT& interval, Kov kov)
    {
        typedef typename interval_traits<KeyT>::domain_type    domain_type;
        typedef typename interval_traits<KeyT>::domain_compare domain_compare;
        if(icl::is_empty(interval)) return c.end();
        const typename Cont::key_compare comp = typename Cont::key_compare();
        It it_;
        if(numeric_maximum<domain_type, domain_compare, is_numeric<domain_type>::value>
               ::is_greater_than(icl::upper(interval)))
            it_ = c.upper_bound(icl::singleton<KeyT>(icl::upper(interval)));   // homogeneous
        else
            it_ = c.end();
        if(it_ != c.begin())
        {
            It prev_ = it_; --prev_;
            if(!comp(kov(*prev_), icl::upper(interval)) && !icl::intersects(kov(*prev_), interval))
                it_ = prev_;                                                   // one back correction
        }
        return it_;
    }

    // static-bounds continuous: HETEROGENEOUS point anchor (C++14), raw in C++11.
    template<class Cont, class It, class Kov,
             class KeyT = typename Cont::key_type,
             typename enable_if<mpl::and_<
                 has_static_bounds<KeyT>,
                 is_continuous<typename interval_traits<KeyT>::domain_type> >, int>::type = 0>
    static It upper_bound(Cont& c, const KeyT& interval, Kov kov)
    {
#       if (BOOST_CXX_VERSION >= 201402L)
        if(icl::is_empty(interval)) return c.end();
        const typename Cont::key_compare comp = typename Cont::key_compare();
        const typename interval_traits<KeyT>::domain_type anchor = icl::upper(interval);
        It it_ = c.upper_bound(anchor);                                        // heterogeneous point
        if(it_ != c.begin())
        {
            It prev_ = it_; --prev_;
            if(!comp(kov(*prev_), anchor) && !icl::intersects(kov(*prev_), interval))
                it_ = prev_;
        }
        return it_;
#       else
        // C++11 LIMITATION (static-bounds continuous intervals only):
        // same as the lower_bound fallback above -- without a heterogeneous
        // point query we hand the tree the full OVERLAPPING interval, which
        // violates the strict weak order under a single-descent search
        // (libc++ >= 22). This call CAN RETURN A WRONG ITERATOR and silently
        // produce incorrect query results. Build with C++14 or newer to get
        // the point-anchored, SWO-safe path.
        (void)kov;
        return c.upper_bound(interval);
#       endif
    }

    //=========================================================================
    //= disjoint-only insertion hardening (removes the formal insert-side UB)
    //=========================================================================

    template<class Cont, class It, class Value, class Kov>
    static std::pair<It,bool> insert(Cont& c, const Value& v, Kov kov)
    {
        const typename Cont::key_compare comp = typename Cont::key_compare();
        It lb_ = lower_bound<Cont, It>(c, kov(v), kov);
        if(lb_ == c.end() || comp(kov(v), kov(*lb_)))
            return std::pair<It,bool>(c.insert(lb_, v), true);
        return std::pair<It,bool>(lb_, false);
    }

    template<class Cont, class It, class Value, class Kov>
    static std::pair<It,bool> insert(Cont& c, It hint_, const Value& v, Kov kov)
    {
        const typename Cont::key_compare comp = typename Cont::key_compare();
        bool good_ = (hint_ == c.end() || comp(kov(v), kov(*hint_)));
        if(good_ && hint_ != c.begin())
            { It p_ = hint_; --p_; good_ = comp(kov(*p_), kov(v)); }
        if(good_)
            return std::pair<It,bool>(c.insert(hint_, v), true);
        return insert<Cont, It>(c, v, kov);
    }
};

}}} // namespace detail icl boost

#endif
