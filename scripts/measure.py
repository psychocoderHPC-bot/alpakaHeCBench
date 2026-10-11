#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#
# measure.py - cross-opt measurement harness (RECONSTRUCTED).
#
# This file is reconstructed from the campaign evidence, not copied from the
# original host copy (which was not reachable from the PR container):
#   results/data/campaign2_exact_commands.log
#   results/data/campaign2_applied_diffs.diff   (the F1/F2 fixes)
#   results/data/campaign2.csv                   (the 13-column output shape)
#   evidence/PROTOCOL_REPORT.md
#
# STATUS: reconstructed-to-be-verified-on-hal. The command construction and the
# 13-column CSV schema follow the recorded contract. Exact stdout parsing was
# inferred from the binary output contract documented in
# src/cross-alpaka/README.md and from the campaign command lines. Re-run one
# known cell against the original harness before trusting a new campaign.
#
# Contract encoded here:
#   F1  The positional <size> <repeat> are ALWAYS passed. Any --prog-args are
#       APPENDED after them and never replace them. The former bug replaced the
#       positionals whenever --prog-args was set, which silently dropped size
#       and repeat for every Intel SYCL row (fixed in campaign2).
#   F1b --prog-args-mode extended builds
#       <binary> --size <n> --repeat <r> <prog-args>; required when named flags
#       are combined with a size (alpaka extended interface, --dtype float).
#   F2  The warm-up is an in-process discarded enqueue in the candidate binary
#       (see campaign2_applied_diffs.diff); --warmup here can additionally drop
#       whole process runs before timing. warmup=1 in the campaign.
#   Serial scheduling: a flock on the output CSV guarantees one timed process at
#       a time even when several harness invocations are launched concurrently.
#
# 13-column CSV schema (campaign2.csv):
#   target,impl,dtype,size,repeat,median_us,min_us,max_us,stddev_us,wall_s,
#   pass/fail,binary_path,command
#
# "impl" carries the kernel role tag used to select the row, e.g.
#   cand-c3-cpu-icpx[crossSoA]. C3 measures its crossSoA kernel and C4 its
#   cross4 kernel; the flat [cross3] siblings are diagnostics and are excluded.

import argparse
import csv
import fcntl
import os
import re
import shlex
import statistics
import subprocess
import sys
import time

CSV_COLUMNS = [
    "target",
    "impl",
    "dtype",
    "size",
    "repeat",
    "median_us",
    "min_us",
    "max_us",
    "stddev_us",
    "wall_s",
    "pass/fail",
    "binary_path",
    "command",
]

# "Average execution time of cross3 kernel: 22.676 (us)"
KERNEL_RE = re.compile(r"Average execution time of (cross\w*) kernel:\s*([0-9.eE+-]+)\s*\(us\)")
# "=========== Data type is FP32 =========="
SECTION_RE = re.compile(r"Data type is (FP32|FP64)")
PASS_RE = re.compile(r"^(PASS|FAIL)\s*$")


def parse_output(text, dtype):
    """Return (kernel_times, passed) for the requested dtype.

    dtype is 'FP32' or 'FP64'. A native binary runs FP32 then FP64 in one
    process, so the section header selects the right block. An extended
    single-dtype run prints one block with no ambiguity.
    """
    section = None
    times = {}
    passed = None
    for line in text.splitlines():
        m = SECTION_RE.search(line)
        if m:
            section = m.group(1)
            continue
        m = KERNEL_RE.search(line)
        if m:
            # With no section header (single-dtype extended run) accept all.
            if section is None or section == dtype:
                times[m.group(1)] = float(m.group(2))
            continue
        m = PASS_RE.match(line.strip())
        if m:
            if section is None or section == dtype:
                passed = m.group(1) == "PASS"
    return times, passed


def build_command(args):
    binary = os.path.abspath(args.binary)
    if args.prog_args_mode == "extended":
        cmd = [binary, "--size", str(args.size), "--repeat", str(args.repeat)]
    else:  # append: native positional contract
        cmd = [binary, str(args.size), str(args.repeat)]
    if args.prog_args:
        cmd += shlex.split(args.prog_args)
    if args.taskset:
        cmd = ["taskset", "-c", str(args.taskset)] + cmd
    return cmd


def run_once(cmd, env, cwd):
    start = time.monotonic()
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    wall = time.monotonic() - start
    return proc, wall


def main():
    ap = argparse.ArgumentParser(description="cross-opt measurement harness (reconstructed).")
    ap.add_argument("--binary", required=True, help="benchmark binary to measure")
    ap.add_argument("--size", required=True, type=int, help="number of rows")
    ap.add_argument("--repeat", default=100, type=int, help="inner repeat count")
    ap.add_argument("--dtype", choices=["float", "double", "both"], default="both",
                    help="dtype to measure; 'both' measures FP32 and FP64 in one native run")
    ap.add_argument("--role", default=None,
                    help="kernel tag to select, e.g. cross3 or crossSoA; default: the "
                         "highest-numbered cross kernel present")
    ap.add_argument("--impl", default=None,
                    help="implementation label written to the CSV 'impl' column")
    ap.add_argument("--target", default=os.environ.get("HARNESS_TARGET", "hal"))
    ap.add_argument("--reps", default=5, type=int, help="timed process repetitions; median reported")
    ap.add_argument("--warmup", default=1, type=int,
                    help="discarded process runs before timing (campaign used 1)")
    ap.add_argument("--output", default="results.csv", help="CSV to append to")
    ap.add_argument("--cwd", default=None)
    ap.add_argument("--taskset", default=None, help="pin with taskset -c <cpus>")
    ap.add_argument("--prog-args", default="",
                    help="extra program args (shlex-split), APPENDED after the positional "
                         "<size> <repeat>; never replaces them")
    ap.add_argument("--prog-args-mode", choices=["append", "extended"], default="append",
                    help="append: <binary> <size> <repeat> <prog-args> (native+flags); "
                         "extended: <binary> --size <size> --repeat <repeat> <prog-args> "
                         "(alpaka extended interface, required when named flags are "
                         "combined with a size)")
    ap.add_argument("--single-threaded-scheduler", action="store_true", default=True,
                    help="assert serial scheduling (default ON)")
    ap.add_argument("--no-single-threaded-scheduler", dest="single_threaded_scheduler",
                    action="store_false")
    ap.add_argument("--env", action="append", default=[],
                    help="KEY=VALUE, repeatable; added to the child environment")
    args = ap.parse_args()

    if args.single_threaded_scheduler:
        pass  # serial scheduling is enforced by the flock below

    env = dict(os.environ)
    for item in args.env:
        k, _, v = item.partition("=")
        env[k] = v

    down = {"float": "FP32", "double": "FP64"}
    dtypes = ["FP32", "FP64"] if args.dtype == "both" else [down[args.dtype]]

    cmd = build_command(args)
    # Match the real campaign harness: the CSV command string carries the
    # OMP_NUM_THREADS prefix when that variable is exported for the child
    # (campaign2.csv: "OMP_NUM_THREADS=64 <bin> <size> <repeat>").
    env_prefix = ""
    if "OMP_NUM_THREADS" in env:
        env_prefix = "OMP_NUM_THREADS=%s " % env["OMP_NUM_THREADS"]
    cmd_str = env_prefix + " ".join(cmd)

    # --- serial scheduling: hold an exclusive lock for the whole measurement ---
    os.makedirs(os.path.dirname(os.path.abspath(args.output)) or ".", exist_ok=True)
    lock_path = args.output + ".lock"
    with open(lock_path, "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)

        for _ in range(args.warmup):
            run_once(cmd, env, args.cwd)

        per_dtype = {dt: [] for dt in dtypes}
        wall_list = []
        pass_all = True
        last_text = ""
        for _ in range(args.reps):
            proc, wall = run_once(cmd, env, args.cwd)
            wall_list.append(wall)
            last_text = proc.stdout
            if proc.returncode != 0 and args.dtype != "both":
                # Native FP64-only runs abort on FP64-less devices; record FAIL.
                pass_all = False
            for dt in dtypes:
                times, passed = parse_output(proc.stdout, dt)
                if passed is None:
                    pass_all = pass_all and False
                elif not passed:
                    pass_all = False
                if times:
                    per_dtype[dt].append(select_kernel(times, args.role))

        lock_dir = os.path.dirname(os.path.abspath(args.output)) or "."
        write_header = not os.path.exists(args.output) or os.path.getsize(args.output) == 0
        with open(args.output, "a", newline="") as fh:
            writer = csv.writer(fh)
            if write_header:
                writer.writerow(CSV_COLUMNS)
            for dt in dtypes:
                samples = [s for s in per_dtype[dt] if s is not None]
                if not samples:
                    writer.writerow([
                        args.target, label(args, dt), dt, args.size, args.repeat,
                        "", "", "", "", f"{statistics.fmean(wall_list):.6f}",
                        "FAIL", os.path.abspath(args.binary), cmd_str,
                    ])
                    continue
                median = statistics.median(samples)
                writer.writerow([
                    args.target,
                    label(args, dt),
                    dt,
                    args.size,
                    args.repeat,
                    f"{median:.6f}",
                    f"{min(samples):.6f}",
                    f"{max(samples):.6f}",
                    f"{statistics.stdev(samples) if len(samples) > 1 else 0.0:.6f}",
                    f"{statistics.median(wall_list):.6f}",
                    "PASS" if pass_all else "FAIL",
                    os.path.abspath(args.binary),
                    cmd_str,
                ])

    if not pass_all:
        print(last_text, file=sys.stderr)
        return 1
    return 0


def select_kernel(times, role):
    if role is not None:
        return times.get(role)
    # default: the highest-numbered cross kernel present (cross3 > cross2 > cross1)
    def rank(name):
        m = re.search(r"(\d+)", name)
        return int(m.group(1)) if m else 0
    return times[max(times, key=rank)]


def label(args, dtype):
    base = args.impl or os.path.basename(args.binary)
    role = args.role or "auto"
    return f"{base}[{role}]" if role != "auto" else base


if __name__ == "__main__":
    sys.exit(main())
