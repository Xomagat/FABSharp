//
// Created by Xomagat on 15.10.2026.
//

#include <random>
#include <ctime>

static unsigned long long g_state;
static bool g_seeded = false;

static inline unsigned long long next_u64()
{
    if (!g_seeded) { g_state = (unsigned long long)std::time(nullptr); g_seeded = true; }

    unsigned long long z = (g_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline unsigned long long bounded(unsigned long long range)
{
    if (range == 0) return next_u64();
    unsigned long long limit = (0ULL - range) % range;
    unsigned long long r;
    do { r = next_u64(); } while (r < limit);
    return r % range;
}

static inline long long rand_range(long long lo, long long hi)
{
    if (lo > hi) { long long t = lo; lo = hi; hi = t; }
    unsigned long long range = (unsigned long long)hi - (unsigned long long)lo + 1ULL;
    return (long long)((unsigned long long)lo + bounded(range));
}

static inline double rand_unit_d() { return (double)(next_u64() >> 11) * (1.0 / 9007199254740992.0); }
static inline float  rand_unit_f() { return (float)(next_u64() >> 40) * (1.0f / 16777216.0f); }

extern "C" void fab_set_seed(int s)
{
    g_state  = (unsigned long long)(unsigned)s;
    g_seeded = true;
}

extern "C" int       fab_randi(int x, int y)             { return (int)rand_range(x, y); }
extern "C" short     fab_rands(short x, short y)         { return (short)rand_range(x, y); }
extern "C" long long fab_randl(long long x, long long y) { return rand_range(x, y); }

extern "C" float  fab_randf(float x, float y)    { return x + rand_unit_f() * (y - x); }
extern "C" double fab_randd(double x, double y)  { return x + rand_unit_d() * (y - x); }