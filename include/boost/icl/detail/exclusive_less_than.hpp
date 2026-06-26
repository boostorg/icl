/*-----------------------------------------------------------------------------+
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_DETAIL_EXCLUSIVE_LESS_THAN_HPP_JOFA_100929
#define BOOST_ICL_DETAIL_EXCLUSIVE_LESS_THAN_HPP_JOFA_100929

#include <boost/icl/concept/interval.hpp>

namespace boost{ namespace icl
{

/// Comparison functor on intervals implementing an overlap free less 
template <class IntervalT>
struct exclusive_less_than 
{
    /// Enables heterogeneous lookup on a domain point (C++14 std::set / Boost.Container).
    typedef void is_transparent;

    typedef typename interval_traits<IntervalT>::domain_type domain_type;


    /** Strict partial ordering on intervals (unchanged). */
    bool operator()(const IntervalT& left, const IntervalT& right)const
    {
        return icl::non_empty::exclusive_less(left, right);
    }


    /** interval vs point: left lies exclusively to the left of point p. */
    bool operator()(const IntervalT& left, const domain_type& p)const
    {
        return icl::domain_less<IntervalT>(icl::upper(left), p)
            || ( !icl::domain_less<IntervalT>(p, icl::upper(left))    // upper(left) == p
                 && !icl::is_right_closed(icl::bounds(left)) );        // ... and right border open
    }

    /** point vs interval: point p lies exclusively to the left of right. */
    bool operator()(const domain_type& p, const IntervalT& right)const
    {
        return icl::domain_less<IntervalT>(p, icl::lower(right))
            || ( !icl::domain_less<IntervalT>(icl::lower(right), p)   // p == lower(right)
                 && !icl::is_left_closed(icl::bounds(right)) );        // ... and left border open
    }

};

}} // namespace boost icl

#endif


