/*-----------------------------------------------------------------------------+
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_TYPE_TRAITS_IS_NUMERIC_HPP_JOFA_100322
#define BOOST_ICL_TYPE_TRAITS_IS_NUMERIC_HPP_JOFA_100322

#include <limits>
#include <complex>
#include <functional>
#include <boost/type_traits/is_floating_point.hpp>
#include <boost/type_traits/is_integral.hpp>

namespace boost{ namespace icl
{

template <class Type> struct is_fixed_numeric
{
    typedef is_fixed_numeric type;
    BOOST_STATIC_CONSTANT(bool, value = (0 < std::numeric_limits<Type>::digits));
};

template <class Type> struct is_std_numeric
{
    typedef is_std_numeric type;
    BOOST_STATIC_CONSTANT(bool, 
        value = (std::numeric_limits<Type>::is_specialized));
};

template <class Type> struct is_std_integral
{
    typedef is_std_integral type;
    BOOST_STATIC_CONSTANT(bool, 
        value = (std::numeric_limits<Type>::is_integer));
};

template <class Type> struct is_numeric
{
    typedef is_numeric type;
    BOOST_STATIC_CONSTANT(bool, value = 
        (mpl::or_< is_std_numeric<Type>
                 , boost::is_integral<Type> 
                 , is_std_integral<Type> >::value) );
};

template <class Type> 
struct is_numeric<std::complex<Type> >
{
    typedef is_numeric type;
    BOOST_STATIC_CONSTANT(bool, value = true);
};

//--------------------------------------------------------------------------
// The lowest and highest representable values of a numeric domain. Used by
// the bound-guards below to detect under-/overflow of domain_prior/domain_next.
//
// Note that for non-integral types std::numeric_limits<T>::min() is the smallest
// *positive* normalized value, NOT the most negative one. The most negative
// value is lowest(). For integral types lowest()==min(), so lowest()
// is the correct universal choice for the lower extreme.
//
// Types for which is_numeric is true but std::numeric_limits is not specialized
// (e.g. boost::rational) have no usable bounds, so the guards impose no
// constraint (return true) rather than comparing against a bogus lowest()/max()
// that silently default-constructs to T(). If boost::rational ever gets a
// specialization it will be used automatically.
//--------------------------------------------------------------------------
namespace detail
{
    // Evaluate the bound comparison compare(left, right), but only when
    // std::numeric_limits<Type> is specialized. Otherwise the domain has no
    // usable bounds and the guard imposes no constraint by returning true.
    // Centralizes the is_specialized check shared by all four guards below.
    template<class Type, class Compare>
    inline bool numeric_bound_check(Type left, Type right, Compare compare)
    {
        return std::numeric_limits<Type>::is_specialized
             ? compare(left, right)
             : true;
    }
} // namespace detail

// checks if a value is at/below the lowest representable bound
template<class Type, class Compare, bool Enable = false>
struct numeric_minimum
{
    static bool is_less_than(Type){ return true; }
    static bool is_less_than_or(Type, bool){ return true; }
};

template<class Type>
struct numeric_minimum<Type, std::less<Type>, true>
{
    static bool is_less_than(Type value)
    { return detail::numeric_bound_check(std::numeric_limits<Type>::lowest(), value, std::less<Type>()); }

    static bool is_less_than_or(Type value, bool cond)
    { return cond || is_less_than(value); }
};

template<class Type>
struct numeric_minimum<Type, std::greater<Type>, true>
{
    static bool is_less_than(Type value)
    { return detail::numeric_bound_check((std::numeric_limits<Type>::max)(), value, std::greater<Type>()); }

    static bool is_less_than_or(Type value, bool cond)
    { return cond || is_less_than(value); }
};

// checks if a value is at/above the highest representable bound
template<class Type, class Compare, bool Enable = false>
struct numeric_maximum
{
    static bool is_greater_than(Type){ return true; }
    static bool is_greater_than_or(Type, bool){ return true; }
};

template<class Type>
struct numeric_maximum<Type, std::less<Type>, true>
{
    static bool is_greater_than(Type value)
    { return detail::numeric_bound_check(value, (std::numeric_limits<Type>::max)(), std::less<Type>()); }

    static bool is_greater_than_or(Type value, bool cond)
    { return cond || is_greater_than(value); }
};

template<class Type>
struct numeric_maximum<Type, std::greater<Type>, true>
{
    static bool is_greater_than(Type value)
    { return detail::numeric_bound_check(value, std::numeric_limits<Type>::lowest(), std::greater<Type>()); }

    static bool is_greater_than_or(Type value, bool cond)
    { return cond || is_greater_than(value); }
};

//--------------------------------------------------------------------------
template<class Type> 
struct is_non_floating_point
{
    typedef is_non_floating_point type;
    BOOST_STATIC_CONSTANT(bool, value = 
        (mpl::not_< is_floating_point<Type> >::value));
};

}} // namespace boost icl

#endif


