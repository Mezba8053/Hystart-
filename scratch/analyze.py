#!/usr/bin/env python3
"""
analyze.py — HyStart++ vs Standard Slow Start: Analysis & Visualization
=========================================================================
Runs both TCP variants, parses their structured output, prints a
comparative statistical report, then generates a six-panel figure.

Usage:
    python3 analyze.py                          # uses defaults
    python3 analyze.py --skip-sim               # re-plot from existing .dat files
    python3 analyze.py --bw 2Mbps --delay 30ms

Requirements:  matplotlib, numpy  (pip install matplotlib numpy)
"""

import argparse
import os
import re
import subprocess
import sys
import textwrap
from collections import defaultdict
from dataclasses import dataclass, field
from typing import List, Tuple, Dict, Optional

import numpy as np
import matplotlib
matplotlib.use("Agg")           # headless – remove if you want an interactive window
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import matplotlib.patches as mpatches
from matplotlib.lines import Line2D

# ─────────────────────────────────────────────────────────────────────────────
#  Configuration
# ─────────────────────────────────────────────────────────────────────────────
NS3_CMD   = "./ns3"                        # path to ns3 wrapper
SCRATCH   = "scratch/hystart++"           # scratch file (no .cc)
OUT_FILE  = "hystart_analysis.png"

VARIANTS  = ["TcpHyStartPlusPlus", "TcpNewReno"]
TAG       = {"TcpHyStartPlusPlus": "hystart", "TcpNewReno": "newreno"}
LABEL     = {"TcpHyStartPlusPlus": "HyStart++", "TcpNewReno": "Standard Slow Start (NewReno)"}
COLOR     = {"TcpHyStartPlusPlus": "#1565C0", "TcpNewReno": "#C62828"}
ALPHA     = {"TcpHyStartPlusPlus": 0.90,       "TcpNewReno": 0.75}

# Phase colours for the phase-band overlay
PHASE_COLOR = {"SS": "#A5D6A7", "CSS": "#FFE082", "CA": "#CE93D8"}
PHASE_LABEL = {"SS":  "Slow Start (SS)",
               "CSS": "Conservative SS (CSS)",
               "CA":  "Congestion Avoidance (CA)"}


# ─────────────────────────────────────────────────────────────────────────────
#  Data containers
# ─────────────────────────────────────────────────────────────────────────────
@dataclass
class SimData:
    variant:   str
    cwnd:      Tuple[List[float], List[float]] = field(default_factory=lambda: ([], []))
    ssthresh:  Tuple[List[float], List[float]] = field(default_factory=lambda: ([], []))
    rtt:       Tuple[List[float], List[float]] = field(default_factory=lambda: ([], []))
    drops:     Tuple[List[float], List[int]]   = field(default_factory=lambda: ([], []))
    phases:    List[dict]                       = field(default_factory=list)   # hystart only
    flow:      dict                             = field(default_factory=dict)
    bdp:       float                            = 0.0
    bdp_overshoot_count: int                   = 0
    loss_times:  List[float]                   = field(default_factory=list)


# ─────────────────────────────────────────────────────────────────────────────
#  Simulation runner
# ─────────────────────────────────────────────────────────────────────────────
def run_simulation(variant: str, args) -> str:
    sim_args = (f"--tcp={variant}"
                f" --bandwidth={args.bw}"
                f" --delay={args.delay}"
                f" --data={args.data}"
                f" --time={args.time}"
                f" --queue={args.queue}")
    cmd = f'{NS3_CMD} run "{SCRATCH} {sim_args}"'
    print(f"  ▶  Running {LABEL[variant]} …")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[WARN] simulation returned non-zero exit code for {variant}")
        print(result.stderr[:600])
    return result.stdout + result.stderr


# ─────────────────────────────────────────────────────────────────────────────
#  Parsers  (work on raw stdout string OR on saved .dat files)
# ─────────────────────────────────────────────────────────────────────────────
def _floats(line: str, pattern: str) -> Optional[tuple]:
    m = re.search(pattern, line)
    return tuple(float(x) for x in m.groups()) if m else None


def parse_output(raw: str, variant: str) -> SimData:
    sd = SimData(variant=variant)
    ct, cv, st, sv, rt, rv, dt, dv = [], [], [], [], [], [], [], []
    lt = []  # loss times

    for line in raw.splitlines():
        # CWND
        m = re.match(r'CWND\s+([\d.]+)\s+\d+\s+(\d+)', line)
        if m:
            ct.append(float(m.group(1))); cv.append(int(m.group(2))); continue

        # SSTHRESH
        m = re.match(r'SSTHRESH\s+([\d.]+)\s+\d+\s+(\d+)', line)
        if m:
            st.append(float(m.group(1))); sv.append(int(m.group(2))); continue

        # RTT
        m = re.match(r'RTT\s+([\d.]+)\s+[\d.]+\s+([\d.]+)', line)
        if m:
            rt.append(float(m.group(1))); rv.append(float(m.group(2))); continue

        # QUEUE_DROP
        m = re.match(r'QUEUE_DROP\s+([\d.]+)\s+(\d+)', line)
        if m:
            dt.append(float(m.group(1))); dv.append(int(m.group(2))); continue

        # HYSTART_PHASE  (HyStart++ only)
        m = re.match(r'HYSTART_PHASE\s+([\d.]+)\s+(\w+)->(\w+)\s+reason=(\S+)'
                     r'.*cwnd=(\d+)', line)
        if m:
            sd.phases.append({
                'time':   float(m.group(1)),
                'from':   m.group(2),
                'to':     m.group(3),
                'reason': m.group(4),
                'cwnd':   int(m.group(5)),
            })
            continue

        # FLOW_STATS
        m = re.match(r'FLOW_STATS\s+tx=(\d+)\s+rx=(\d+)\s+lost=(\d+)'
                     r'\s+lossRate=([\d.]+)%\s+throughput=([\d.]+)Mbps'
                     r'\s+avgDelay=([\d.]+)ms\s+avgJitter=([\d.]+)ms'
                     r'\s+queueDrops=(\d+)\s+bdpOvershoot=(\d+)', line)
        if m:
            sd.flow = {
                'tx':         int(m.group(1)),   'rx':          int(m.group(2)),
                'lost':       int(m.group(3)),   'loss_rate':   float(m.group(4)),
                'throughput': float(m.group(5)), 'avg_delay':   float(m.group(6)),
                'avg_jitter': float(m.group(7)), 'queue_drops': int(m.group(8)),
                'bdp_overshoot': int(m.group(9)),
            }
            sd.bdp_overshoot_count = int(m.group(9))
            continue

        # BDP
        m = re.match(r'BDP_LINE\s+([\d.]+)', line)
        if m:
            sd.bdp = float(m.group(1)); continue

        # Loss events
        m = re.match(r'HYSTART_LOSS\s+([\d.]+)', line)
        if m:
            lt.append(float(m.group(1))); continue

    sd.cwnd     = (ct, cv)
    sd.ssthresh = (st, sv)
    sd.rtt      = (rt, rv)
    sd.drops    = (dt, dv)
    sd.loss_times = lt
    return sd


def load_from_files(variant: str, bdp: float) -> SimData:
    """Reload from the per-variant .dat files written by the C++ sim."""
    tag = TAG[variant]
    sd  = SimData(variant=variant, bdp=bdp)

    def load_xy(path):
        ts, vs = [], []
        if os.path.exists(path):
            for line in open(path):
                parts = line.split()
                if len(parts) >= 2:
                    ts.append(float(parts[0])); vs.append(float(parts[1]))
        return ts, vs

    sd.cwnd    = load_xy(f"cwnd_{tag}.dat")
    sd.rtt     = load_xy(f"rtt_{tag}.dat")
    sd.ssthresh= load_xy(f"ssthresh_{tag}.dat")
    sd.drops   = load_xy(f"drops_{tag}.dat")

    if os.path.exists(f"phases_{tag}.dat"):
        for line in open(f"phases_{tag}.dat"):
            p = line.split()
            if len(p) >= 5:
                sd.phases.append({'time': float(p[0]), 'from': p[1],
                                   'to':   p[2], 'reason': p[3], 'cwnd': int(p[4])})

    if os.path.exists(f"stats_{tag}.dat"):
        for line in open(f"stats_{tag}.dat"):
            nums = line.split()
            if len(nums) >= 9:
                sd.flow = {
                    'throughput':    float(nums[0]), 'loss_rate':  float(nums[1]),
                    'avg_delay':     float(nums[2]), 'avg_jitter': float(nums[3]),
                    'tx':            int(nums[4]),   'rx':         int(nums[5]),
                    'lost':          int(nums[6]),   'queue_drops':int(nums[7]),
                    'bdp_overshoot': int(nums[8]),
                }
    return sd


# ─────────────────────────────────────────────────────────────────────────────
#  Statistical report (printed to terminal)
# ─────────────────────────────────────────────────────────────────────────────
def print_report(data: Dict[str, SimData]):
    H = data["TcpHyStartPlusPlus"]
    N = data["TcpNewReno"]
    w = 72

    def bar(char="─", width=w): return char * width

    def improvement(h_val, n_val, higher_better=True):
        if n_val == 0:
            return "  N/A"
        pct = (h_val - n_val) / abs(n_val) * 100
        if not higher_better:
            pct = -pct
        sign = "+" if pct >= 0 else ""
        marker = "✓" if pct >= 0 else "✗"
        return f"{marker} {sign}{pct:.1f}%"

    print()
    print("╔" + "═" * w + "╗")
    print("║{:^{w}}║".format("  HyStart++ vs Standard Slow Start — Comparative Analysis  ", w=w))
    print("╚" + "═" * w + "╝")

    # ── Network config ──────────────────────────────────────────────────────
    print()
    print(f"  BDP = {H.bdp:,.0f} bytes  ({H.bdp/536:.0f} segments @ 536-byte MSS)")
    print(f"  Phase constants: MIN_RTT_THRESH=4ms  MAX_RTT_THRESH=16ms")
    print(f"                   MIN_RTT_DIVISOR=8   N_RTT_SAMPLE=8")
    print(f"                   CSS_GROWTH_DIVISOR=4   CSS_ROUNDS=5   L=8")

    # ── Metrics table ───────────────────────────────────────────────────────
    print()
    print("  " + bar())
    print(f"  {'Metric':<28} {'HyStart++':>14} {'NewReno':>14} {'Δ (HyStart++ vs NR)':>18}")
    print("  " + bar())

    rows = [
        ("Throughput (Mbps)",     "throughput",    True),
        ("Packet Loss Rate (%)",  "loss_rate",     False),
        ("Avg RTT Delay (ms)",    "avg_delay",     False),
        ("Avg Jitter (ms)",       "avg_jitter",    False),
        ("Packets Sent",          "tx",            None),
        ("Packets Received",      "rx",            True),
        ("Packets Lost",          "lost",          False),
        ("Queue Drop Events",     "queue_drops",   False),
        ("BDP Overshoot Events",  "bdp_overshoot", False),
    ]

    for name, key, hb in rows:
        hv = H.flow.get(key, 0)
        nv = N.flow.get(key, 0)
        imp = improvement(hv, nv, hb) if hb is not None else ""
        print(f"  {name:<28} {str(hv):>14} {str(nv):>14} {imp:>18}")

    # Extra computed metrics
    h_drops_count = len(H.drops[0])
    n_drops_count = len(N.drops[0])
    imp = improvement(h_drops_count, n_drops_count, higher_better=False) \
          if n_drops_count else "  N/A"
    print(f"  {'Drop Event Count':<28} {str(h_drops_count):>14} {str(n_drops_count):>14} {imp:>18}")

    if H.rtt[1]:
        h_rtt_max = max(H.rtt[1])
        n_rtt_max = max(N.rtt[1]) if N.rtt[1] else 0
        imp = improvement(h_rtt_max, n_rtt_max, higher_better=False) if n_rtt_max else "  N/A"
        print(f"  {'Peak RTT (ms)':<28} {str(h_rtt_max):>14} {str(n_rtt_max):>14} {imp:>18}")

    if H.cwnd[1]:
        h_cwnd_max = max(H.cwnd[1]) // 1024
        n_cwnd_max = max(N.cwnd[1]) // 1024 if N.cwnd[1] else 0
        imp = improvement(h_cwnd_max, n_cwnd_max, higher_better=False) if n_cwnd_max else "  N/A"
        print(f"  {'Peak cwnd (KB)':<28} {str(h_cwnd_max):>14} {str(n_cwnd_max):>14} {imp:>18}")

    print("  " + bar())

    # ── HyStart++ phase transition log ──────────────────────────────────────
    print()
    print("  ── HyStart++ Phase Transitions ─────────────────────────────────")
    if H.phases:
        for p in H.phases:
            cwnd_kb  = p['cwnd'] / 1024
            cwnd_seg = p['cwnd'] / 536
            tag_str  = f"{p['from']:>3} → {p['to']:<3}"
            print(f"    t={p['time']:6.3f}s  {tag_str}  reason={p['reason']:<28}"
                  f"  cwnd={cwnd_kb:.1f} KB ({cwnd_seg:.1f} segs)")
    else:
        print("    (no transitions recorded — check phase log)")

    # ── Problems with standard slow start ───────────────────────────────────
    print()
    print("  ── Problems HyStart++ Solves vs Standard Slow Start ────────────")
    problems = [
        ("Cwnd overshoot (BDP exceedance)",
         "SS doubles cwnd past BDP before loss signals → queue buildup.",
         f"H: {H.bdp_overshoot_count} events   NR: {N.bdp_overshoot_count} events"),
        ("Packet loss rate",
         "Loss in SS triggers cwnd/2, wasting bandwidth & time.",
         f"H: {H.flow.get('loss_rate',0):.2f}%   NR: {N.flow.get('loss_rate',0):.2f}%"),
        ("Queue drop events",
         "SS fills and overflows the router queue before backing off.",
         f"H: {len(H.drops[0])} drops   NR: {len(N.drops[0])} drops"),
        ("Peak RTT inflation",
         "Queued packets inflate RTT for all flows sharing the link.",
         f"H peak: {max(H.rtt[1]) if H.rtt[1] else 0:.1f}ms   "
         f"NR peak: {max(N.rtt[1]) if N.rtt[1] else 0:.1f}ms"),
        ("Throughput gap",
         "Loss + recovery cycles leave the link under-utilised in NR.",
         f"H: {H.flow.get('throughput',0):.3f} Mbps   NR: {N.flow.get('throughput',0):.3f} Mbps"),
    ]
    for title, desc, metric in problems:
        print(f"\n    ● {title}")
        print(f"      {desc}")
        print(f"      Measured → {metric}")

    print()
    print("  " + bar("═"))
    print()


# ─────────────────────────────────────────────────────────────────────────────
#  Phase-band helper (draws coloured backgrounds on an axis)
# ─────────────────────────────────────────────────────────────────────────────
def draw_phase_bands(ax, phases: List[dict], sim_end: float):
    """Shade axis background with SS/CSS/CA colours based on phase log."""
    if not phases:
        return
    # Build (start, end, phase_name) intervals
    intervals = []
    # Everything before the first transition is SS
    if phases[0]['time'] > 1.0:
        intervals.append((1.0, phases[0]['time'], phases[0]['from']))
    for i, p in enumerate(phases):
        end = phases[i+1]['time'] if i+1 < len(phases) else sim_end
        intervals.append((p['time'], end, p['to']))

    for start, end, ph in intervals:
        ax.axvspan(start, end,
                   color=PHASE_COLOR.get(ph, "#EEEEEE"),
                   alpha=0.18, zorder=0)


# ─────────────────────────────────────────────────────────────────────────────
#  Main plotting function
# ─────────────────────────────────────────────────────────────────────────────
def make_plots(data: Dict[str, SimData], sim_time: float):
    H = data["TcpHyStartPlusPlus"]
    N = data["TcpNewReno"]

    fig = plt.figure(figsize=(18, 15))
    fig.patch.set_facecolor("#F7F9FC")
    fig.suptitle("HyStart++ (RFC 9406) vs Standard Slow Start — Comparative Analysis",
                 fontsize=15, fontweight="bold", y=0.98, color="#1A1A2E")

    gs = gridspec.GridSpec(3, 2, figure=fig, hspace=0.50, wspace=0.32,
                           top=0.93, bottom=0.06, left=0.07, right=0.97)

    # ── shared legend handles ────────────────────────────────────────────────
    legend_handles = [
        Line2D([0],[0], color=COLOR["TcpHyStartPlusPlus"], lw=2.0, label=LABEL["TcpHyStartPlusPlus"]),
        Line2D([0],[0], color=COLOR["TcpNewReno"],          lw=2.0, label=LABEL["TcpNewReno"]),
        Line2D([0],[0], color="gray", lw=1.5, linestyle="--", label="BDP"),
    ]
    phase_handles = [mpatches.Patch(color=c, alpha=0.4, label=PHASE_LABEL[k])
                     for k, c in PHASE_COLOR.items()]

    # ────────────────────────────────────────────────────────────────────────
    #  Panel 1 (top row, full width): cwnd + ssthresh
    # ────────────────────────────────────────────────────────────────────────
    ax1 = fig.add_subplot(gs[0, :])
    ax1.set_facecolor("#FAFBFF")
    draw_phase_bands(ax1, H.phases, sim_time)

    for v, sd in data.items():
        t, c = sd.cwnd
        ax1.plot(t, [x / 1024 for x in c],
                 color=COLOR[v], lw=1.8, alpha=ALPHA[v],
                 label=LABEL[v], zorder=3)

        t2, s2 = sd.ssthresh
        ax1.plot(t2, [x / 1024 for x in s2],
                 color=COLOR[v], lw=1.0, linestyle=":", alpha=0.5, zorder=2)

    bdp_kb = H.bdp / 1024
    ax1.axhline(y=bdp_kb, color="gray", linestyle="--", lw=1.5,
                label=f"BDP ≈ {bdp_kb:.0f} KB", zorder=2)

    # Mark HyStart++ loss events
    for lt in H.loss_times:
        ax1.axvline(x=lt, color="#E53935", lw=0.8, alpha=0.6, zorder=4)

    ax1.set_xlabel("Time (s)", fontsize=10)
    ax1.set_ylabel("Congestion Window (KB)", fontsize=10)
    ax1.set_title("Congestion Window & ssthresh Over Time  (dotted = ssthresh)", fontsize=11)
    ax1.legend(handles=legend_handles + phase_handles,
               loc="upper right", fontsize=8, ncol=3, framealpha=0.9)
    ax1.grid(True, alpha=0.25)
    ax1.set_xlim(1.0, sim_time)

    # ────────────────────────────────────────────────────────────────────────
    #  Panel 2 (middle-left): RTT
    # ────────────────────────────────────────────────────────────────────────
    ax2 = fig.add_subplot(gs[1, 0])
    ax2.set_facecolor("#FAFBFF")
    draw_phase_bands(ax2, H.phases, sim_time)

    for v, sd in data.items():
        t, r = sd.rtt
        if t:
            # Smooth for readability
            rr = np.array(r)
            ax2.plot(t, rr, color=COLOR[v], lw=1.5, alpha=ALPHA[v], label=LABEL[v])

    ax2.set_xlabel("Time (s)", fontsize=10)
    ax2.set_ylabel("RTT (ms)", fontsize=10)
    ax2.set_title("RTT Over Time\n"
                  "(lower peak → less queue buildup → HyStart++ advantage)", fontsize=10)
    ax2.legend(fontsize=8)
    ax2.grid(True, alpha=0.25)
    ax2.set_xlim(1.0, sim_time)

    # ────────────────────────────────────────────────────────────────────────
    #  Panel 3 (middle-right): Queue drops histogram
    # ────────────────────────────────────────────────────────────────────────
    ax3 = fig.add_subplot(gs[1, 1])
    ax3.set_facecolor("#FAFBFF")

    bins = np.arange(1.0, sim_time + 1.0, 1.0)
    for v, sd in data.items():
        dt, _ = sd.drops
        if dt:
            counts, edges = np.histogram(dt, bins=bins)
            ax3.bar(edges[:-1] + 0.2, counts, width=0.4 if v == VARIANTS[0] else 0.4,
                    align="edge", color=COLOR[v], alpha=0.75, label=LABEL[v])

    total_h = len(H.drops[0])
    total_n = len(N.drops[0])
    ax3.set_xlabel("Time (s)", fontsize=10)
    ax3.set_ylabel("Queue Drop Events", fontsize=10)
    ax3.set_title(f"Queue Drops per Second\n"
                  f"Total  HyStart++: {total_h}  |  NewReno: {total_n}", fontsize=10)
    ax3.legend(fontsize=8)
    ax3.grid(True, alpha=0.25, axis="y")
    ax3.set_xlim(1.0, sim_time)

    # ────────────────────────────────────────────────────────────────────────
    #  Panel 4 (bottom-left): Key metrics bar chart
    # ────────────────────────────────────────────────────────────────────────
    ax4 = fig.add_subplot(gs[2, 0])
    ax4.set_facecolor("#FAFBFF")

    metric_keys   = ["throughput", "loss_rate", "avg_delay", "avg_jitter"]
    metric_labels = ["Throughput\n(Mbps)", "Loss Rate\n(%)",
                     "Avg Delay\n(ms)", "Avg Jitter\n(ms)"]
    x = np.arange(len(metric_keys))
    w = 0.35

    h_vals = [H.flow.get(k, 0) for k in metric_keys]
    n_vals = [N.flow.get(k, 0) for k in metric_keys]

    bars_h = ax4.bar(x - w/2, h_vals, w, color=COLOR["TcpHyStartPlusPlus"],
                     alpha=0.85, label="HyStart++")
    bars_n = ax4.bar(x + w/2, n_vals, w, color=COLOR["TcpNewReno"],
                     alpha=0.85, label="NewReno")

    # Value labels on top of bars
    for bar, val in zip(list(bars_h) + list(bars_n), h_vals + n_vals):
        ax4.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.01 * max(h_vals+n_vals),
                 f"{val:.2f}", ha="center", va="bottom", fontsize=7.5, color="#333333")

    ax4.set_xticks(x)
    ax4.set_xticklabels(metric_labels, fontsize=9)
    ax4.set_title("Key Performance Metrics Comparison", fontsize=11)
    ax4.legend(fontsize=9)
    ax4.grid(True, alpha=0.25, axis="y")
    ax4.set_ylabel("Value", fontsize=10)

    # ────────────────────────────────────────────────────────────────────────
    #  Panel 5 (bottom-right): HyStart++ phase timeline
    # ────────────────────────────────────────────────────────────────────────
    ax5 = fig.add_subplot(gs[2, 1])
    ax5.set_facecolor("#FAFBFF")

    if H.phases:
        # Build phase intervals
        intervals = []
        start0 = 1.0
        if H.phases[0]['time'] > start0:
            intervals.append((start0, H.phases[0]['time'], H.phases[0]['from']))
        for i, p in enumerate(H.phases):
            end = H.phases[i+1]['time'] if i+1 < len(H.phases) else sim_time
            intervals.append((p['time'], end, p['to']))

        y_map   = {"SS": 2, "CSS": 1, "CA": 0}
        colours = {"SS": PHASE_COLOR["SS"], "CSS": PHASE_COLOR["CSS"], "CA": PHASE_COLOR["CA"]}

        for (s, e, ph) in intervals:
            ax5.barh(y=y_map.get(ph, 0), width=e-s, left=s, height=0.6,
                     color=colours.get(ph, "gray"), alpha=0.85, edgecolor="white")
            if (e - s) > 0.5:
                ax5.text((s+e)/2, y_map.get(ph, 0), ph,
                         ha="center", va="center", fontsize=9, fontweight="bold")

        # Annotate transitions
        for p in H.phases:
            yy = y_map.get(p['to'], 0)
            ax5.annotate(f"{p['time']:.1f}s\n{p['reason'].replace('_',' ')}",
                         xy=(p['time'], yy),
                         xytext=(p['time'] + 0.3, yy + 0.4),
                         fontsize=7, color="#333",
                         arrowprops=dict(arrowstyle="->", color="#555", lw=0.8))

        ax5.set_yticks([0, 1, 2])
        ax5.set_yticklabels(["Congestion\nAvoidance", "Conservative\nSlow Start",
                              "Slow\nStart"], fontsize=8)
        ax5.set_xlim(1.0, sim_time)
        ax5.set_xlabel("Time (s)", fontsize=10)
        ax5.set_title("HyStart++ Phase Timeline", fontsize=11)
        ax5.grid(True, alpha=0.2, axis="x")
    else:
        ax5.text(0.5, 0.5, "No phase transition data\n(run with TcpHyStartPlusPlus)",
                 ha="center", va="center", transform=ax5.transAxes,
                 fontsize=11, color="gray")
        ax5.set_title("HyStart++ Phase Timeline", fontsize=11)

    # ────────────────────────────────────────────────────────────────────────
    #  Save
    # ────────────────────────────────────────────────────────────────────────
    plt.savefig(OUT_FILE, dpi=150, bbox_inches="tight", facecolor=fig.get_facecolor())
    print(f"\n  ✔  Figure saved → {OUT_FILE}")


# ─────────────────────────────────────────────────────────────────────────────
#  Entry point
# ─────────────────────────────────────────────────────────────────────────────
def parse_args():
    p = argparse.ArgumentParser(description="HyStart++ Analysis & Visualization")
    p.add_argument("--skip-sim",   action="store_true",
                   help="Skip running NS-3; reload from existing .dat files")
    p.add_argument("--bw",         default="1Mbps",     help="Bottleneck bandwidth")
    p.add_argument("--delay",      default="50ms",      help="One-way bottleneck delay")
    p.add_argument("--data",       default="10000000",  help="Data size (bytes)")
    p.add_argument("--time",       default="30",        help="Simulation time (s)")
    p.add_argument("--queue",      default="50",        help="Queue size (packets)")
    return p.parse_args()


def main():
    args    = parse_args()
    sim_end = float(args.time)
    data    = {}

    if args.skip_sim:
        print("  ↩  Loading from existing .dat files …")
        # We still need BDP — estimate it from args
        import re as _re
        bw_bits = float(_re.sub(r'[^0-9.]', '', args.bw)) * (
            1e6 if 'M' in args.bw.upper() else 1e3 if 'K' in args.bw.upper() else 1)
        d_s = float(_re.sub(r'[^0-9.]', '', args.delay)) * (
            1e-3 if 'ms' in args.delay.lower() else 1.0)
        bdp = bw_bits * 2 * (0.005 + d_s) / 8
        for v in VARIANTS:
            data[v] = load_from_files(v, bdp)
    else:
        print("\n  Running simulations (this may take a minute) …")
        raw_outputs = {}
        for v in VARIANTS:
            raw_outputs[v] = run_simulation(v, args)
        for v in VARIANTS:
            data[v] = parse_output(raw_outputs[v], v)
        # Propagate BDP from HyStart++ run to NewReno for shared reference
        if data["TcpHyStartPlusPlus"].bdp > 0 and data["TcpNewReno"].bdp == 0:
            data["TcpNewReno"].bdp = data["TcpHyStartPlusPlus"].bdp

    print_report(data)
    make_plots(data, sim_end)


if __name__ == "__main__":
    main()