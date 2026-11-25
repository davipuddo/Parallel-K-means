#!/usr/bin/env python3
"""
Build a class-balanced, inflated copy of a *_sempayload.csv dataset so that the
sequential k-means in ../src/main.c runs long enough (>10 s) to be worth
parallelizing, and so that its accuracy number is actually informative.

Two problems with the raw captures, and what this script does about them:

1. Class imbalance. treino is ~94% flag_binaria=0, so a clustering that puts
   every point in one class still scores ~94%: both clusters end up mapped to
   class 0 and the accuracy says nothing. Resampling to ~50/50 makes the
   majority-vote accuracy meaningful again -- a useless clustering now scores
   ~50%, and anything above that is real signal.

2. Feature scaling. k-means sums un-normalized squared differences, so a
   feature weighs on the distance in proportion to its variance. 'mudou' is
   binary and swings the full 0->1 while the other four wiggle inside ~0.15
   bands, so it alone is 65% of all distance in the capture -- the clustering
   was essentially just recovering that one column, and it is only weakly
   predictive. Features are z-scored by default so all five count equally;
   --no-standardize keeps the original values. Measured at 80M rows this is
   worth ~1.7 points (58.7% -> 60.3%): it removes an arbitrary weighting, but
   it is not a large accuracy win, because the better-separating clustering is
   not the one k-means converges to (see the note on initialization below).

3. Size. 10M rows take ~1.7 s sequentially, too short to show a speedup over
   thread/process startup noise. Rows are resampled up to --rows.

The minority class is oversampled well past its original row count, so the
output is SYNTHETIC: it is a benchmark workload with the same feature
distribution and cluster geometry as the capture, not new observations. Small
Gaussian jitter is added to the continuous features so the duplicated rows are
not bit-identical points (exact duplicates make centroids degenerate). Treat
timings from it as real and accuracy from it as "does my parallel version still
agree with the sequential one", not as a scientific result about the capture.

A note on initialization, which this script cannot fix: main.c seeds its
centroids with two uniformly random points. On the z-scored data that reaches
one of two local optima -- a ~60% one (lower inertia, found by ~10 of 12 seeds)
and a ~80% one (higher inertia, ~2 of 12). k-means minimizes inertia, so it
prefers the less accurate partition; the accuracy ceiling here is an artifact
of k=2 random-restart k-means, not of the data. k-means++ seeding or
best-of-N-restarts would raise and stabilize it.

Usage:
    python3 make_benchmark_dataset.py                     # defaults, ~80M rows
    python3 make_benchmark_dataset.py --rows 40000000
    python3 make_benchmark_dataset.py --input teste_sempayload.csv \
                                      --output teste_balanced_big.csv
"""

import argparse
import os
import sys
import time

import numpy as np
import pandas as pd

# arbitration_id,dlc,dt_id,dt,mudou,flag_binaria
FEATURES = ["arbitration_id", "dlc", "dt_id", "dt", "mudou"]
LABEL = "flag_binaria"
# 'mudou' is binary and must stay binary -- jitter only the continuous columns.
CONTINUOUS = [0, 1, 2, 3]


def parse_args():
    here = os.path.dirname(os.path.abspath(__file__))
    p = argparse.ArgumentParser(
        description="Balance and inflate a k-means benchmark dataset.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--input", default=os.path.join(here, "treino_sempayload.csv"),
                   help="source CSV to resample from")
    p.add_argument("--output", default=os.path.join(here, "treino_balanced_big.csv"),
                   help="CSV to write")
    p.add_argument("--rows", type=int, default=80_000_000,
                   help="rows to generate (~80M puts sequential k-means near 13 s)")
    p.add_argument("--minority-frac", type=float, default=0.5,
                   help="target fraction of flag_binaria=1 rows, in (0,1)")
    p.add_argument("--jitter", type=float, default=0.01,
                   help="Gaussian noise on continuous features, as a fraction "
                        "of each feature's std dev (0 disables)")
    p.add_argument("--standardize", action=argparse.BooleanOptionalAction,
                   default=True,
                   help="z-score every feature. k-means uses plain Euclidean "
                        "distance, so the widest-spread feature dominates: on "
                        "the raw capture that is 'mudou' (std 0.45 vs ~0.12-0.18 "
                        "for the rest), which is only weakly predictive. Scaling "
                        "lets dt_id and the others count too (~1.7 accuracy "
                        "points at 80M rows). Pass --no-standardize to keep "
                        "the capture's original feature values.")
    p.add_argument("--seed", type=int, default=3,
                   help="RNG seed, for a reproducible dataset")
    p.add_argument("--chunk-rows", type=int, default=2_000_000,
                   help="rows generated and written per chunk, to bound memory")
    return p.parse_args()


def even_resample(pool, count, rng):
    """
    Draw `count` indices from `pool`, using every pool entry as evenly as
    possible. Sampling with replacement would leave some source rows unused and
    over-represent others, skewing the feature distribution; tiling a fresh
    permutation per repeat keeps each row's multiplicity within one of the mean.
    """
    if count <= 0:
        return np.empty(0, dtype=np.int32)

    full, remainder = divmod(count, len(pool))
    parts = [rng.permutation(pool) for _ in range(full)]
    if remainder:
        parts.append(rng.choice(pool, size=remainder, replace=False))
    return np.concatenate(parts).astype(np.int32, copy=False)


def main():
    args = parse_args()

    if not 0.0 < args.minority_frac < 1.0:
        sys.exit("--minority-frac must be strictly between 0 and 1")
    if args.rows <= 0:
        sys.exit("--rows must be positive")

    rng = np.random.default_rng(args.seed)

    print(f"Reading {args.input} ...", flush=True)
    t0 = time.perf_counter()
    source = pd.read_csv(args.input, dtype=np.float64)
    missing = [c for c in FEATURES + [LABEL] if c not in source.columns]
    if missing:
        sys.exit(f"Input is missing expected columns: {missing}")

    data = source[FEATURES].to_numpy(dtype=np.float64)
    labels = source[LABEL].to_numpy(dtype=np.int8)
    del source
    print(f"  {len(data):,} rows in {time.perf_counter() - t0:.1f} s")

    if args.standardize:
        spread = data.std(axis=0)
        spread[spread == 0.0] = 1.0  # a constant column would divide by zero
        data = (data - data.mean(axis=0)) / spread
        print("  z-scored features (every column now mean 0, std 1)")
        print("  NOTE: stats come from this file alone. To compare a train and "
              "a test\n        set, both must be scaled by the TRAINING mean "
              "and std, not their own.")

    majority_pool = np.flatnonzero(labels == 0).astype(np.int32)
    minority_pool = np.flatnonzero(labels == 1).astype(np.int32)
    if len(majority_pool) == 0 or len(minority_pool) == 0:
        sys.exit("Input has only one class; nothing to balance")
    print(f"  class 0: {len(majority_pool):,}  "
          f"class 1: {len(minority_pool):,} "
          f"({len(minority_pool) / len(labels):.2%} minority)")

    n_minority = int(round(args.rows * args.minority_frac))
    n_majority = args.rows - n_minority
    print(f"\nTarget: {args.rows:,} rows at {args.minority_frac:.0%} minority")
    print(f"  class 0 x{n_majority / len(majority_pool):.1f}  "
          f"class 1 x{n_minority / len(minority_pool):.1f} (duplication factor)")

    # Index plan for the whole output, then shuffled so the file has no class
    # blocks: main.c can cap the row count and still read a representative
    # prefix.
    print("Planning and shuffling row order ...", flush=True)
    index = np.concatenate([
        even_resample(majority_pool, n_majority, rng),
        even_resample(minority_pool, n_minority, rng),
    ])
    rng.shuffle(index)

    sigma = data[:, CONTINUOUS].std(axis=0) * args.jitter if args.jitter > 0 else None
    lo = data[:, CONTINUOUS].min(axis=0)
    hi = data[:, CONTINUOUS].max(axis=0)
    if sigma is not None:
        print(f"Jitter sigma per continuous feature: {np.round(sigma, 5)}")

    print(f"Writing {args.output} ...", flush=True)
    t0 = time.perf_counter()
    header = ",".join(FEATURES + [LABEL]) + "\n"
    written = 0

    with open(args.output, "w") as out:
        out.write(header)

        for start in range(0, len(index), args.chunk_rows):
            take = index[start:start + args.chunk_rows]
            chunk = data[take]

            if sigma is not None:
                noise = rng.normal(0.0, sigma, size=(len(take), len(CONTINUOUS)))
                chunk[:, CONTINUOUS] = np.clip(chunk[:, CONTINUOUS] + noise, lo, hi)

            frame = pd.DataFrame(chunk, columns=FEATURES)
            if not args.standardize:
                # 'mudou' is categorical -- keep it as a literal 0/1.
                frame["mudou"] = chunk[:, 4].astype(np.int8)
            frame[LABEL] = labels[take]
            frame.to_csv(out, index=False, header=False, float_format="%.6f",
                         columns=FEATURES + [LABEL])

            written += len(take)
            pct = 100.0 * written / len(index)
            print(f"\r  {written:,}/{len(index):,} rows ({pct:5.1f}%)",
                  end="", flush=True)

    elapsed = time.perf_counter() - t0
    size_gb = os.path.getsize(args.output) / 1e9
    print(f"\n  done in {elapsed / 60:.1f} min, {size_gb:.2f} GB")

    rel = os.path.relpath(args.output, os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "..", "src"))
    print(f"\nRun it (the trailing 0 means 'read every row', since main.c "
          f"defaults to 1M):\n    ./kmeans {rel} 0")


if __name__ == "__main__":
    main()
