# Cache Hierarchy Performance Profiler

**Aditi Shankar | BITS Pilani, Goa Campus | ECE 2026**

A systems-level benchmark written in C that experimentally characterizes 
CPU cache hierarchy performance by measuring memory access latency across 
working set sizes from 4KB to 256MB.

## Key Results (measured on Apple x86_64, macOS)

| Working Set | Sequential (ns) | Random (ns) | Region |
|---|---|---|---|
| 4 KB – 16 KB | ~2.5 | ~5.5 | L1/L2 cache |
| 64 KB – 256 KB | ~2.6 | ~5.5 – 8.2 | L2 cache |
| 1 MB – 8 MB | ~2.9 – 3.2 | ~11 – 25 | L3 cache |
| 16 MB | ~4.1 | ~72 | Deep L3 |
| 64 MB – 128 MB | ~4.3 – 4.4 | ~109 – 117 | DRAM |

**Cache boundaries detected:**
- L2 → L3 transition near **1 MB** (3.6x latency jump)
- L3 deepening near **8 MB** (2.1x jump)  
- L3 → DRAM transition near **16 MB** (2.9x jump)

**Observed non-uniform L3 behaviour** at 4MB and 256MB — latency 
lower than surrounding sizes, consistent with shared L3 occupancy 
variation under different thermal and load conditions.

**Stride sweep:** Cache line size confirmed at 64 bytes — latency 
begins climbing at stride=32 and flatlines after stride=64.

## What it measures and why

### Sequential benchmark
Touches every cache line in order. The hardware prefetcher detects 
the pattern and fetches ahead — latency stays ~2.5ns regardless of 
working set size. Measures bandwidth-bound performance.

### Random / pointer-chase benchmark  
Builds a shuffled linked list through the buffer using Fisher-Yates 
shuffle. Each access depends on the result of the previous one — 
the prefetcher cannot predict the next address. Every cache miss 
pays full penalty. Measures true latency-bound performance.

### Stride sweep
Accesses a fixed 4MB buffer at strides of 1, 2, 4...256 bytes. 
Latency increases until stride crosses the cache line boundary, 
then flatlines — empirically revealing the hardware cache line size.

## Concepts demonstrated

| Observation | Chip Design Relevance |
|---|---|
| L1 latency ~2–5 ns | On-chip SRAM access time in processor design |
| Cache line = 64 bytes | Block size decisions in cache controller RTL |
| DRAM latency ~110 ns | Off-chip memory interface timing constraints |
| Prefetcher effectiveness | Hardware prefetch unit design trade-offs |
| Non-uniform L3 behaviour | Shared cache contention in multi-core designs |

## How to run

```bash
# Compile
make

# Run and save
./cache_bench | tee results.txt

# Plot
python3 plot_cache.py results.txt
```

## Files
- `cache_bench.c` — C benchmark (sequential, random, stride sweep)
- `plot_cache.py` — Python visualisation
- `Makefile` — build system# cache-hierarchy-profiler
