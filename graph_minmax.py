#!/usr/bin/env python3
"""Charts for the min/max demo: comparisons that separate, times that do not.

    python3 graph_minmax.py                # build, run, then chart
    python3 graph_minmax.py --graph-only   # chart results_minmax.csv as it is

Two charts, because the whole point is that they disagree:

  1. comparisons vs n -- the 2n and 1.5n lines pull apart and stay apart. This
     is the clean, machine-independent result.
  2. time vs n -- every line is straight, and the ORDER of the lines does not
     match chart 1. The variant doing the fewest comparisons is not the fastest.

Colours come from the accessible palette in CLAUDE.md. Every one of them is
verified at WCAG AA against white, which is why the plot background is white
and not a theme.
"""
import csv
import subprocess
import sys
from pathlib import Path

try:
    import plotly.graph_objects as go
except ImportError:
    print("plotly not found -- installing...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "plotly"])
    import plotly.graph_objects as go

REPO_DIR = Path(__file__).resolve().parent
CSV_FILE = REPO_DIR / "results_minmax.csv"
OUTPUT_HTML = REPO_DIR / "charts_minmax.html"
EXE_NAME = "big-o-demo-sorting-minmax"

# CLAUDE.md's accessible palette. Each is >= 4.5:1 on white both ways.
INK = "#2c3e50"
MUTED = "#6e6e6e"
RULE = "#d8dee6"
COLORS = {
    "A two passes":        "#c0392b",   # red     5.44:1
    "B one pass, two ifs": "#1c7e45",   # green   5.10:1
    "B' one pass, else if": "#2573a7",  # blue    5.14:1
    "C pairwise by hand":  "#8e44ad",   # purple  5.87:1
    "C' minmax_element":   "#00695c",   # teal    6.61:1
}
# Dash keeps the two ~1.5n lines apart where they sit on top of each other.
DASH = {"C pairwise by hand": "dash", "C' minmax_element": "dot"}
ORDER = list(COLORS.keys())


def find_executable():
    for pattern in (EXE_NAME, EXE_NAME + ".exe"):
        for path in REPO_DIR.rglob(pattern):
            if path.is_file():
                return path
    return None


def build_and_run():
    build_dir = REPO_DIR / "build"
    exe = find_executable()
    if exe is None:
        print("building...")
        subprocess.run(["cmake", "-B", str(build_dir), str(REPO_DIR)], check=True)
        subprocess.run(["cmake", "--build", str(build_dir), "--target", EXE_NAME],
                       check=True)
        exe = find_executable()
    if exe is None:
        sys.exit("ERROR: could not find %s after building" % EXE_NAME)
    print("running %s -- the 50M cases take a minute..." % exe.name)
    subprocess.run([str(exe)], cwd=str(REPO_DIR), check=True)


def read_results():
    """-> (comparison rows, timing rows), each keyed by variant."""
    if not CSV_FILE.exists():
        sys.exit("ERROR: %s not found -- run without --graph-only first" % CSV_FILE)
    counts, times = {}, {}
    with open(CSV_FILE, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            if row["input"] != "random":
                continue           # both charts show random; the CSV keeps all three
            variant, n = row["variant"], int(row["n"])
            if row["comparisons"]:
                counts.setdefault(variant, []).append((n, int(row["comparisons"])))
            elif row["time_us"]:
                times.setdefault(variant, []).append((n, float(row["time_us"])))
    for d in (counts, times):
        for v in d:
            d[v].sort()
    return counts, times


def style(fig, title, subtitle, yaxis):
    fig.update_layout(
        title=dict(text="<b>%s</b><br><span style='font-size:13px;color:%s'>%s</span>"
                        % (title, MUTED, subtitle),
                   font=dict(size=19, color=INK), x=0.02, xanchor="left"),
        template="plotly_white",
        paper_bgcolor="#ffffff",
        plot_bgcolor="#ffffff",
        font=dict(family="Segoe UI, system-ui, sans-serif", size=14, color=INK),
        xaxis=dict(title="n (elements)", type="log", gridcolor=RULE,
                   linecolor=RULE, tickfont=dict(color=INK)),
        yaxis=dict(title=yaxis, type="log", gridcolor=RULE,
                   linecolor=RULE, tickfont=dict(color=INK)),
        legend=dict(bgcolor="#ffffff", bordercolor=RULE, borderwidth=1,
                    font=dict(color=INK)),
        margin=dict(l=70, r=30, t=90, b=60),
        height=520,
    )
    return fig


def comparisons_chart(counts):
    fig = go.Figure()
    for variant in ORDER:
        if variant not in counts:
            continue
        ns = [n for n, _ in counts[variant]]
        ys = [c for _, c in counts[variant]]
        per = ys[-1] / ns[-1]
        fig.add_trace(go.Scatter(
            x=ns, y=ys, mode="lines+markers",
            name="%s  (%.2f per element)" % (variant, per),
            line=dict(color=COLORS[variant], width=3,
                      dash=DASH.get(variant, "solid")),
            marker=dict(size=8, color=COLORS[variant]),
            hovertemplate="%s<br>n=%%{x:,}<br>%%{y:,} comparisons<extra></extra>" % variant,
        ))
    return style(fig, "Comparisons: the 2n and 1.5n lines separate and stay separate",
                 "Counted exactly, with a counting comparator. Random input. "
                 "This result is the same on every machine.",
                 "comparisons")


def time_chart(times):
    fig = go.Figure()
    for variant in ORDER:
        if variant not in times:
            continue
        ns = [n for n, _ in times[variant]]
        ys = [t for _, t in times[variant]]
        fig.add_trace(go.Scatter(
            x=ns, y=ys, mode="lines+markers", name=variant,
            line=dict(color=COLORS[variant], width=3,
                      dash=DASH.get(variant, "solid")),
            marker=dict(size=8, color=COLORS[variant]),
            hovertemplate="%s<br>n=%%{x:,}<br>%%{y:,.0f} us<extra></extra>" % variant,
        ))
    return style(fig, "Time: every line is straight, and the order is not chart 1's order",
                 "Median of 11 interleaved runs, random input. Straight on log-log "
                 "means linear. Note which variant is on top.",
                 "time (microseconds)")


PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Big O: Finding Min and Max</title>
<style>
  body {{ background:#ffffff; color:{ink}; margin:0;
         font-family:"Segoe UI", system-ui, sans-serif; line-height:1.6; }}
  .wrap {{ max-width:68rem; margin:0 auto; padding:2rem 1.25rem 4rem; }}
  h1 {{ font-size:1.6rem; margin:0 0 .4rem; }}
  .sub {{ color:{muted}; margin:0 0 1.6rem; }}
  .card {{ border:1px solid {rule}; border-radius:10px; padding:.5rem;
           margin-bottom:1.6rem; }}
  .lesson {{ border-left:5px solid {deep}; background:#ede7f6; border-radius:0 8px 8px 0;
             padding:1rem 1.25rem; margin:0 0 1.6rem; }}
  .lesson h2 {{ font-size:1.1rem; margin:0 0 .5rem; color:{deep}; }}
  .lesson p {{ margin:0 0 .6rem; }}
  .lesson p:last-child {{ margin:0; }}
  code {{ font-family:Consolas, monospace; font-size:.92em;
          background:#eef2f6; padding:.1em .35em; border-radius:4px; }}
</style>
</head>
<body>
<div class="wrap">
  <h1>Finding min and max: two passes, one pass, or pairs?</h1>
  <p class="sub">CS 2420 &middot; the question a student asked about step 1a of bucket sort</p>

  <div class="lesson">
    <h2>Read the two charts against each other</h2>
    <p>The first chart is arithmetic: the pairwise variants do 1.5 comparisons per
    element where the obvious loop does 2. A quarter fewer, on every machine, forever.</p>
    <p>The second chart is this machine. Every line is straight &mdash; all five are
    O(n) &mdash; but the variant with the fewest comparisons is <strong>not</strong> the
    fastest. <code>B</code>'s two <code>if</code>s compile to branchless
    <code>cmov</code>; the pairwise loop needs five conditional jumps per pair and
    mispredicts on random data.</p>
    <p><strong>A comparison in source code is not a machine instruction.</strong>
    Counting the one is not measuring the other, and Big O is the same for all of them.</p>
  </div>

  <div class="card">{chart1}</div>
  <div class="card">{chart2}</div>
</div>
</body>
</html>
"""


def generate_html(counts, times):
    c1 = comparisons_chart(counts).to_html(full_html=False, include_plotlyjs=True)
    c2 = time_chart(times).to_html(full_html=False, include_plotlyjs=False)
    OUTPUT_HTML.write_text(
        PAGE.format(ink=INK, muted=MUTED, rule=RULE, deep="#4527a0",
                    chart1=c1, chart2=c2),
        encoding="utf-8")
    print("wrote %s" % OUTPUT_HTML)


if __name__ == "__main__":
    if "--graph-only" not in sys.argv:
        build_and_run()
    counts, times = read_results()
    if not counts or not times:
        sys.exit("ERROR: %s has no random-input rows to chart" % CSV_FILE)
    print("charting %d variants of comparisons, %d of timings"
          % (len(counts), len(times)))
    generate_html(counts, times)
