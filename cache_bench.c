#include <stdio.h>      // printf
#include <stdlib.h>     // malloc, free, rand
#include <stdint.h>     // uint64_t, uint8_t
#include <string.h>     // memset, memcpy
#include <time.h>       // clock_gettime

#define KB              1024UL
#define MB              (1024UL * KB)

#define STRIDE          64          // one cache line = 64 bytes
#define ACCESSES        (1UL << 24) // 16M accesses — enough for stable measurement
#define WARMUP_REPS     2           // runs before measuring
#define MEASURE_REPS    5           // take median of these

static inline uint64_t now_ns(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

double bench_sequential(volatile uint8_t *buf, size_t size) {
    
    // How many cache lines fit in our buffer?
    size_t n_lines = size / STRIDE;   // e.g. 4096/64 = 64 lines for 4KB
    
    uint64_t times[MEASURE_REPS];     // store timing of each measured run

    for (int rep = 0; rep < WARMUP_REPS + MEASURE_REPS; rep++) {
        
        uint64_t t0 = now_ns();       // start timer
        volatile uint64_t sink = 0;   // fake destination so compiler keeps the loop
        size_t idx = 0;               // start at cache line 0
        size_t accesses = ACCESSES;   // countdown: 16 million iterations

        while (accesses--) {
            sink += buf[idx * STRIDE]; // touch cache line idx
            idx++;
            if (idx >= n_lines)        // if we've reached the end
                idx = 0;               // wrap back to start
        }
        
        (void)sink;                    // suppress "unused variable" warning

        // Only record timing after warmup runs are done
        if (rep >= WARMUP_REPS)
            times[rep - WARMUP_REPS] = now_ns() - t0;
    }

    // Sort times[] and return the middle value (median)
    for (int i = 0; i < MEASURE_REPS - 1; i++)
        for (int j = i+1; j < MEASURE_REPS; j++)
            if (times[j] < times[i]) {
                uint64_t t = times[i];
                times[i] = times[j];
                times[j] = t;
            }

    // Divide total time by number of accesses = ns per single access
    return (double)times[MEASURE_REPS/2] / (double)ACCESSES;
}

double bench_random(volatile uint8_t *buf, size_t size) {
    size_t n_lines = size / STRIDE;    // number of cache lines in buffer
    uint64_t times[MEASURE_REPS];

    // ── Build the random linked list ──────────────────────────
    
    // Temporary array to build the permutation
    size_t *chain = malloc(n_lines * sizeof(size_t));
    
    // Start with identity: chain[i] = i
    for (size_t i = 0; i < n_lines; i++) 
        chain[i] = i;
    
    // Fisher-Yates shuffle — uniform random permutation
    for (size_t i = n_lines - 1; i > 0; i--) {
        size_t j = (size_t)rand() % (i + 1);
        size_t tmp = chain[i]; 
        chain[i] = chain[j]; 
        chain[j] = tmp;
    }
    
    // Write permutation into buffer — each cache line stores next index
    for (size_t i = 0; i < n_lines; i++) {
        size_t next = chain[i];
        memcpy((void *)(buf + i * STRIDE), &next, sizeof(size_t));
    }
    free(chain);    // linked list is now inside buf, don't need chain anymore

    // ── Measure traversal time ────────────────────────────────
    
    for (int rep = 0; rep < WARMUP_REPS + MEASURE_REPS; rep++) {
        
        uint64_t t0 = now_ns();
        volatile size_t idx = 0;        // start at cache line 0
        size_t accesses = ACCESSES;

        while (accesses--) {
            size_t next;
            // Read 8 bytes from start of cache line idx
            memcpy(&next, (void *)(buf + idx * STRIDE), sizeof(size_t));
            idx = next;     // ← this dependency defeats the prefetcher
        }

        if (rep >= WARMUP_REPS)
            times[rep - WARMUP_REPS] = now_ns() - t0;
    }

    // Sort and return median
    for (int i = 0; i < MEASURE_REPS - 1; i++)
        for (int j = i+1; j < MEASURE_REPS; j++)
            if (times[j] < times[i]) {
                uint64_t t = times[i];
                times[i] = times[j];
                times[j] = t;
            }

    return (double)times[MEASURE_REPS/2] / (double)ACCESSES;
}
void stride_sweep(volatile uint8_t *buf, size_t buf_size) {
    printf("\n── Stride Sweep (cache line size detection) ──────────\n");
    printf("%-12s  %-16s  %s\n", "Stride(B)", "Latency(ns)", "Note");
    printf("%-12s  %-16s  %s\n", "---------", "-----------", "----");

    double prev_ns = 0;

    // Double stride each iteration: 1, 2, 4, 8, 16, 32, 64, 128, 256
    for (int stride = 1; stride <= 256; stride *= 2) {
        
        size_t n_slots = buf_size / stride;  // how many slots at this stride
        
        // Warmup
        volatile uint64_t sink = 0;
        size_t idx = 0;
        for (size_t i = 0; i < ACCESSES/4; i++) {
            sink += buf[idx * stride];
            if (++idx >= n_slots) idx = 0;
        }
        (void)sink;

        // Measure
        uint64_t t0 = now_ns();
        sink = 0;
        idx = 0;
        size_t accesses = ACCESSES;
        while (accesses--) {
            sink += buf[idx * stride];
            if (++idx >= n_slots) idx = 0;
        }
        (void)sink;

        double ns = (double)(now_ns() - t0) / (double)ACCESSES;

        // Detect the jump — this is where cache line boundary is
        const char *note = "";
        if (prev_ns > 0 && ns / prev_ns > 1.5)
            note = "← latency jump here";

        printf("%-12d  %-16.3f  %s\n", stride, ns, note);
        prev_ns = ns;
    }

    printf("\nCache line size = stride where latency stops increasing\n");
}
int main(void) {
    srand(42);  // fixed seed = same shuffle every run = reproducible results

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        Cache Hierarchy Performance Profiler          ║\n");
    printf("║        Aditi Shankar | BITS Pilani Goa               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    // ── Define working set sizes ──────────────────────────────
    // Chosen specifically to cross each cache boundary
    // Your CPU probably has: L1=32-64KB, L2=256KB-1MB, L3=4-32MB
    size_t sizes[] = {
        4*KB,   8*KB,   16*KB,          // should all hit L1
        64*KB,  128*KB, 256*KB,         // should transition L1→L2
        1*MB,   4*MB,   8*MB,  16*MB,   // should transition L2→L3
        64*MB,  128*MB, 256*MB          // should hit DRAM
    };
    int n_sizes = sizeof(sizes) / sizeof(sizes[0]);  // = 13

    // ── Allocate buffer ───────────────────────────────────────
    // Allocate the LARGEST size once, then use subsets of it.
    // Much faster than malloc/free for every size.
    size_t max_size = 256 * MB;
    volatile uint8_t *buf = (volatile uint8_t *)malloc(max_size);
    if (!buf) {
        perror("malloc failed");  // print error if allocation fails
        return 1;
    }
    // Touch every page upfront — forces OS to actually allocate
    // physical memory now rather than lazily during measurement
    memset((void *)buf, 1, max_size);

    // ── Arrays to store results for CSV output later ──────────
    double seq_results[13];
    double rnd_results[13];

    // ── Main sweep ────────────────────────────────────────────
    printf("%-12s  %-16s  %-16s  %-10s\n",
           "Size", "Sequential(ns)", "Random(ns)", "Region");
    printf("%-12s  %-16s  %-16s  %-10s\n",
           "----", "--------------", "----------", "------");

    for (int i = 0; i < n_sizes; i++) {
        size_t sz = sizes[i];

        // Re-touch pages for this specific size
        // Ensures the OS hasn't swapped anything out
        memset((void *)buf, 1, sz);

        // Run both benchmarks
        double s = bench_sequential(buf, sz);
        double r = bench_random(buf, sz);

        // Store for CSV
        seq_results[i] = s;
        rnd_results[i] = r;

        // Infer which cache level based on random latency
        // These thresholds work for most modern CPUs
        // Your actual numbers may shift slightly — that's fine
        const char *region;
        if      (r <  6.0)  region = "L1 cache";
        else if (r < 25.0)  region = "L2 cache";
        else if (r < 90.0)  region = "L3 cache";
        else                region = "DRAM";

        // Format size as human-readable label
        char label[16];
        if      (sz >= MB) snprintf(label, 16, "%3lu MB", sz/MB);
        else               snprintf(label, 16, "%3lu KB", sz/KB);

        printf("%-12s  %-16.3f  %-16.3f  %s\n", label, s, r, region);
        
        // fflush prints each line immediately
        // Without this, output buffers and you see nothing until the end
        // — bad UX since this benchmark takes a few minutes
        fflush(stdout);
    }

    // ── Boundary detection ────────────────────────────────────
    // Look for where random latency jumps significantly
    // A 1.8x increase = likely a cache boundary
    printf("\n── Cache Boundaries Detected ─────────────────────────\n");
    double prev = rnd_results[0];
    for (int i = 1; i < n_sizes; i++) {
        double ratio = rnd_results[i] / prev;
        if (ratio > 1.8) {
            char label[16];
            if (sizes[i] >= MB) snprintf(label, 16, "%lu MB", sizes[i]/MB);
            else                snprintf(label, 16, "%lu KB", sizes[i]/KB);
            printf("  → Boundary near %-8s  (%.1fx latency jump)\n",
                   label, ratio);
        }
        prev = rnd_results[i];
    }

    // ── CSV output ────────────────────────────────────────────
    // Pipe this into plot_cache.py to get graphs
    // Usage: ./cache_bench | tee results.txt
    //        python3 plot_cache.py results.txt
    printf("\n── CSV (for plotting) ────────────────────────────────\n");
    printf("size_bytes,seq_ns,rnd_ns\n");
    for (int i = 0; i < n_sizes; i++)
        printf("%zu,%.3f,%.3f\n",
               sizes[i], seq_results[i], rnd_results[i]);

    // ── Stride sweep ──────────────────────────────────────────
    // Use a 4MB buffer — big enough to exceed L2
    // so accesses that miss actually go somewhere deep
    stride_sweep(buf, 4*MB);

    free((void *)buf);
    return 0;
}
