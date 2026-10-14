#!/usr/bin/env python3
"""Generate the benchmark datasets in this folder.

Every dataset is a JSON file in the format read by benchmark/environment.hpp:

    {
      "name": ..., "description": ..., "params": {...}, "stats": {...},
      "pages":    [{"key": <int>, "content": <string>}, ...],
      "workload": [<int>, ...]
    }

Only "pages" and "workload" are read by the benchmark; the other fields
document what the workload models and which strategy it is expected to favour.

All generators use fixed seeds, so running this script again reproduces the
same files byte for byte:

    python3 data/generate.py
"""

import bisect
import itertools
import json
import random
import string
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parent

INT_MIN, INT_MAX = -2**31, 2**31 - 1  # the benchmark uses KeyT = int


# ---------------------------------------------------------------- helpers ---

class Zipf:
    """Samples a rank in [0, n) with P(rank r) ~ 1 / (r + 1) ** alpha."""

    def __init__(self, rng, n, alpha):
        self.rng = rng
        self.cdf = list(itertools.accumulate(1.0 / (r ** alpha) for r in range(1, n + 1)))

    def __call__(self):
        return bisect.bisect_left(self.cdf, self.rng.random() * self.cdf[-1])


def shuffled_keys(rng, first, n):
    """n consecutive keys starting at `first` in random order, so that
    popularity rank is not correlated with the key value."""
    keys = list(range(first, first + n))
    rng.shuffle(keys)
    return keys


def make_content(rng, key, size):
    prefix = f"page-{key}:"
    alphabet = string.ascii_letters + string.digits
    return prefix + "".join(rng.choices(alphabet, k=max(0, size - len(prefix))))


def workload_stats(workload):
    counts = {}
    for k in workload:
        counts[k] = counts.get(k, 0) + 1
    return {
        "requests": len(workload),
        "distinct_keys": len(counts),
        "one_hit_keys": sum(1 for c in counts.values() if c == 1),
        "max_key_frequency": max(counts.values()) if counts else 0,
    }


def write_dataset(name, description, params, workload, page_size=(16, 64)):
    assert all(isinstance(k, int) and INT_MIN <= k <= INT_MAX for k in workload)
    rng = random.Random(f"{name}/pages")
    keys = sorted(set(workload))  # every requested key has a page
    pages = [{"key": k, "content": make_content(rng, k, rng.randint(*page_size))} for k in keys]
    stats = workload_stats(workload)

    # pages one per line and 25 keys per workload line: compact, but diffable
    lines = [
        "{",
        f'  "name": {json.dumps(name)},',
        f'  "description": {json.dumps(description)},',
        f'  "params": {json.dumps(params)},',
        f'  "stats": {json.dumps(stats)},',
        '  "pages": [',
        ",\n".join(f"    {json.dumps(p)}" for p in pages),
        "  ],",
        '  "workload": [',
        ",\n".join("    " + ", ".join(map(str, workload[i:i + 25]))
                   for i in range(0, len(workload), 25)),
        "  ]",
        "}",
    ]
    path = OUT_DIR / f"{name}.json"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    json.loads(path.read_text(encoding="utf-8"))  # self-check: valid JSON
    print(f"{path.name:28} requests {stats['requests']:6}  distinct {stats['distinct_keys']:6}"
          f"  one-hit {stats['one_hit_keys']:6}  {path.stat().st_size / 1024:8.1f} KiB")


# --------------------------------------------------------------- datasets ---

def smoke():
    # Belady's anomaly sequence: small enough to check every policy by hand
    workload = [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]
    write_dataset(
        "01_smoke",
        "Classic 12-request sequence (Belady's anomaly example). Sanity check: small enough "
        "to trace every policy by hand; with capacity 3 the optimum is 5 hits.",
        {}, workload)


def uniform():
    rng = random.Random(2)
    n_keys, n_req = 2000, 50000
    workload = [rng.randrange(n_keys) for _ in range(n_req)]
    write_dataset(
        "02_uniform",
        "Uniform random keys, no locality at all. Every policy should give a hit rate close "
        "to capacity / 2000; a policy far from that is broken. Baseline for all others.",
        {"keys": n_keys, "requests": n_req}, workload)


def zipf(name, alpha, seed, note):
    rng = random.Random(seed)
    n_keys, n_req = 5000, 50000
    keys = shuffled_keys(rng, 0, n_keys)
    rank = Zipf(rng, n_keys, alpha)
    workload = [keys[rank()] for _ in range(n_req)]
    write_dataset(
        name,
        f"Stationary Zipf popularity, alpha = {alpha}, over {n_keys} keys. {note}",
        {"keys": n_keys, "requests": n_req, "alpha": alpha}, workload)


def loop(name, length, n_req, note):
    workload = [i % length for i in range(n_req)]
    write_dataset(
        name,
        f"Cyclic scan 0..{length - 1} repeated. {note}",
        {"loop_length": length, "requests": n_req}, workload)


def scan_hot():
    rng = random.Random(8)
    hot, hot_burst, scan_len, n_req = 200, 2000, 800, 50000
    next_scan_key = 100000
    workload = []
    while len(workload) < n_req:
        workload += [rng.randrange(hot) for _ in range(hot_burst)]
        workload += range(next_scan_key, next_scan_key + scan_len)
        next_scan_key += scan_len
    workload = workload[:n_req]
    write_dataset(
        "08_scan_hot",
        "Hot set of 200 keys (uniform) interrupted every 2000 requests by a one-time "
        "sequential scan of 800 never-repeated keys. Tests scan resistance: LRU loses the hot "
        "set on every scan (for capacity < 1000); 2Q / ARC / LIRS should keep it.",
        {"hot_keys": hot, "hot_burst": hot_burst, "scan_length": scan_len, "requests": n_req},
        workload)


def phase_shift():
    rng = random.Random(9)
    phases, phase_len, set_size, alpha = 5, 10000, 300, 1.0
    workload = []
    for p in range(phases):
        keys = shuffled_keys(rng, p * 1000, set_size)
        rank = Zipf(rng, set_size, alpha)
        workload += [keys[rank()] for _ in range(phase_len)]
    write_dataset(
        "09_phase_shift",
        "Five phases of 10000 requests; each phase uses its own disjoint set of 300 keys with "
        "Zipf(1.0) popularity. Tests adaptation to a changing working set: LFU keeps stale "
        "counts of old phases, LRU / ARC adapt quickly.",
        {"phases": phases, "phase_length": phase_len, "set_size": set_size, "alpha": alpha},
        workload)


def lfu_pollution():
    rng = random.Random(10)
    a_len, a_keys, b_len, b_keys = 15000, 100, 35000, 1000
    a = shuffled_keys(rng, 0, a_keys)
    b = shuffled_keys(rng, 10000, b_keys)
    rank_a, rank_b = Zipf(rng, a_keys, 1.2), Zipf(rng, b_keys, 0.9)
    workload = [a[rank_a()] for _ in range(a_len)] + [b[rank_b()] for _ in range(b_len)]
    write_dataset(
        "10_lfu_pollution",
        "15000 requests to 100 very hot keys (Zipf 1.2), then they are never used again and "
        "35000 requests go to 1000 other keys (Zipf 0.9). Frequencies never decay in LFU, so "
        "the dead hot keys occupy the cache for the whole second part.",
        {"burst_requests": a_len, "burst_keys": a_keys, "tail_requests": b_len,
         "tail_keys": b_keys}, workload)


def lru_stack():
    rng = random.Random(11)
    n_req, p_new, max_depth = 50000, 0.1, 2000
    stack, next_key, workload = [], 0, []
    for _ in range(n_req):
        if not stack or rng.random() < p_new:
            key = next_key
            next_key += 1
        else:
            # log-uniform stack distance: every decade of cache size adds about the
            # same number of hits, so the hit-rate curve over capacity is informative
            depth = int(max_depth ** rng.random()) - 1
            key = stack.pop(min(depth, len(stack) - 1))
        stack.insert(0, key)
        workload.append(key)
    write_dataset(
        "11_lru_stack",
        "Generated by the LRU stack model: 10% new keys, otherwise a key at a log-uniform "
        "stack distance in [1, 2000]. Pure temporal locality, the ideal case for LRU; "
        "frequency-based policies have no advantage here.",
        {"requests": n_req, "new_key_probability": p_new, "max_stack_depth": max_depth},
        workload)


def correlated():
    rng = random.Random(12)
    n_req, bg_keys, burst_pool = 50000, 3000, 20000
    keys = shuffled_keys(rng, 0, bg_keys)
    rank = Zipf(rng, bg_keys, 0.8)
    workload = []
    while len(workload) < n_req:
        if rng.random() < 0.6:
            workload.append(keys[rank()])
        else:
            # correlated references: one logical access = 2..4 requests close together
            k = 100000 + rng.randrange(burst_pool)
            for _ in range(rng.randint(2, 4)):
                workload.append(k)
                workload += [keys[rank()] for _ in range(rng.randint(0, 2))]
    workload = workload[:n_req]
    write_dataset(
        "12_correlated",
        "Zipf(0.8) background over 3000 keys mixed with correlated bursts: a key from a pool "
        "of 20000 is requested 2-4 times within a few requests and is then rarely seen again. "
        "The motivating case of 2Q: a short burst should not look like a popular page.",
        {"requests": n_req, "background_keys": bg_keys, "burst_pool": burst_pool}, workload)


def multilevel_tiers():
    rng = random.Random(13)
    n_req = 50000
    hot, warm, cold = 20, 500, 20000
    workload = []
    for _ in range(n_req):
        r = rng.random()
        if r < 0.50:
            workload.append(rng.randrange(hot))
        elif r < 0.85:
            workload.append(1000 + rng.randrange(warm))
        else:
            workload.append(100000 + rng.randrange(cold))
    write_dataset(
        "13_multilevel_tiers",
        "Three tiers: 50% of requests to 20 hot keys, 35% to 500 warm keys, 15% to 20000 cold "
        "keys (mostly one-hit). Built for hierarchies: an L1 of ~20-30 pages should hold the hot "
        "tier and an L2 of ~500+ the warm one. Compare which strategy is best at each level.",
        {"requests": n_req, "hot_keys": hot, "warm_keys": warm, "cold_keys": cold,
         "shares": [0.5, 0.35, 0.15]}, workload)


def oltp_scan():
    rng = random.Random(14)
    n_keys, oltp_len, n_req = 3000, 12000, 50000
    keys = shuffled_keys(rng, 0, n_keys)
    rank = Zipf(rng, n_keys, 0.9)
    workload = []
    while len(workload) < n_req:
        workload += [keys[rank()] for _ in range(oltp_len)]
        workload += range(n_keys)  # full table scan over the same keys, in order
    workload = workload[:n_req]
    write_dataset(
        "14_oltp_scan",
        "Database-like: Zipf(0.9) point lookups over a 3000-row table, and every 12000 "
        "requests a full sequential scan of the same 3000 rows. Unlike 08_scan_hot the scan "
        "touches existing keys, so it pollutes recency AND bumps frequencies.",
        {"keys": n_keys, "lookups_between_scans": oltp_len, "requests": n_req}, workload)


def one_hit_wonders():
    rng = random.Random(15)
    n_req, n_keys, p_unique = 50000, 1000, 0.6
    keys = shuffled_keys(rng, 0, n_keys)
    rank = Zipf(rng, n_keys, 1.0)
    next_unique, workload = 1000000, []
    for _ in range(n_req):
        if rng.random() < p_unique:
            workload.append(next_unique)
            next_unique += 1
        else:
            workload.append(keys[rank()])
    write_dataset(
        "15_one_hit_wonders",
        "CDN-like: 60% of requests are keys seen exactly once, 40% follow Zipf(1.0) over 1000 "
        "keys. Tests admission: a policy that lets one-time keys push out popular pages loses "
        "(LRU), policies with a probation area (2Q A1in, ARC T1, LIRS HIR) should win.",
        {"requests": n_req, "popular_keys": n_keys, "unique_share": p_unique}, workload,
        page_size=(12, 24))


def large_pages():
    rng = random.Random(16)
    n_keys, n_req = 1000, 20000
    keys = shuffled_keys(rng, 0, n_keys)
    rank = Zipf(rng, n_keys, 1.0)
    workload = [keys[rank()] for _ in range(n_req)]
    write_dataset(
        "16_large_pages",
        "Zipf(1.0) over 1000 keys with 1-2 KiB pages. Hit rates are unremarkable; the point is "
        "running time: every lookup copies T, and in a hierarchy the page is copied on every "
        "level, so this shows the cost of the by-value interface.",
        {"keys": n_keys, "requests": n_req, "page_bytes": [1024, 2048]}, workload,
        page_size=(1024, 2048))


def sliding_window():
    rng = random.Random(17)
    n_req, width, step_every = 50000, 300, 40
    workload = [i // step_every + rng.randrange(width) for i in range(n_req)]
    write_dataset(
        "17_sliding_window",
        "Uniform requests inside a window of 300 consecutive keys that slides forward by one "
        "key every 40 requests. Slow drift of the working set: recency-based policies track it, "
        "LFU keeps keys that have just left the window.",
        {"requests": n_req, "window": width, "step_every": step_every}, workload)


def loop_zipf_mix():
    rng = random.Random(18)
    n_req, loop_len, n_keys = 50000, 600, 2000
    keys = shuffled_keys(rng, 10000, n_keys)
    rank = Zipf(rng, n_keys, 1.0)
    workload, pos = [], 0
    for _ in range(n_req):
        if rng.random() < 0.5:
            workload.append(pos)
            pos = (pos + 1) % loop_len
        else:
            workload.append(keys[rank()])
    write_dataset(
        "18_loop_zipf_mix",
        "Half of the requests walk a loop over 600 keys, the other half are Zipf(1.0) over "
        "2000 other keys, interleaved. Recency and frequency signals conflict: ARC has to "
        "find the right T1/T2 split, LIRS should keep a part of the loop as LIR.",
        {"requests": n_req, "loop_length": loop_len, "zipf_keys": n_keys}, workload)


def main():
    smoke()
    uniform()
    zipf("03_zipf_0.8", 0.8, 3, "Mild skew, typical for web objects.")
    zipf("04_zipf_1.0", 1.0, 4, "Classic Zipf law.")
    zipf("05_zipf_1.2", 1.2, 5, "Strong skew: a small hot set, so even a tiny L1 gets many "
                                "hits; frequency-aware policies (LFU, ARC, LIRS) should lead.")
    loop("06_loop_150", 150, 30000,
         "Pathological for LRU: with capacity < 150 every request misses. LIRS / ARC keep a "
         "part of the loop resident. Sized for caches of ~100 pages.")
    loop("07_loop_1200", 1200, 60000,
         "The same pattern sized for larger (L2/L3) caches of a few hundred to 1000 pages.")
    scan_hot()
    phase_shift()
    lfu_pollution()
    lru_stack()
    correlated()
    multilevel_tiers()
    oltp_scan()
    one_hit_wonders()
    large_pages()
    sliding_window()
    loop_zipf_mix()


if __name__ == "__main__":
    main()
