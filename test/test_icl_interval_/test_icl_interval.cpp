/*-----------------------------------------------------------------------------+    
Copyright (c) 2008-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#define BOOST_TEST_MODULE icl::interval unit test
#include <disable_test_warnings.hpp>
#include <string>
#include <boost/mpl/list.hpp>
#include "../unit_test_unwarned.hpp"


// interval instance types
#include "../test_type_lists.hpp"
#include "../test_value_maker.hpp"
#include "../test_interval_laws.hpp"

using namespace std;
using namespace boost;
using namespace unit_test;
using namespace boost::icl;

#include "../test_icl_interval_shared.hpp"
#include "../test_icl_interval.hpp"
#include "../test_icl_dynamic_interval.hpp"
#include "../test_icl_discrete_interval.hpp"
#include "../test_icl_continuous_interval.hpp"
#include "../test_icl_static_interval.hpp"

#include <boost/icl/right_open_interval.hpp>
#include <boost/icl/left_open_interval.hpp>
#include <boost/icl/closed_interval.hpp>
#include <boost/icl/open_interval.hpp>

#include <boost/icl/discrete_interval.hpp>
#include <boost/icl/continuous_interval.hpp>
#include <boost/icl/interval.hpp>

//- sta.asy.{dis|con} ----------------------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_right_open_interval_ctor_4_ordered_types, T, ordered_types)
{                   interval_ctor_4_ordered_types<right_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_right_open_interval_4_ordered_types, T, discrete_types)
{      singelizable_interval_4_ordered_types<right_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_right_open_interval_4_bicremental_types, T, discrete_types)
{      singelizable_interval_4_bicremental_types<right_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_left_open_interval_ctor_4_ordered_types, T, ordered_types)
{                  interval_ctor_4_ordered_types<left_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_left_open_interval_4_ordered_types_singelizable, T, signed_discrete_types)
{     singelizable_interval_4_ordered_types<left_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_left_open_interval_4_bicremental_types, T, discrete_types)
{     singelizable_interval_4_bicremental_types<left_open_interval<T> >(); }

//------------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_distant_intervals_4_discrete_types, T, discrete_types)
{         distant_intervals_4_discrete_types<T, std::less>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(test_itl_distant_intervals_4_numeric_continuous_types, T, numeric_continuous_types)
{         distant_intervals_4_numeric_continuous_types<T, std::less>(); }


//- sta.asy.{dis|con} ----------------------------------------------------------
//- n tests for right_open_inverval --------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_right_open_interval_ctor_4_ordered_types, T, ordered_types)
{                       interval_ctor_4_ordered_types<right_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_right_open_interval_4_ordered_types, T, discrete_types)
{          singelizable_interval_4_ordered_types<right_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_right_open_interval_4_bicremental_types, T, discrete_types)
{         singelizable_interval_4_bicremental_types<right_open_interval<T> >(); }

//- n tests for left_open_inverval ---------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_left_open_interval_ctor_4_ordered_types, T, ordered_types)
{                     interval_ctor_4_ordered_types<left_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_left_open_interval_4_ordered_types_singelizable, T, signed_discrete_types)
{        singelizable_interval_4_ordered_types<left_open_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_left_open_interval_4_bicremental_types, T, discrete_types)
{        singelizable_interval_4_bicremental_types<left_open_interval<T> >(); }

// first()/last() must be symmetric guards over the open boundary.
// last() asserts numeric_minimum (domain_prior can't underflow); first() must
// likewise assert numeric_maximum (domain_next can't overflow). This checks the
// guards pass on valid intervals and that both first() overloads still return
// the correct boundary element. See https://github.com/boostorg/icl/issues/58
BOOST_AUTO_TEST_CASE(test_itl_interval_first_last_symmetry)
{
    // Static interval types -> first()/last() overloads gated on is_static_*.
    BOOST_CHECK_EQUAL(icl::first(right_open_interval<int>(2,7)), 2); // [2,7)
    BOOST_CHECK_EQUAL(icl::first(left_open_interval <int>(2,7)), 3); // (2,7]
    BOOST_CHECK_EQUAL(icl::first(open_interval      <int>(2,7)), 3); // (2,7)
    BOOST_CHECK_EQUAL(icl::first(closed_interval    <int>(2,7)), 2); // [2,7]

    BOOST_CHECK_EQUAL(icl::last (right_open_interval<int>(2,7)), 6); // [2,7)
    BOOST_CHECK_EQUAL(icl::last (left_open_interval <int>(2,7)), 7); // (2,7]
    BOOST_CHECK_EQUAL(icl::last (open_interval      <int>(2,7)), 6); // (2,7)
    BOOST_CHECK_EQUAL(icl::last (closed_interval    <int>(2,7)), 7); // [2,7]

    // Dynamic bounds -> the is_discrete_interval first()/last() overloads,
    // exercising both branches of the is_left_closed/is_right_closed guards.
    BOOST_CHECK_EQUAL(icl::first(interval<int>::right_open(2,7)), 2);
    BOOST_CHECK_EQUAL(icl::first(interval<int>::left_open (2,7)), 3);
    BOOST_CHECK_EQUAL(icl::first(interval<int>::open      (2,7)), 3);
    BOOST_CHECK_EQUAL(icl::first(interval<int>::closed    (2,7)), 2);

    BOOST_CHECK_EQUAL(icl::last (interval<int>::right_open(2,7)), 6);
    BOOST_CHECK_EQUAL(icl::last (interval<int>::left_open (2,7)), 7);
    BOOST_CHECK_EQUAL(icl::last (interval<int>::open      (2,7)), 6);
    BOOST_CHECK_EQUAL(icl::last (interval<int>::closed    (2,7)), 7);
}

//- dyn.dis --------------------------------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_discrete_interval_ctor_4_discrete_types_base, T, discrete_types)
{                     interval_ctor_4_ordered_types<discrete_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_discrete_interval_ctor_4_discrete_types_dynamic, T, discrete_types)
{             dynamic_interval_ctor_4_ordered_types<discrete_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_discrete_interval_4_ordered_types, T, discrete_types)
{        singelizable_interval_4_ordered_types<discrete_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_discrete_interval_4_bicremental_types, T, discrete_types)
{        singelizable_interval_4_bicremental_types<discrete_interval<T> >(); }

//- dyn.con --------------------------------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_continuous_interval_ctor_4_continuous_types_base, T, continuous_types)
{                       interval_ctor_4_ordered_types<continuous_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_continuous_interval_ctor_4_continuous_types_dynamic, T, continuous_types)
{               dynamic_interval_ctor_4_ordered_types<continuous_interval<T> >(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_continuous_interval_4_continuous_types_singelizable, T, continuous_types)
{          singelizable_interval_4_ordered_types<continuous_interval<T> >(); }

//------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_distant_intervals_4_discrete_types, T, discrete_types)
{            distant_intervals_4_discrete_types<T, std::less>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_distant_intervals_4_numeric_continuous_types, T, numeric_continuous_types)
{            distant_intervals_4_numeric_continuous_types<T, std::less>(); }

//------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_itl_dynamic_interval_bounds_4_bicremental_types, T, bicremental_types)
{            dynamic_interval_bounds_4_bicremental_types<T>(); }

//==============================================================================
//==============================================================================
BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_equal_4_integral_types, T, integral_types)
{            interval_equal_4_integral_types<T>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_less_4_integral_types, T, integral_types)
{            interval_less_4_integral_types<T>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_touches_4_bicremental_types, T, bicremental_types)
{            interval_touches_4_bicremental_types<T>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_touches_4_integral_types, T, integral_types)
{            interval_touches_4_integral_types<T>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_subtract_4_bicremental_types, T, bicremental_types)
{            interval_subtract_4_bicremental_types<T>(); }

#ifndef BOOST_ICL_USE_STATIC_BOUNDED_INTERVALS

BOOST_AUTO_TEST_CASE
(fastest_icl_interval_ctor_specific)
{            interval_ctor_specific(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_equal_4_bicremental_continuous_types, T, bicremental_continuous_types)
{            interval_equal_4_bicremental_continuous_types<T>(); }

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_infix_intersect_4_bicremental_types, T, bicremental_types)
{            interval_infix_intersect_4_bicremental_types<T>(); }

#else

BOOST_AUTO_TEST_CASE_TEMPLATE
(fastest_icl_interval_infix_intersect_4_bicremental_types, T, discrete_types)
{            interval_infix_intersect_4_bicremental_types<T>(); }

#endif // ndef BOOST_ICL_USE_STATIC_BOUNDED_INTERVALS



