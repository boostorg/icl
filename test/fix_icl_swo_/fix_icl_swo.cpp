// Unit tests for the Boost.ICL strict-weak-ordering search fix
// (interval_base_map / interval_base_set lower_bound/upper_bound/equal_range under the
//  exclusive_less_than comparator, which is NOT a strict weak ordering).
//
// Self-contained: no test framework required, only a C++17 compiler and the Boost.ICL headers.
//
//   c++ -std=c++17 -I<boost-include> test_icl_swo_fix.cpp -o test_icl_swo_fix
//   ./test_icl_swo_fix ; echo "exit=$?"
//
// Expected results:
//   * UNPATCHED Boost.ICL on libc++ >= LLVM 22:
//       - the interval_map construction case ABORTS (BOOST_ASSERT in gap_insert), OR
//       - if assertions are disabled, the accessor/decomposition checks FAIL.
//     Either way the suite does not reach "ALL TESTS PASSED".
//   * PATCHED Boost.ICL: prints "ALL TESTS PASSED", exit 0.
//
// Note: the interval_map construction test is placed LAST, because on an unpatched build it
// aborts the process; the accessor tests above it already demonstrate the wrong behaviour
// (without relying on a crash) on builds where BOOST_ASSERT is compiled out.

#include <boost/icl/interval_map.hpp>
#include <boost/icl/split_interval_set.hpp>
#include <boost/icl/continuous_interval.hpp>

#include <set>
#include <string>
#include <sstream>
#include <iostream>
#include <iterator>

using Iv = boost::icl::interval<double>::type;

// ---- tiny assertion harness ------------------------------------------------------------------
static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond, msg)                                                              \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) { ++g_failures; std::cerr << "FAIL: " << (msg) << "\n"; }        \
    } while (0)

static std::string br(const Iv& i)
{
    std::ostringstream os;
    os << boost::icl::left_bracket(i) << i.lower() << ',' << i.upper()
       << boost::icl::right_bracket(i);
    return os.str();
}

// ---- helpers ---------------------------------------------------------------------------------
template <class SetT>
static std::string dump(const SetT& s)
{
    std::ostringstream os;
    for (auto& e : s) os << br(e) << ' ';
    return os.str();
}

// Brute-force reference: the contiguous run of stored intervals that overlap q,
// returned as [first, end). overlap == neither side exclusively-less than the other.
template <class SetT>
static void brute_overlap_range(const SetT& s, const Iv& q,
                                typename SetT::const_iterator& bf,
                                typename SetT::const_iterator& be)
{
    typename SetT::key_compare comp;
    auto overlaps = [&](const Iv& a, const Iv& b) { return !comp(a, b) && !comp(b, a); };
    bf = s.end(); be = s.end();
    for (auto it = s.begin(); it != s.end(); ++it)
        if (overlaps(*it, q)) { if (bf == s.end()) bf = it; be = std::next(it); }
}

// ===============================================================================================
// Test 1: interval_set accessors return the FULL overlapping run (no crash needed).
// ===============================================================================================
static void test_set_accessors()
{
    using ISet = boost::icl::split_interval_set<double>;

    // Three intervals sharing the lower endpoint 0. split_interval_set keeps the internal split
    // boundary at 1, storing two separate entries [0,1] and (1,3] that share the point 1 -- the
    // layout that makes exclusive_less_than's "equivalence" (overlap) intransitive for a query
    // that straddles 1.
    ISet s;
    s += Iv::closed(0, 1);
    s += Iv::closed(0, 3);
    CHECK(dump(s) == "[0,1] (1,3] ", "set layout precondition: got '" + dump(s) + "'");

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
        const bool empty_run = (bf == be); // no stored interval overlaps the query

        auto lb = s.lower_bound(c.q);
        auto ub = s.upper_bound(c.q);
        auto er = s.equal_range(c.q);

        if (empty_run) {
            // For an empty overlap run the only contract every ICL algorithm relies on is that the
            // range is EMPTY: lower_bound == upper_bound (== equal_range pair). The exact iterator
            // used to denote "empty" is unspecified, so we do not pin it to a particular element.
            CHECK(lb == ub,             std::string("empty range lb==ub  ") + c.name);
            CHECK(er.first == er.second, std::string("empty range equal_range  ") + c.name);
        } else {
            CHECK(lb == bf,        std::string("lower_bound  ") + c.name);
            CHECK(ub == be,        std::string("upper_bound  ") + c.name);
            CHECK(er.first == bf,  std::string("equal_range.first  ") + c.name);
            CHECK(er.second == be, std::string("equal_range.second ") + c.name);
            // Range must never be inverted (the failure mode that corrupts the tree downstream).
            CHECK(std::distance(lb, ub) > 0, std::string("range not inverted ") + c.name);
        }
    }
}

// ===============================================================================================
// Test 2: interval_map decomposition is correct for a query spanning multiple stored entries.
//         (Also covers the unbounded upper-bound walk shape: Q spans the whole run.)
// ===============================================================================================
static void test_map_multispan()
{
    using IMap = boost::icl::interval_map<double, std::set<int>>;
    auto dumpm = [](const IMap& m) {
        std::ostringstream os;
        for (auto& e : m) {
            os << br(e.first) << '{';
            for (int v : e.second) os << v;
            os << "} ";
        }
        return os.str();
    };

    // Adjacent unit segments, then a single value covering all of them.
    IMap m;
    m += std::make_pair(Iv::right_open(0, 1), std::set<int>{0});
    m += std::make_pair(Iv::right_open(1, 2), std::set<int>{1});
    m += std::make_pair(Iv::right_open(2, 3), std::set<int>{2});
    m += std::make_pair(Iv::closed(0, 3),     std::set<int>{9}); // spans all three + the point 3

    const std::string expected = "[0,1){09} [1,2){19} [2,3){29} [3,3]{9} ";
    CHECK(dumpm(m) == expected, "map multispan decomposition: got '" + dumpm(m) + "'");
}

// ===============================================================================================
// Test 3 (LAST): interval_map construction that ABORTS on unpatched libc++ >= LLVM 22.
//         Three CLOSED, non-degenerate intervals sharing the lower endpoint 0.
// ===============================================================================================
static void test_map_crash_case()
{
    using IMap = boost::icl::interval_map<double, std::set<int>>;
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
    CHECK(dumpm(m) == expected, "map crash-case decomposition: got '" + dumpm(m) + "'");
}

int main()
{
    test_set_accessors();
    test_map_multispan();
    test_map_crash_case(); // last: may abort the process on an unpatched build

    if (g_failures == 0) {
        std::cout << "ALL TESTS PASSED (" << g_checks << " checks)\n";
        return 0;
    }
    std::cout << g_failures << " of " << g_checks << " checks FAILED\n";
    return 1;
}
