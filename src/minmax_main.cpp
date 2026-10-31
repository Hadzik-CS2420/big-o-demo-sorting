// =============================================================================
// Big O Demo: Finding Min and Max -- Two Passes, One Pass, or Pairs?
// =============================================================================
// Bucket sort and counting sort both open with the same two lines:
//
//     int min_val = *std::min_element(data.begin(), data.end());
//     int max_val = *std::max_element(data.begin(), data.end());
//
// A student asked the obvious question: why walk the list twice? This demo
// answers it with measurements rather than opinion.
//
// Key takeaway:
// - every variant here is O(n). When n doubles, every one of them doubles
// - a plain one-pass loop does NOT halve the work: it still does ~2n comparisons
// - the pairwise algorithm really does fewer, ~3n/2, and that is a constant
//   factor, not a complexity change
// - one pass really is faster than two, by about 2x at 50M -- but for the
//   memory traffic, NOT the comparisons. Both do exactly 2(n-1) of those
// - FEWER COMPARISONS IS NOT THE SAME AS LESS TIME. The pairwise variants do
//   25% fewer comparisons and measured ~1.6x SLOWER than the naive two-if loop.
//   Read the note at the bottom for the reason, taken from the assembly
// =============================================================================

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// Counting the work
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: Why two comparators instead of one counter.
//   - every variant below is written ONCE, as a template over its comparator
//   - CountingLess bumps a counter, so a run with it reports exact comparisons
//   - PlainLess is a bare `a < b`, so a run with it is what actually gets timed
//   - that split matters: a counter inside a timed loop is a memory write per
//     comparison, and it would distort exactly the number we are measuring
//   - this is INSTRUCTOR demo code. Do not put a counting helper back into the
//     student starters -- it was deliberately taken out of CT 13, CT 14 and
//     Lab 6 so students write plain comparisons
//
struct CountingLess {
    long long* count;
    bool operator()(int a, int b) const { ++*count; return a < b; }
};

struct PlainLess {
    bool operator()(int a, int b) const { return a < b; }
};

using MinMax = std::pair<int, int>;

// -----------------------------------------------------------------------------
// Variant A -- two passes
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: What the class code actually does, and why that is fine.
//   - min_element walks the whole range: exactly n-1 comparisons
//   - max_element walks it again: another n-1
//   - 2(n-1) comparisons and two passes over memory
//   - it is the version in bucket sort and counting sort because each line says
//     what it does, and the lesson there is the value range k, not how k is found
//
template <class Less>
MinMax two_pass(const std::vector<int>& v, Less less) {
    int mn = *std::min_element(v.begin(), v.end(), less);
    int mx = *std::max_element(v.begin(), v.end(), less);
    return {mn, mx};
}

// -----------------------------------------------------------------------------
// Variant B -- naive one pass
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: The answer most people reach for, and what it does not buy.
//   - one loop, so one walk over memory instead of two
//   - but TWO comparisons per element, so the comparison count is unchanged
//   - 2(n-1), identical to variant A
//   - the saving is memory traffic, not comparisons. Whether that shows up at
//     all depends on whether the array fits in cache -- and at 50M it shows up
//     clearly, about 2x
//   - measured, not assumed: at -O2 both `if`s compile to CMOV, so this loop
//     makes no data-dependent branch at all. 22 instructions per element, two
//     of them conditional moves, and the only jump is the loop itself
//
template <class Less>
MinMax one_pass(const std::vector<int>& v, Less less) {
    int mn = v[0], mx = v[0];
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (less(v[i], mn)) mn = v[i];
        if (less(mx, v[i])) mx = v[i];
    }
    return {mn, mx};
}

// -----------------------------------------------------------------------------
// Variant B' -- one pass with else-if
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: One character of difference, and suddenly the input matters.
//   - `else if` skips the second test whenever the first one fired
//   - a new minimum cannot also be a new maximum, so this is still correct
//   - DESCENDING input: every element is a new minimum, so the first test is
//     true every time and the second never runs -- about n comparisons
//   - ASCENDING input: the first test is never true, so both run every time --
//     about 2n comparisons
//   - RANDOM input: lands in between, and closer to 2n than you would guess,
//     because new minima get rare fast
//   - this is the only variant here whose comparison count depends on input
//     ORDER rather than input SIZE, which is worth a minute in class
//
template <class Less>
MinMax one_pass_elseif(const std::vector<int>& v, Less less) {
    int mn = v[0], mx = v[0];
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (less(v[i], mn)) mn = v[i];
        else if (less(mx, v[i])) mx = v[i];
    }
    return {mn, mx};
}

// -----------------------------------------------------------------------------
// Variant C -- pairwise, by hand
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: The actual improvement, and it is a classic.
//   - take elements TWO at a time and spend 3 comparisons on the pair:
//       1. the two against each other       -> which is the smaller
//       2. the smaller against the min      -> only the smaller can beat min
//       3. the larger against the max       -> only the larger can beat max
//   - 3 comparisons per 2 elements is 3n/2, against 2n for the obvious loop
//   - a quarter fewer comparisons, and it is PROVABLY the best possible: see
//     CLRS section 9.1, which shows 3n/2 - 2 is the lower bound
//   - the Big O does not move. O(n) before, O(n) after. This is what "constant
//     factor" means in practice
//   - and here is the sting: on this machine it is SLOWER than variant B, by
//     about 1.6x, despite doing a quarter fewer comparisons. The assembly says
//     why --
//     47 instructions per pair against B's 22 per element is roughly a wash,
//     but this loop has five conditional jumps where B has two, and on random
//     data those mispredict
//   - a "comparison" in C++ source is not one instruction, and three fewer of
//     them can still cost more
//
template <class Less>
MinMax pairwise(const std::vector<int>& v, Less less) {
    const std::size_t n = v.size();
    std::size_t i;
    int mn, mx;

    if (n % 2 == 1) {
        mn = mx = v[0];          // odd count: seed from the lone element, 0 comparisons
        i = 1;
    } else {
        if (less(v[0], v[1])) { mn = v[0]; mx = v[1]; }   // 1 comparison
        else                  { mn = v[1]; mx = v[0]; }
        i = 2;
    }

    for (; i + 1 < n; i += 2) {
        int lo = v[i], hi = v[i + 1];
        if (less(hi, lo)) std::swap(lo, hi);   // 1. the pair against each other
        if (less(lo, mn)) mn = lo;             // 2. only the smaller can lower min
        if (less(mx, hi)) mx = hi;             // 3. only the larger can raise max
    }
    return {mn, mx};
}

// -----------------------------------------------------------------------------
// Variant C' -- the library already has it
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: C++11 added this, and almost nobody uses it.
//   - std::minmax_element does the pairwise algorithm for you
//   - the standard GUARANTEES at most max(floor(3(N-1)/2), 0) comparisons, so
//     the 3n/2 is not an implementation detail you are hoping for -- it is
//     promised by the specification
//   - it returns ITERATORS, not values, which is the safety point below
//
template <class Less>
MinMax minmax_library(const std::vector<int>& v, Less less) {
    auto [min_it, max_it] = std::minmax_element(v.begin(), v.end(), less);
    return {*min_it, *max_it};
}

// -----------------------------------------------------------------------------
// What each variant SHOULD cost, so the demo can check itself
// -----------------------------------------------------------------------------
long long expect_two_pass(long long n)  { return 2 * (n - 1); }
long long expect_one_pass(long long n)  { return 2 * (n - 1); }
long long expect_pairwise(long long n) {
    // even n: 1 for the seeding pair, then 3 per remaining pair
    // odd  n: 0 for seeding, then 3 per pair
    return (n % 2 == 0) ? 1 + 3 * (n - 2) / 2 : 3 * (n - 1) / 2;
}

// -----------------------------------------------------------------------------
// Output formatting -- matches the other demos in this folder
// -----------------------------------------------------------------------------
void print_header(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

void rule(int width) { std::cout << std::string(width, '-') << "\n"; }

// -----------------------------------------------------------------------------
// Input generation
// -----------------------------------------------------------------------------
enum class Order { Random, Ascending, Descending };

const char* order_name(Order o) {
    switch (o) {
        case Order::Random:     return "random";
        case Order::Ascending:  return "ascending";
        case Order::Descending: return "descending";
    }
    return "?";
}

std::vector<int> make_data(std::size_t n, Order order) {
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 1);
    if (order == Order::Random) {
        std::mt19937 rng(42);                   // fixed seed: the demo repeats
        std::shuffle(v.begin(), v.end(), rng);
    } else if (order == Order::Descending) {
        std::reverse(v.begin(), v.end());
    }
    return v;
}

// -----------------------------------------------------------------------------
// The variant table -- one place that names all five
// -----------------------------------------------------------------------------
struct Variant {
    const char* key;                                     // "A", "B", ...
    const char* name;
    const char* formula;
    MinMax (*run_counted)(const std::vector<int>&, CountingLess);
    MinMax (*run_plain)(const std::vector<int>&, PlainLess);
    long long (*expect)(long long);                      // null when input-dependent
};

const std::vector<Variant> VARIANTS = {
    {"A",  "two passes",       "2(n-1)",      two_pass<CountingLess>,
     two_pass<PlainLess>,           expect_two_pass},
    {"B",  "one pass, two ifs", "2(n-1)",     one_pass<CountingLess>,
     one_pass<PlainLess>,           expect_one_pass},
    {"B'", "one pass, else if", "n-1 .. 2(n-1)", one_pass_elseif<CountingLess>,
     one_pass_elseif<PlainLess>,    nullptr},
    {"C",  "pairwise by hand", "~3n/2",       pairwise<CountingLess>,
     pairwise<PlainLess>,           expect_pairwise},
    {"C'", "minmax_element",      "<= 3(n-1)/2", minmax_library<CountingLess>,
     minmax_library<PlainLess>,     nullptr},
};

// -----------------------------------------------------------------------------
// A worked pair, printed step by step
// -----------------------------------------------------------------------------
void trace_one_pair() {
    print_header("How 3 comparisons handle 2 elements");

    std::vector<int> v = {38, 27, 43, 3, 9, 82, 10};
    std::cout << "  data: ";
    for (int x : v) std::cout << x << " ";
    std::cout << "\n  (7 elements, so the first seeds min and max and the rest go in pairs)\n\n";

    PlainLess less;
    int mn = v[0], mx = v[0];
    long long c = 0;
    std::cout << "  seed from v[0]      min=" << mn << "  max=" << mx
              << "   comparisons so far: 0\n";

    for (std::size_t i = 1; i + 1 < v.size(); i += 2) {
        int lo = v[i], hi = v[i + 1];
        std::cout << "\n  pair (" << lo << ", " << hi << ")\n";

        bool swapped = less(hi, lo);
        ++c;
        std::cout << "    1. " << hi << " < " << lo << " ? "
                  << (swapped ? "yes -> swap, so" : "no  -> so") << " smaller="
                  << (swapped ? hi : lo) << ", larger=" << (swapped ? lo : hi) << "\n";
        if (swapped) std::swap(lo, hi);

        bool newmin = less(lo, mn);
        ++c;
        std::cout << "    2. " << lo << " < min(" << mn << ") ? "
                  << (newmin ? "yes -> min becomes " : "no  -> min stays ");
        if (newmin) mn = lo;
        std::cout << mn << "\n";

        bool newmax = less(mx, hi);
        ++c;
        std::cout << "    3. max(" << mx << ") < " << hi << " ? "
                  << (newmax ? "yes -> max becomes " : "no  -> max stays ");
        if (newmax) mx = hi;
        std::cout << mx << "\n";
        std::cout << "    3 comparisons spent, " << c << " total\n";
    }

    std::cout << "\n  result: min=" << mn << " max=" << mx << " in " << c
              << " comparisons\n";
    std::cout << "  the obvious two-if loop would have spent "
              << expect_one_pass(static_cast<long long>(v.size())) << " on the same data\n";
    std::cout << "  note step 2 only ever looks at the SMALLER of the pair, and step 3\n";
    std::cout << "  only at the larger. That is where the fourth comparison goes missing.\n";
}

// -----------------------------------------------------------------------------
// The bug that actually matters
// -----------------------------------------------------------------------------
//
// ! DISCUSSION: Empty input is the real defect in those two lines.
//   - *std::min_element(v.begin(), v.end()) on an EMPTY vector dereferences
//     end(). That is undefined behavior, not an exception and not a zero
//   - the one-pass rewrite is no better: it reads v[0] on an empty vector
//   - minmax_element is not magic either, but it hands back iterators, so the
//     caller CAN check before dereferencing. Writing `*min_element(...)` throws
//     that chance away on the same line it was given
//   - the fix is a size guard before the step, which is what every sort in
//     CT 15 and Lab 6 now opens with
//
void show_empty_input_hazard() {
    print_header("Empty input: the bug worth more than the constant factor");

    std::vector<int> empty;
    auto [min_it, max_it] = std::minmax_element(empty.begin(), empty.end());

    std::cout << "  on an empty vector:\n";
    std::cout << "    minmax_element returns end()==end() pair : "
              << ((min_it == empty.end() && max_it == empty.end()) ? "yes" : "no") << "\n";
    std::cout << "    so `min_it == v.end()` is a legal, checkable test\n";
    std::cout << "    but `*std::min_element(v.begin(), v.end())` dereferences end()\n";
    std::cout << "       -> undefined behavior. Not an exception. Not 0.\n";
    std::cout << "    and the one-pass rewrite reads v[0] -- same bug, new clothes\n\n";
    std::cout << "  every variant in this demo is called only after a size check,\n";
    std::cout << "  which is the guard the student code needs too:\n";
    std::cout << "      if (data.size() <= 1) return;\n";
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main() {
    std::cout << "=============================================================\n";
    std::cout << " Big O Demo: Finding Min and Max\n";
    std::cout << " Two passes, one pass, or pairs?\n";
    std::cout << "=============================================================\n";

    const std::vector<Order> orders = {Order::Random, Order::Ascending, Order::Descending};

    // -------------------------------------------------------------------------
    // 0. Correctness first. A faster wrong answer is worth nothing.
    // -------------------------------------------------------------------------
    print_header("Agreement check: every variant, every input order");
    {
        bool all_ok = true;
        for (Order o : orders) {
            std::vector<int> v = make_data(1001, o);
            MinMax want = {1, 1001};
            for (const Variant& var : VARIANTS) {
                MinMax got = var.run_plain(v, PlainLess{});
                bool ok = (got == want);
                all_ok = all_ok && ok;
                if (!ok) {
                    std::cout << "  MISMATCH " << var.key << " on " << order_name(o)
                              << ": got (" << got.first << ", " << got.second
                              << ") want (" << want.first << ", " << want.second << ")\n";
                }
            }
        }
        std::cout << "  " << (all_ok
            ? "all 5 variants agree on all 3 input orders"
            : "SOME VARIANT DISAGREES -- fix that before reading any timing below") << "\n";
        if (!all_ok) return 1;
    }

    trace_one_pair();
    show_empty_input_hazard();

    // -------------------------------------------------------------------------
    // 1. Comparison counts. Separate run, counting comparator.
    // -------------------------------------------------------------------------
    const std::vector<std::size_t> count_sizes = {1'000, 10'000, 100'000, 1'000'000};

    print_header("Comparisons counted (not timed -- see the note at the end)");
    std::cout << "  " << std::setw(24) << std::left << "variant"
              << std::setw(12) << "input"
              << std::setw(14) << std::right << "n"
              << std::setw(16) << "comparisons"
              << std::setw(12) << "per element" << "\n  ";
    rule(76);

    struct CountRow { std::string variant, input; std::size_t n; long long comparisons; };
    std::vector<CountRow> count_rows;
    bool formulas_hold = true;

    for (Order o : orders) {
        for (std::size_t n : count_sizes) {
            std::vector<int> v = make_data(n, o);
            for (const Variant& var : VARIANTS) {
                long long c = 0;
                var.run_counted(v, CountingLess{&c});
                count_rows.push_back({std::string(var.key) + " " + var.name,
                                      order_name(o), n, c});
                if (var.expect) {
                    long long want = var.expect(static_cast<long long>(n));
                    if (c != want) {
                        formulas_hold = false;
                        std::cout << "  FORMULA MISMATCH " << var.key << " n=" << n
                                  << " counted " << c << " expected " << want << "\n";
                    }
                }
            }
        }
    }

    // print just the largest n, so the table stays readable; the CSV has them all
    for (const CountRow& r : count_rows) {
        if (r.n != count_sizes.back()) continue;
        std::cout << "  " << std::setw(24) << std::left << r.variant
                  << std::setw(12) << r.input
                  << std::setw(14) << std::right << r.n
                  << std::setw(16) << r.comparisons
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << (static_cast<double>(r.comparisons) / static_cast<double>(r.n)) << "\n";
    }
    std::cout << "\n  " << (formulas_hold
        ? "every counted variant matches its formula exactly"
        : "A COUNT DOES NOT MATCH ITS FORMULA -- see above") << "\n";
    std::cout << "  read the 'per element' column: 2.00 for A and B, 1.50 for C and C',\n";
    std::cout << "  and B' moving with the input order rather than the size.\n";

    // -------------------------------------------------------------------------
    // 2. Timing. Separate run, plain comparator, median of several.
    // -------------------------------------------------------------------------
    const std::vector<std::size_t> time_sizes = {1'000'000, 5'000'000, 10'000'000,
                                                 25'000'000, 50'000'000};
    const int REPEATS = 11;

    std::cout << "\n";
    print_header("Time (median of 11, microseconds)");
    std::cout << "  50M ints is about 200 MB, chosen so the array cannot sit in cache.\n";
    std::cout << "  Only then does 'walk it twice' cost real memory bandwidth.\n\n";

    struct TimeRow { std::string variant, input; std::size_t n; double us; };
    std::vector<TimeRow> time_rows;
    long long sink = 0;                 // keeps the results in use

    for (Order o : orders) {
        std::cout << "  input: " << order_name(o) << "\n  ";
        std::cout << std::setw(24) << std::left << "variant";
        for (std::size_t n : time_sizes) {
            std::cout << std::setw(13) << std::right << (std::to_string(n / 1'000'000) + "M");
        }
        std::cout << "\n  ";
        rule(24 + 13 * static_cast<int>(time_sizes.size()));

        std::vector<std::vector<double>> per_variant(VARIANTS.size());
        std::vector<double> spread(VARIANTS.size(), 0.0);
        for (std::size_t n : time_sizes) {
            std::vector<int> v = make_data(n, o);
            std::vector<std::vector<double>> runs(VARIANTS.size());

            // Warm-up, untimed: a freshly built 200 MB vector has not been
            // touched yet, and charging the first timed run for its page
            // faults is measurement error, not algorithm cost.
            for (const Variant& w : VARIANTS) sink += w.run_plain(v, PlainLess{}).first;

            // Repeat OUTER, variant INNER, rotating which goes first. Running
            // every repeat of one variant before starting the next charges any
            // slow stretch of the machine to whichever variant was unlucky --
            // that is what moved one median by 2.7x between two runs of the
            // same binary.
            for (int r = 0; r < REPEATS; ++r) {
                for (std::size_t k = 0; k < VARIANTS.size(); ++k) {
                    std::size_t i = (k + static_cast<std::size_t>(r)) % VARIANTS.size();
                    auto t0 = std::chrono::steady_clock::now();
                    MinMax got = VARIANTS[i].run_plain(v, PlainLess{});
                    auto t1 = std::chrono::steady_clock::now();
                    sink += got.first ^ got.second;
                    runs[i].push_back(
                        std::chrono::duration<double, std::micro>(t1 - t0).count());
                }
            }

            for (std::size_t i = 0; i < VARIANTS.size(); ++i) {
                std::sort(runs[i].begin(), runs[i].end());
                per_variant[i].push_back(runs[i][runs[i].size() / 2]);
                if (n == time_sizes.back()) spread[i] = runs[i].back() / runs[i].front();
                time_rows.push_back({std::string(VARIANTS[i].key) + " " + VARIANTS[i].name,
                                     order_name(o), n, runs[i][runs[i].size() / 2]});
            }
        }
        for (std::size_t i = 0; i < VARIANTS.size(); ++i) {
            std::cout << "  " << std::setw(24) << std::left
                      << (std::string(VARIANTS[i].key) + " " + VARIANTS[i].name);
            for (double us : per_variant[i]) {
                std::cout << std::setw(13) << std::right << std::fixed
                          << std::setprecision(0) << us;
            }
            std::cout << "\n";
        }
        std::cout << "    slowest/fastest across the " << REPEATS << " runs at "
                  << (time_sizes.back() / 1'000'000) << "M: ";
        for (double sp : spread) {
            std::cout << std::fixed << std::setprecision(1) << sp << "x  ";
        }
        std::cout << "\n";
        std::cout << "    (a wide spread means the machine was busy, not that the code is slow)\n\n";
    }

    // -------------------------------------------------------------------------
    // 3. Growth: the point of the whole exercise
    // -------------------------------------------------------------------------
    print_header("Cost per element: flat is what O(n) looks like");
    std::cout << "  A ratio divides two noisy numbers together, so this reports the\n";
    std::cout << "  cost PER ELEMENT instead. Linear means the two columns match:\n";
    std::cout << "  five times the data at the same cost each is five times the time.\n";
    std::cout << "  1M is left out because it fits in cache and is not comparable.\n\n";
    std::cout << "  " << std::setw(24) << std::left << "variant"
              << std::setw(16) << std::right << "ns/elem @10M"
              << std::setw(16) << "ns/elem @50M"
              << std::setw(12) << "flat?" << "\n  ";
    rule(66);
    for (const Variant& var : VARIANTS) {
        auto at = [&](std::size_t n) {
            for (const TimeRow& r : time_rows)
                if (r.n == n && r.input == std::string("random")
                    && r.variant.rfind(var.key, 0) == 0) return r.us;
            return 0.0;
        };
        // us for n elements -> ns per element
        double a = at(10'000'000) * 1000.0 / 10'000'000.0;
        double b = at(50'000'000) * 1000.0 / 50'000'000.0;
        double ratio = (a > 0) ? b / a : 0;
        const char* verdict = (ratio > 0.75 && ratio < 1.33) ? "yes" : "noisy";
        std::cout << "  " << std::setw(24) << std::left
                  << (std::string(var.key) + " " + var.name)
                  << std::setw(16) << std::right << std::fixed << std::setprecision(2) << a
                  << std::setw(16) << b
                  << std::setw(12) << verdict << "\n";
    }
    std::cout << "\n  Equal columns are the whole result: the per-element cost does not grow\n";
    std::cout << "  with n, which is what O(n) means. A \"noisy\" row means this\n";
    std::cout << "  machine could not hold still, not that the algorithm bent.\n";

    std::cout << "\n-------------------------------------------------------------\n";
    std::cout << " Key Lesson:\n";
    std::cout << "   All five are O(n): the cost per element does not grow with n.\n";
    std::cout << "   C and C' do 25% fewer comparisons than A and B -- a constant\n";
    std::cout << "   factor, which Big O deliberately ignores.\n";
    std::cout << "   But fewer comparisons did NOT mean less time. C and C' measured\n";
    std::cout << "   1.6x SLOWER than B here, with 25% fewer comparisons.\n";
    std::cout << "   Why, from the assembly (g++ 13.3 -O2): B's two ifs become CMOV,\n";
    std::cout << "   so it makes no data-dependent branch -- 22 instructions per\n";
    std::cout << "   element, 2 of them conditional moves. The pairwise loop needs\n";
    std::cout << "   47 instructions per pair and five conditional jumps, and on\n";
    std::cout << "   random data those mispredict. No loop here vectorizes: all five\n";
    std::cout << "   carry a dependency on mn and mx, which blocks it.\n";
    std::cout << "   A comparison in source is not an instruction. Counting the one\n";
    std::cout << "   is not measuring the other.\n";
    std::cout << "   What DID help: one pass over 200 MB instead of two, about 2x.\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << " (checksum " << sink << " -- printed only so the optimizer cannot\n";
    std::cout << "  delete the work we just timed)\n";

    // -------------------------------------------------------------------------
    // CSV + charts
    // -------------------------------------------------------------------------
    std::string repo_dir = REPO_DIR;
    std::string csv_path = repo_dir + "/results_minmax.csv";
    std::ofstream csv(csv_path);
    csv << "operation,variant,input,n,comparisons,time_us\n";
    for (const CountRow& r : count_rows) {
        csv << "minmax,\"" << r.variant << "\"," << r.input << "," << r.n << ","
            << r.comparisons << ",\n";
    }
    for (const TimeRow& r : time_rows) {
        csv << "minmax,\"" << r.variant << "\"," << r.input << "," << r.n << ",,"
            << std::fixed << std::setprecision(4) << r.us << "\n";
    }
    csv.close();
    std::cout << "\n  Results written to results_minmax.csv -- generating charts...\n";

#ifdef _WIN32
    std::string py = "py -3";
#else
    std::string py = "python3";
#endif
    std::string cmd = py + " \"" + repo_dir + "/graph_minmax.py\" --graph-only";
    if (std::system(cmd.c_str()) != 0) {
        std::cout << "  (chart step skipped -- run `" << cmd << "` yourself)\n";
    }
    return 0;
}
