#!/usr/bin/env python3
"""
Plot average time‑to‑crash for Heap and Kernel tests.

Usage:
    python3 plot_results.py [csv_file]

If no CSV is given, defaults to 'mem_crash_results.csv' in the CWD.
Creates 'mem_crash_plot.png'.
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt

fname = sys.argv[1] if len(sys.argv) > 1 else "mem_crash_results.csv"
df = pd.read_csv(fname)

# Aggregate
avg = df.groupby("Test")["Time_ns"].mean().reset_index()

plt.figure()
plt.bar(avg["Test"], avg["Time_ns"])
plt.ylabel("Average time to SIGSEGV (ns)")
plt.title("Heap‑overflow vs Kernel‑access crash latency")
plt.tight_layout()
out_name = "mem_crash_plot.png"
plt.savefig(out_name, dpi=180)
print(f"[+] plot written → {out_name}")
