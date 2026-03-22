/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Joaquin M Lopez Munoz
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_DETAIL_ASSOC_CONTAINER_ADAPTOR_HPP_JMLM_260320
#define BOOST_ICL_DETAIL_ASSOC_CONTAINER_ADAPTOR_HPP_JMLM_260320

#include <boost/icl/impl_config.hpp>
#include <utility>

namespace boost{namespace icl{namespace detail
{

template<class Compare>
struct transparent_compare: Compare
{
    using is_transparent = void;
    using super = Compare;

    using super::super;
};

template<class AssocContainer>
struct assoc_container_adaptor: AssocContainer
{
    using key_type = typename AssocContainer::key_type;
    using size_type = typename AssocContainer::size_type;
    using key_compare = typename AssocContainer::key_compare::super;
    using iterator = typename AssocContainer::iterator;
    using const_iterator = typename AssocContainer::const_iterator;

    using AssocContainer::AssocContainer;

    size_type count(const key_type& key)
    {
        return AssocContainer::count(std::cref(key));
    }

    size_type count(const key_type& key) const
    {
        return AssocContainer::count(std::cref(key));
    }

    iterator find(const key_type& key)
    {
        return AssocContainer::find(std::cref(key));
    }

    const_iterator find(const key_type& key) const
    {
        return AssocContainer::find(std::cref(key));
    }

    std::pair<iterator, iterator> equal_range(const key_type& key)
    {
        return AssocContainer::equal_range(std::cref(key));
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        return AssocContainer::equal_range(std::cref(key));
    }

    iterator lower_bound(const key_type& key)
    {
        return AssocContainer::lower_bound(std::cref(key));
    }

    const_iterator lower_bound(const key_type& key) const
    {
        return AssocContainer::lower_bound(std::cref(key));
    }

    iterator upper_bound(const key_type& key)
    {
        return AssocContainer::upper_bound(std::cref(key));
    }

    const_iterator upper_bound(const key_type& key) const
    {
        return AssocContainer::upper_bound(std::cref(key));
    }
};

}}} // namespace detail icl boost

#endif
