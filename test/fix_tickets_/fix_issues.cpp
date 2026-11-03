/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#define BOOST_TEST_MODULE icl::issues unit test

#include <disable_test_warnings.hpp>
#include "../unit_test_unwarned.hpp"

#include <boost/icl/interval_set.hpp>
#include <boost/icl/right_open_interval.hpp>
#include <boost/icl/left_open_interval.hpp>
#include <boost/icl/closed_interval.hpp>
#include <boost/icl/open_interval.hpp>
#include <boost/cstdint.hpp>
#include <limits>


//CL #include <iostream>

using namespace std;
using namespace boost;
using namespace unit_test;
using namespace boost::icl;

using right_open_interval_set = interval_set<int, std::less, right_open_interval<int>>; 
using const_interval_iterator = right_open_interval_set::const_iterator;

template<template<class> class Compare, class Interval>
void check_interval_size(const Interval& span, std::size_t expected)
{
    typedef typename interval_traits<Interval>::domain_type Domain;
    interval_set<Domain, Compare, Interval> values;
    values.add(span);
    BOOST_CHECK_EQUAL(cardinality(span), expected);
    BOOST_CHECK_EQUAL(cardinality(values), expected);
    BOOST_CHECK_EQUAL(length(span), expected);
    BOOST_CHECK_EQUAL(length(values), expected);
}

template<class Domain>
void check_cardinality_at_limits()
{
    typedef discrete_interval<Domain> Interval;
    const Domain high = (std::numeric_limits<Domain>::max)();
    const Domain low = (std::numeric_limits<Domain>::min)();

    check_interval_size<std::less>(Interval::closed(high, high), 1);
    check_interval_size<std::less>(Interval::closed(high - 3, high), 4);
    check_interval_size<std::less>(Interval::left_open(high - 3, high), 3);
    check_interval_size<std::less>(Interval::right_open(high - 3, high), 3);
    check_interval_size<std::less>(Interval::open(high - 3, high), 2);
    check_interval_size<std::less>(Interval::right_open(high, high), 0);

    check_interval_size<std::less>(closed_interval<Domain>(high - 3, high), 4);
    check_interval_size<std::less>(left_open_interval<Domain>(high - 3, high), 3);
    check_interval_size<std::less>(right_open_interval<Domain>(high - 3, high), 3);
    check_interval_size<std::less>(open_interval<Domain>(high - 3, high), 2);

    typedef discrete_interval<Domain, std::greater> Reverse;
    check_interval_size<std::greater>(Reverse::closed(low, low), 1);
    check_interval_size<std::greater>(Reverse::closed(low + 3, low), 4);
    check_interval_size<std::greater>(Reverse::open(low + 3, low), 2);
    check_interval_size<std::greater>(closed_interval<Domain, std::greater>(low + 3, low), 4);
    check_interval_size<std::greater>(left_open_interval<Domain, std::greater>(low + 3, low), 3);
    check_interval_size<std::greater>(right_open_interval<Domain, std::greater>(low + 3, low), 3);
    check_interval_size<std::greater>(open_interval<Domain, std::greater>(low + 3, low), 2);
}

// 
// https://github.com/boostorg/icl/issues/41
// https://github.com/boostorg/icl/issues/63
BOOST_AUTO_TEST_CASE(issue_41_cardinality_at_domain_limits_plus_decrementing_domains)
{
    check_cardinality_at_limits<boost::uint8_t>();
    check_cardinality_at_limits<boost::int8_t>();
    check_cardinality_at_limits<boost::uint32_t>();
    check_cardinality_at_limits<boost::int32_t>();
    check_cardinality_at_limits<boost::uint64_t>();
    check_cardinality_at_limits<boost::int64_t>();

    // The size type can represent more values than the signed domain difference.
    if (std::numeric_limits<std::size_t>::digits > 32)
    {
        typedef discrete_interval<boost::int32_t> Interval;
        BOOST_CHECK_EQUAL(cardinality(Interval::closed(
            (std::numeric_limits<boost::int32_t>::min)(),
            (std::numeric_limits<boost::int32_t>::max)())), boost::uint64_t(1) << 32);
    }
    if (std::numeric_limits<std::size_t>::digits >= 64)
    {
        typedef discrete_interval<boost::int64_t> Interval;
        BOOST_CHECK_EQUAL(cardinality(Interval::closed(
            (std::numeric_limits<boost::int64_t>::min)() + 1,
            (std::numeric_limits<boost::int64_t>::max)())),
            (std::numeric_limits<boost::uint64_t>::max)());
    }
}

// 
// https://github.com/boostorg/icl/issues/51
BOOST_AUTO_TEST_CASE(issue_51_exclusive_less_not_std_compliant)
{
    right_open_interval_set itvset;

    itvset.add(right_open_interval<int>(0, 2));
    itvset.add(right_open_interval<int>(3, 5));
    itvset.add(right_open_interval<int>(6, 9));
    itvset.add(right_open_interval<int>(10, 12));

    auto ro_itv_4_7 = right_open_interval<int>(4, 7);
    auto lwb = itvset.lower_bound(right_open_interval<int>(4, 7));
    auto upb = itvset.upper_bound(ro_itv_4_7);
    BOOST_CHECK_EQUAL(*lwb, right_open_interval<int>(3, 5));
    BOOST_CHECK_EQUAL(*upb, right_open_interval<int>(10, 12));

    auto collision = itvset.equal_range(ro_itv_4_7);
    // equal_range returns the range of intervals in itvset that collide with interval [4,7)
    BOOST_CHECK_EQUAL(*collision.first,  *lwb);
    BOOST_CHECK_EQUAL(*collision.second, *upb);

    const_interval_iterator cit = lwb;

    BOOST_CHECK_EQUAL(*cit, right_open_interval<int>(3, 5));
    BOOST_CHECK(intersects(*cit, ro_itv_4_7));
    auto intersect1 = *cit & ro_itv_4_7;
    BOOST_CHECK_EQUAL(4, last(intersect1));

    ++cit;
    BOOST_CHECK_EQUAL(*cit, right_open_interval<int>(6, 9));
    auto intersect2 = *cit & ro_itv_4_7;
    BOOST_CHECK_EQUAL(6, last(intersect2));

    ++cit;
    auto ro_itv_10_12 = right_open_interval<int>(10, 12);
    BOOST_CHECK_EQUAL(*cit, ro_itv_10_12);
    BOOST_CHECK_EQUAL(*upb, ro_itv_10_12);
    BOOST_CHECK( exclusive_less(ro_itv_4_7, ro_itv_10_12) );
    BOOST_CHECK( disjoint(ro_itv_4_7, ro_itv_10_12) );
}
