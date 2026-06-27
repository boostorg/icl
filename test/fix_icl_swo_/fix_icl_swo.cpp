// =============================================================================
// Boost.Test suite for the Boost.ICL strict-weak-ordering search fix.
//
// interval_base_set / interval_base_map lower_bound / upper_bound / equal_range use
// exclusive_less_than, which is NOT a strict weak ordering for a query that overlaps
// several disjoint stored intervals. libc++ >= LLVM 22 optimized those lookups into a
// single root-to-leaf descent that assumes SWO, so the unpatched accessors can return
// the wrong boundary (and the interval_map build path can abort in gap_insert).
//
// This file merges two suites:
//   * accessor / decomposition tests on continuous (dynamic) intervals, incl. the
//     construction case that aborts on an unpatched libc++ >= 22 build;
//   * the PR #57 interval-type matrix proving the SWO-safe accessors return ICL's
//     documented boundaries for every interval type (static & dynamic, discrete &
//     continuous), including the static-continuous case fixed by the point-lookup
//     fallback (Patch B).
//
// Header-only Boost.Test (no link step needed):
//   c++ -std=c++17 -I<boost-include> test_icl_swo_fix.cpp -o test_icl_swo_fix
//   ./test_icl_swo_fix
//
// Build against clang-22 + libc++ (the failing config) WITHOUT defining
// _LIBCPP_ENABLE_LEGACY_TREE_LOWER_UPPER_BOUND:
//   * UNPATCHED ICL  -> the construction case aborts in gap_insert, or (asserts off)
//                       the accessor/decomposition checks FAIL.
//   * PATCHED ICL    -> all test cases pass.
// On libstdc++ / clang<=21 every case passes either way (the assertions encode the
// correct boundary).
//
// Contract under test (exclusive_less_than `prec`):
//   lower_bound(Q) = first stored E with !prec(E,Q)  -> leftmost E overlapping Q,
//                                                       else first E entirely right of Q
//   upper_bound(Q) = first stored E with  prec(Q,E)  -> first E entirely right of Q
//   [lower_bound(Q), upper_bound(Q)) = the contiguous run of intervals overlapping Q
// =============================================================================
#define BOOST_TEST_MODULE icl_swo_search_fix
#include <boost/test/included/unit_test.hpp>

#include <boost/icl/interval_set.hpp>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/split_interval_set.hpp>
#include <boost/icl/right_open_interval.hpp>
#include <boost/icl/left_open_interval.hpp>
#include <boost/icl/closed_interval.hpp>
#include <boost/icl/open_interval.hpp>
#include <boost/icl/discrete_interval.hpp>
#include <boost/icl/continuous_interval.hpp>
#include <boost/icl/concept/interval.hpp>      // construct

#include <set>
#include <string>
#include <sstream>
#include <iterator>

using namespace boost::icl;

// ---- shared helpers --------------------------------------------------------------------------

template <class IvT>
static std::string br(const IvT& i)
{
    std::ostringstream os;
    os << left_bracket(i) << i.lower() << ',' << i.upper()
       << right_bracket(i);
    return os.str();
}

template <class SetT>
static std::string dump(const SetT& s)
{
    std::ostringstream os;
    for (auto& e : s) os << br(e) << ' ';
    return os.str();
}

// Brute-force reference: the contiguous run of stored intervals overlapping q, as [first,end).
// overlap == neither side exclusively-less than the other.
template <class SetT>
static void brute_overlap_range(const SetT& s, const typename SetT::key_type& q,
                                typename SetT::const_iterator& bf,
                                typename SetT::const_iterator& be)
{
    typename SetT::key_compare comp;
    auto overlaps = [&](const typename SetT::key_type& a, const typename SetT::key_type& b)
    { return !comp(a, b) && !comp(b, a); };
    bf = s.end(); be = s.end();
    for (auto it = s.begin(); it != s.end(); ++it)
        if (overlaps(*it, q)) { if (bf == s.end()) bf = it; be = std::next(it); }
}

// Matrix helper: build {ivs...}, query Q, expect lower/upper at the given bounds.
template <class SetT, class IvT>
static void check_run(SetT& s, const IvT& Q,
                      typename SetT::domain_type lb_a, typename SetT::domain_type lb_b,
                      bool upper_is_end,
                      typename SetT::domain_type ub_a = 0,
                      typename SetT::domain_type ub_b = 0)
{
    auto lb = s.lower_bound(Q);
    BOOST_CHECK(lb != s.end());
    if (lb != s.end()) BOOST_CHECK(*lb == construct<IvT>(lb_a, lb_b));

    auto ub = s.upper_bound(Q);
    if (upper_is_end) BOOST_CHECK(ub == s.end());
    else { BOOST_CHECK(ub != s.end()); if (ub != s.end()) BOOST_CHECK(*ub == construct<IvT>(ub_a, ub_b)); }

    auto er = s.equal_range(Q);
    BOOST_CHECK(er.first == lb);
    BOOST_CHECK(er.second == ub);
}

// =============================================================================================
// Accessors: interval_set returns the FULL overlapping run (no crash needed).
// =============================================================================================
BOOST_AUTO_TEST_CASE(set_accessors_overlap_run)
{
    using Iv   = interval<double>::type;   // continuous_interval<double> by default
    using ISet = split_interval_set<double>;

    // Two entries sharing the boundary 1: split_interval_set stores [0,1] and (1,3] separately,
    // the layout that makes exclusive_less_than's overlap-equivalence intransitive for a query
    // straddling 1.
    ISet s;
    s += Iv::closed(0, 1);
    s += Iv::closed(0, 3);
    BOOST_CHECK_MESSAGE(dump(s) == "[0,1] (1,3] ",
                        "set layout precondition: got '" + dump(s) + "'");

    struct Case { Iv q; const char* name; };
    const Case cases[] = {
        { Iv::closed(0, 3),     "Q=[0,3] straddles boundary 1, overlaps BOTH entries" },
        { Iv::closed(1, 1),     "Q=[1,1] singleton at the shared boundary, overlaps BOTH" },
        { Iv::closed(0, 1),     "Q=[0,1] overlaps [0,1], only touches (1,3] at open 1" },
        { Iv::open(1, 3),       "Q=(1,3) overlaps only (1,3]" },
        { Iv::closed(2, 5),     "Q=[2,5] overlaps only (1,3]" },
        { Iv::right_open(0, 1), "Q=[0,1) overlaps only [0,1]" },
        { Iv::closed(4, 5),     "Q=[4,5] empty run, right of everything" },
        { Iv::closed(-2, -1),   "Q=[-2,-1] empty run, left of everything" },
    };

    for (auto& c : cases) {
        ISet::const_iterator bf, be;
        brute_overlap_range(s, c.q, bf, be);
        const bool empty_run = (bf == be);

        auto lb = s.lower_bound(c.q);
        auto ub = s.upper_bound(c.q);
        auto er = s.equal_range(c.q);

        if (empty_run) {
            // Only contract for an empty run is that the range is EMPTY (lb == ub). The exact
            // iterator denoting "empty" is unspecified, so it is not pinned to an element.
            BOOST_CHECK_MESSAGE(lb == ub,              std::string("empty range lb==ub  ") + c.name);
            BOOST_CHECK_MESSAGE(er.first == er.second, std::string("empty range equal_range  ") + c.name);
        } else {
            BOOST_CHECK_MESSAGE(lb == bf,        std::string("lower_bound  ") + c.name);
            BOOST_CHECK_MESSAGE(ub == be,        std::string("upper_bound  ") + c.name);
            BOOST_CHECK_MESSAGE(er.first == bf,  std::string("equal_range.first  ") + c.name);
            BOOST_CHECK_MESSAGE(er.second == be, std::string("equal_range.second ") + c.name);
            BOOST_CHECK_MESSAGE(std::distance(lb, ub) > 0, std::string("range not inverted ") + c.name);
        }
    }
}

// =============================================================================================
// Decomposition: interval_map is correct for a query spanning multiple stored entries
// (also exercises the unbounded upper-bound walk: Q spans the whole run).
// =============================================================================================
BOOST_AUTO_TEST_CASE(map_multispan_decomposition)
{
    using Iv   = interval<double>::type;
    using IMap = interval_map<double, std::set<int>>;
    auto dumpm = [](const IMap& m) {
        std::ostringstream os;
        for (auto& e : m) {
            os << br(e.first) << '{';
            for (int v : e.second) os << v;
            os << "} ";
        }
        return os.str();
    };

    IMap m;
    m += std::make_pair(Iv::right_open(0, 1), std::set<int>{0});
    m += std::make_pair(Iv::right_open(1, 2), std::set<int>{1});
    m += std::make_pair(Iv::right_open(2, 3), std::set<int>{2});
    m += std::make_pair(Iv::closed(0, 3),     std::set<int>{9}); // spans all three + the point 3

    const std::string expected = "[0,1){09} [1,2){19} [2,3){29} [3,3]{9} ";
    BOOST_CHECK_MESSAGE(dumpm(m) == expected, "map multispan decomposition: got '" + dumpm(m) + "'");
}

// =============================================================================================
// PR #57 interval-type matrix.
// =============================================================================================

// ---- right_open_interval<int>  (static, discrete) : honnesh's original case ----
BOOST_AUTO_TEST_CASE(pr57_right_open_int)
{
    typedef right_open_interval<int> Iv;
    interval_set<int, std::less, Iv> s;
    s.add(Iv(0,2)); s.add(Iv(3,5)); s.add(Iv(6,9)); s.add(Iv(10,12));

    check_run(s, Iv(4,7), /*lb*/3,5, /*ub*/false, 10,12);   // Q=[4,7) overlaps [3,5) and [6,9)

    // touching at an OPEN upper border: a stored interval starting exactly at upper(Q).
    interval_set<int, std::less, Iv> t;
    t.add(Iv(3,5)); t.add(Iv(7,9));
    check_run(t, Iv(1,3), /*lb*/3,5, /*ub*/false, 3,5);     // overlaps nothing; lb==ub==[3,5)
    check_run(t, Iv(1,4), /*lb*/3,5, /*ub*/false, 7,9);     // overlaps [3,5); ub=[7,9)
}

// ---- closed_interval<int>  (static, discrete) ----
BOOST_AUTO_TEST_CASE(pr57_closed_int)
{
    typedef closed_interval<int> Iv;
    interval_set<int, std::less, Iv> s;
    s.add(Iv(0,1)); s.add(Iv(3,4)); s.add(Iv(6,8)); s.add(Iv(10,11));

    check_run(s, Iv(4,7), /*lb*/3,4, /*ub*/false, 10,11);   // Q=[4,7] overlaps [3,4] and [6,8]
}

// ---- left_open_interval<int>  (static, discrete) ----
BOOST_AUTO_TEST_CASE(pr57_left_open_int)
{
    typedef left_open_interval<int> Iv;                 // (a,b] = {a+1..b}
    interval_set<int, std::less, Iv> s;
    s.add(Iv(-1,1)); s.add(Iv(2,4)); s.add(Iv(5,8)); s.add(Iv(9,11));

    check_run(s, Iv(3,7), /*lb*/2,4, /*ub*/false, 9,11);    // Q=(3,7] overlaps (2,4] and (5,8]
}

// ---- open_interval<int>  (static, discrete) ----
BOOST_AUTO_TEST_CASE(pr57_open_int)
{
    typedef open_interval<int> Iv;                      // (a,b) = {a+1..b-1}
    interval_set<int, std::less, Iv> s;
    s.add(Iv(-1,2)); s.add(Iv(2,5)); s.add(Iv(5,9)); s.add(Iv(9,12));

    check_run(s, Iv(3,7), /*lb*/2,5, /*ub*/false, 9,12);    // Q=(3,7) overlaps (2,5) and (5,9)
}

// ---- discrete_interval<int>  (DYNAMIC bounds, discrete) ----
BOOST_AUTO_TEST_CASE(pr57_discrete_interval_int)
{
    typedef discrete_interval<int> Iv;
    interval_set<int, std::less, Iv> s;
    s.add(interval<int>::right_open(0,2));
    s.add(interval<int>::right_open(3,5));
    s.add(interval<int>::right_open(6,9));
    s.add(interval<int>::right_open(10,12));

    Iv Q = interval<int>::right_open(4,7);
    auto lb = s.lower_bound(Q); BOOST_CHECK(lb != s.end() && lb->lower()==3);
    auto ub = s.upper_bound(Q); BOOST_CHECK(ub != s.end() && ub->lower()==10);
}

// ---- continuous_interval<double>  (DYNAMIC bounds, continuous) ----
BOOST_AUTO_TEST_CASE(pr57_continuous_interval_double)
{
    typedef continuous_interval<double> Iv;
    interval_set<double, std::less, Iv> s;
    s.add(interval<double>::right_open(0.0, 2.0));
    s.add(interval<double>::right_open(3.0, 5.0));
    s.add(interval<double>::right_open(6.0, 9.0));
    s.add(interval<double>::right_open(10.0,12.0));

    Iv Q = interval<double>::right_open(4.0, 7.0);
    auto lb = s.lower_bound(Q); BOOST_CHECK(lb != s.end() && lb->lower()==3.0);
    auto ub = s.upper_bound(Q); BOOST_CHECK(ub != s.end() && ub->lower()==10.0);

    // open/closed mix
    interval_set<double, std::less, Iv> t;
    t.add(interval<double>::closed(0.0, 1.0));
    t.add(interval<double>::open  (2.0, 4.0));   // (2,4)
    t.add(interval<double>::closed(5.0, 7.0));
    Iv Q2 = interval<double>::closed(3.0, 6.0);  // overlaps (2,4) and [5,7]
    auto lb2 = t.lower_bound(Q2); BOOST_CHECK(lb2 != t.end() && lb2->lower()==2.0);
    auto ub2 = t.upper_bound(Q2); BOOST_CHECK(ub2 == t.end());
}

// ---- right_open_interval<double>  (STATIC bounds, continuous) : Patch B point fallback ----
// has_static_bounds && is_continuous. No singleton exists on a continuous domain, so the
// SWO-safe accessor anchors on a domain POINT via heterogeneous lookup. Pre-patch this is the
// raw call: UB on libc++22 (returns [6,9) instead of [10,12)).
BOOST_AUTO_TEST_CASE(pr57_right_open_double_static)
{
    typedef right_open_interval<double> Iv;
    interval_set<double, std::less, Iv> s;
    s.add(Iv(0.0,2.0)); s.add(Iv(3.0,5.0)); s.add(Iv(6.0,9.0)); s.add(Iv(10.0,12.0));

    auto lb = s.lower_bound(Iv(4.0,7.0)); BOOST_CHECK(lb != s.end() && lb->lower()==3.0);
    auto ub = s.upper_bound(Iv(4.0,7.0)); BOOST_CHECK(ub != s.end() && ub->lower()==10.0); // pre-patch: 6.0
}

// =============================================================================================
// LAST: interval_map construction that ABORTS on unpatched libc++ >= LLVM 22.
// Declared last so the checks above run first; on an unpatched build BOOST_ASSERT in gap_insert
// aborts the process here.
// =============================================================================================
BOOST_AUTO_TEST_CASE(map_crash_case_closed_shared_lower)
{
    using Iv   = interval<double>::type;
    using IMap = interval_map<double, std::set<int>>;
    auto dumpm = [](const IMap& m) {
        std::ostringstream os;
        for (auto& e : m) {
            os << br(e.first) << '{';
            for (int v : e.second) os << v;
            os << "} ";
        }
        return os.str();
    };

    IMap m;
    m += std::make_pair(Iv::closed(0, 1), std::set<int>{0});
    m += std::make_pair(Iv::closed(0, 3), std::set<int>{1});
    m += std::make_pair(Iv::closed(0, 2), std::set<int>{2}); // aborts here if unpatched

    const std::string expected = "[0,1]{012} (1,2]{12} (2,3]{1} ";
    BOOST_CHECK_MESSAGE(dumpm(m) == expected, "map crash-case decomposition: got '" + dumpm(m) + "'");
}


// interval_set: add an interval overlapping a stored one.
// Exercises the blind-add path: the overlapping insert must be rejected as a
// collision and merged, regardless of the std lib's insertion algorithm.
BOOST_AUTO_TEST_CASE(set_overlapping_add_is_swo_safe)
{
    interval_set<int> s;
    s += interval<int>::right_open(0, 5);          // stored, disjoint: {[0,5)}

    BOOST_CHECK_NO_THROW(
        s += interval<int>::right_open(3, 8));     // overlaps [0,5)

    // Sanity: the merged result must be a single [0,8).
    interval_set<int> expected;
    expected += interval<int>::right_open(0, 8);
    BOOST_CHECK_EQUAL(s, expected);
}

// interval_map: add an interval overlapping a stored one.
// Same blind-add path as above; must merge without throwing or corrupting.
BOOST_AUTO_TEST_CASE(map_overlapping_add_is_swo_safe)
{
    interval_map<int,int> m;
    m += std::make_pair(interval<int>::right_open(0, 5), 1);   // {[0,5)->1}

    BOOST_CHECK_NO_THROW(
        m += std::make_pair(interval<int>::right_open(3, 8), 1));
}

// interval_set: multi-interval overlap (collision run).
// The added interval overlaps a contiguous run of stored intervals; the insert
// must land on the run and merge all of them into one.
BOOST_AUTO_TEST_CASE(set_multi_overlap_add_is_swo_safe)
{
    interval_set<int> s;
    s += interval<int>::right_open(0, 5);     // {[0,5), [10,15)}
    s += interval<int>::right_open(10, 15);

    BOOST_CHECK_NO_THROW(
        s += interval<int>::right_open(3, 12));// overlaps BOTH stored intervals

    interval_set<int> expected;
    expected += interval<int>::right_open(0, 15);
    BOOST_CHECK_EQUAL(s, expected);
}

// interval_set: HINTED add overload with a deliberately stale hint.
// The public add(prior,.) forwards a caller hint with a possibly-overlapping
// argument. Tested standard libraries reject the overlap safely here (the
// O(1)-hint neighbour check fails and they fall back to a full search), so this
// is a robustness guard, not a reproduction of a known corruption. A hint must
// never change the result, so compare against the un-hinted add.
BOOST_AUTO_TEST_CASE(set_hinted_overlapping_add_is_swo_safe)
{
    interval_set<int> s;
    s += interval<int>::right_open(0, 1);
    s += interval<int>::right_open(2, 3);
    s += interval<int>::right_open(4, 5);          // {[0,1),[2,3),[4,5)}

    BOOST_CHECK_NO_THROW(
        s.add(std::prev(s.end()), interval<int>::right_open(0, 3)));

    interval_set<int> expected;
    expected += interval<int>::right_open(0, 1);
    expected += interval<int>::right_open(2, 3);
    expected += interval<int>::right_open(4, 5);
    expected += interval<int>::right_open(0, 3);   // un-hinted, known-correct
    BOOST_CHECK_EQUAL(s, expected);
}

// interval_map: HINTED add overload with a stale hint, same robustness guard.
BOOST_AUTO_TEST_CASE(map_hinted_overlapping_add_is_swo_safe)
{
    interval_map<int,int> m;
    m += std::make_pair(interval<int>::right_open(0, 1), 1);
    m += std::make_pair(interval<int>::right_open(2, 3), 1);
    m += std::make_pair(interval<int>::right_open(4, 5), 1);

    BOOST_CHECK_NO_THROW(
        m.add(std::prev(m.end()), std::make_pair(interval<int>::right_open(0, 3), 1)));

    interval_map<int,int> expected;
    expected += std::make_pair(interval<int>::right_open(0, 1), 1);
    expected += std::make_pair(interval<int>::right_open(2, 3), 1);
    expected += std::make_pair(interval<int>::right_open(4, 5), 1);
    expected += std::make_pair(interval<int>::right_open(0, 3), 1); // un-hinted, known-correct
    BOOST_CHECK_EQUAL(m, expected);
}
