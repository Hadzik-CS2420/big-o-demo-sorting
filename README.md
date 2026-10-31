# Big O Demo: Sorting Algorithms

A benchmark program that compares O(n^2) sorting algorithms (bubble sort, insertion sort, selection sort) against O(n log n) algorithms (merge sort, quick sort) to demonstrate the dramatic performance difference as input size grows.

## What It Shows

- **O(n^2) algorithms** (bubble, insertion, selection): when n doubles, runtime grows ~4x
- **O(n log n) algorithms** (merge sort, quick sort): when n doubles, runtime grows ~2x
- At small sizes the difference is modest; at larger sizes the gap becomes enormous

The program benchmarks each algorithm at sizes 1,000 / 5,000 / 10,000 / 50,000 and prints a summary comparison table at the end.

## How to Run

```bash
cmake -B build -S .
cmake --build build
./build/big-o-demo-sorting
```

`CMakeLists.txt` defaults `CMAKE_BUILD_TYPE` to `Release`. Timing an unoptimized
build produces numbers that mean nothing, so do not override it unless you are
debugging.

## Space Complexity Notes

| Algorithm      | Time (avg) | Space   |
|----------------|------------|---------|
| Bubble Sort    | O(n^2)     | O(1)    |
| Insertion Sort | O(n^2)     | O(1)    |
| Selection Sort | O(n^2)     | O(1)    |
| Merge Sort     | O(n log n) | O(n)    |
| Quick Sort     | O(n log n) | O(log n)|

---

# Second demo: finding min and max

```bash
cmake --build build --target big-o-demo-sorting-minmax
./build/big-o-demo-sorting-minmax        # writes results_minmax.csv
python3 graph_minmax.py                  # builds, runs, writes charts_minmax.html
python3 graph_minmax.py --graph-only     # chart an existing CSV
```

## What It Shows

A student asked, about step 1a of bucket sort, why the code walks the list twice:

```cpp
int min_val = *std::min_element(data.begin(), data.end());
int max_val = *std::max_element(data.begin(), data.end());
```

This demo answers it by measuring instead of arguing. It compares five ways of
finding both values, counts their comparisons exactly, times them on up to 50
million ints, and ends up disagreeing with itself in a useful way.

### The variants

| Variant | Code | Comparisons |
|---|---|---|
| A. Two passes | `min_element` then `max_element` | 2(n−1) |
| B. Naive one pass | two `if`s per element | 2(n−1) |
| B′. One pass with `else if` | `if (x<mn) ... else if (x>mx) ...` | n−1 to 2(n−1), by input order |
| C. Pairwise (hand-written) | 3 comparisons per 2 elements | ~3n/2 |
| C′. Library | `std::minmax_element` | ≤ 3(n−1)/2 |

Every variant runs on **random, ascending and descending** input.

### Counted comparisons, at n = 1,000,000

Exact, reproducible, and the same on any machine. The demo asserts each count
against its formula and says so if one disagrees.

| Variant | random | ascending | descending | per element |
|---|---:|---:|---:|---|
| A two passes | 1,999,998 | 1,999,998 | 1,999,998 | 2.00 |
| B one pass, two ifs | 1,999,998 | 1,999,998 | 1,999,998 | 2.00 |
| B′ one pass, else if | 1,999,980 | 1,999,998 | **999,999** | 1.00–2.00 |
| C pairwise by hand | 1,499,998 | 1,499,998 | 1,499,998 | 1.50 |
| C′ `minmax_element` | 1,499,998 | 1,499,998 | 1,499,998 | 1.50 |

B′ is the only one whose count depends on input *order* rather than input
*size*: on descending input every element is a new minimum, so the `else`
skips the second test every time and it costs exactly n−1.

### Measured time, and the point of the whole exercise

Nanoseconds per element, random input, median of 11 interleaved runs, g++ 13.3
`-O2` in the Dev Container image:

| Variant | comparisons/elem | ns/elem @10M | ns/elem @50M |
|---|---:|---:|---:|
| A two passes | 2.00 | 4.32 | 4.71 |
| B one pass, two ifs | 2.00 | **2.58** | **3.09** |
| B′ one pass, else if | 2.00 | 2.97 | 3.75 |
| C pairwise by hand | 1.50 | 5.31 | 5.07 |
| C′ `minmax_element` | 1.50 | 5.10 | 5.07 |

Read the two tables against each other, because they do not agree:

1. **Every variant is O(n).** The cost per element does not grow between 10M
   and 50M. That is what linear means, and it is why none of this changes
   bucket sort's O(n + k).
2. **One pass really is faster than two** — about 2x at 50M. But *not* for the
   reason the question assumed: A and B do exactly the same 2(n−1)
   comparisons. The saving is memory traffic, one walk over 200 MB instead of
   two, and it only appears once the array is far too big for cache.
3. **Fewer comparisons was slower.** C and C′ do 25% fewer comparisons and
   measured ~1.6x *slower* than B. From the assembly: B's two `if`s compile to
   branchless `cmov`, so it makes no data-dependent branch — 22 instructions
   per element. The pairwise loop needs 47 instructions per pair and five
   conditional jumps, which mispredict on random data. No loop here
   vectorizes; all five carry a dependency on `mn`/`mx` that blocks it.

So a comparison in source code is not a machine instruction, and counting the
one is not measuring the other.

### Why the class code still uses two calls

Each line says plainly what it does, the lesson at that point is the value
range *k* rather than how *k* is found, and the step was never the bottleneck.
The 3n/2 algorithm is the textbook result (CLRS §9.1 proves 3n/2 − 2 is the
lower bound) and it is worth knowing — it is simply not worth spending here.

### The bug that matters more than any of this

`*std::min_element(v.begin(), v.end())` on an **empty** vector dereferences
`end()`. That is undefined behavior — not an exception, not a zero. The
one-pass rewrite is no better, because it reads `v[0]`. `minmax_element`
returns iterators, so a caller *can* test `it == v.end()` first, and writing
`*min_element(...)` throws that chance away on the same line it was given.

The fix is the size guard every sort in CT 15 and Lab 6 now opens with:

```cpp
if (data.size() <= 1) return;
```

## How It Measures

- **Comparisons and time are measured in separate runs.** Each variant is a
  template over its comparator: a counting comparator for the counting run, a
  plain `a < b` for the timed run. A counter inside a timed loop is a memory
  write per comparison and would distort exactly the number being measured.
  This is instructor demo code — the `counted_*` helpers were deliberately
  removed from CT 13, CT 14 and Lab 6, and should not come back.
- **Sizes run to 50M ints (200 MB)** so the data cannot sit in cache. Below
  that, "walk it twice" is nearly free and the interesting effect disappears.
  Check available memory before going higher.
- **Timing runs are interleaved**, repeat outer and variant inner, rotating
  which variant goes first. Running all repeats of one variant before starting
  the next charges any slow stretch of the machine to whichever variant was
  unlucky — doing it that way moved one median by 2.7x between two runs of the
  same binary.
- **A warm-up pass is untimed**, so no variant is charged for first-touch page
  faults on a freshly built 200 MB vector.
- **The results are kept in use** via a checksum printed at the end, so the
  optimizer cannot delete the work being timed.
- **Linearity is reported per element, not as a growth ratio.** A ratio divides
  two noisy measurements together. The demo also prints the slowest/fastest
  spread of its own runs — typically 1.5x to 3x on a shared machine — so no one
  reads a single microsecond column as precise.

## Output

- `results_minmax.csv` — `operation,variant,input,n,comparisons,time_us`.
  Counting rows carry a comparison count and no time; timing rows the reverse.
  The variant field is quoted, because several variant names contain a comma.
- `charts_minmax.html` — two Plotly charts. Comparisons vs n, where the 2n and
  1.5n lines separate and stay separate; and time vs n, where every line is
  straight and the order is *not* the order of the first chart.
