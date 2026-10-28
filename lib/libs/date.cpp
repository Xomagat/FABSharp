//
// Created by Xomagat on 27.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct FabDate { int y, m, d; };
struct FabTime { int secs; };            // 0..86399

#define D(h) ((FabDate*)(h))
#define T(h) ((FabTime*)(h))

// ---------- calendar helpers ----------
static bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int days_in_month(int y, int m)
{
    static const int t[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && is_leap(y)) ? 29 : t[m - 1];
}

static bool valid_date(int y, int m, int d)
{
    return y >= 1 && m >= 1 && m <= 12 && d >= 1 && d <= days_in_month(y, m);
}

// days since 1970-01-01
static long long days_from_civil(long long y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

static void civil_from_days(long long z, int& y, int& m, int& d)
{
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp  = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)((long long)yoe + era * 400 + (m <= 2));
}

static long long to_days(const FabDate* x) { return days_from_civil(x->y, x->m, x->d); }

// 1 = Monday ... 7 = Sunday (1970-01-01 was Thursday)
static int weekday_of(long long days) { return (int)(((days % 7) + 7 + 3) % 7) + 1; }

static const char* MONTHS[] = {"january", "february", "march", "april", "may", "june", "july",
                               "august", "september", "october", "november", "december"};
static const char* DAYS[]   = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};

static int wrap_day(long long s) { long long r = s % 86400; if (r < 0) r += 86400; return (int)r; }
static int sign_of(long long x)  { return (x > 0) - (x < 0); }

static char* alloc_fmt3(const char* fmt, int a, int b, int c)
{
    char* p = (char*)malloc(48);
    if (!p) return nullptr;
    snprintf(p, 48, fmt, a, b, c);
    return p;
}

// ---------- local "now" ----------
struct Now { int y, mo, d, h, mi, s, wd, yd; };

static Now now_local()
{
    time_t t = time(nullptr);
#ifdef _WIN32
    struct tm tmv = *localtime(&t);
#else
    struct tm tmv;
    localtime_r(&t, &tmv);
#endif
    return { tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
             tmv.tm_wday == 0 ? 7 : tmv.tm_wday, tmv.tm_yday + 1 };
}

// getters: year_now, month_now, ...
#define FAB_NOW(NAME, FIELD) extern "C" int fab_##NAME##_now() { return now_local().FIELD; }
FAB_NOW(year, y)   FAB_NOW(month, mo)  FAB_NOW(day, d)
FAB_NOW(hour, h)   FAB_NOW(min, mi)    FAB_NOW(sec, s)
FAB_NOW(weekday, wd) FAB_NOW(yday, yd)

extern "C" long long fab_unix_now() { return (long long)time(nullptr); }

// ====================== DATE ======================
extern "C" bool fab_date_valid(int y, int m, int d) { return valid_date(y, m, d); }

extern "C" void* fab_date_new(int y, int m, int d)
{
    if (!valid_date(y, m, d)) return nullptr;
    auto* x = (FabDate*)malloc(sizeof(FabDate));
    if (!x) return nullptr;
    x->y = y; x->m = m; x->d = d;
    return x;
}

extern "C" void* fab_date_now()
{
    Now n = now_local();
    return fab_date_new(n.y, n.mo, n.d);
}

extern "C" void* fab_date_from_days(long long days)
{
    int y, m, d;
    civil_from_days(days, y, m, d);
    return fab_date_new(y, m, d);
}

// "YYYY-MM-DD"
extern "C" void* fab_date_parse(const char* s)
{
    int y, m, d;
    if (!s || sscanf(s, "%d-%d-%d", &y, &m, &d) != 3) return nullptr;
    return fab_date_new(y, m, d);
}

extern "C" void  fab_date_free(void* h) { free(h); }
extern "C" void* fab_date_copy(void* h)
{
    if (!h) return nullptr;
    return fab_date_new(D(h)->y, D(h)->m, D(h)->d);
}

// Access (null-safe: 0)
extern "C" int fab_date_year(void* h)  { return h ? D(h)->y : 0; }
extern "C" int fab_date_month(void* h) { return h ? D(h)->m : 0; }
extern "C" int fab_date_day(void* h)   { return h ? D(h)->d : 0; }
extern "C" int fab_date_weekday(void* h)     { return h ? weekday_of(to_days(D(h))) : 0; }
extern "C" int fab_date_day_of_year(void* h) { return h ? (int)(to_days(D(h)) - days_from_civil(D(h)->y, 1, 1)) + 1 : 0; }
extern "C" bool fab_date_is_leap(void* h)    { return h && is_leap(D(h)->y); }
extern "C" int fab_date_days_in_month(void* h) { return h ? days_in_month(D(h)->y, D(h)->m) : 0; }
extern "C" long long fab_date_to_days(void* h) { return h ? to_days(D(h)) : 0; }
extern "C" const char* fab_date_month_name(void* h)   { return h ? MONTHS[D(h)->m - 1] : ""; }
extern "C" const char* fab_date_weekday_name(void* h) { return h ? DAYS[weekday_of(to_days(D(h))) - 1] : ""; }

// Change (in place, like list)
extern "C" void fab_date_add_days(void* h, int n)
{
    if (!h) return;
    civil_from_days(to_days(D(h)) + n, D(h)->y, D(h)->m, D(h)->d);
}

extern "C" void fab_date_add_months(void* h, int n)
{
    if (!h) return;
    long long total = (long long)D(h)->y * 12 + (D(h)->m - 1) + n;
    if (total < 12) return;                      // year < 1
    int ny = (int)(total / 12), nm = (int)(total % 12) + 1;
    D(h)->y = ny; D(h)->m = nm;
    if (D(h)->d > days_in_month(ny, nm)) D(h)->d = days_in_month(ny, nm);   // 31 jan + 1 month = 28/29 feb
}

extern "C" void fab_date_add_years(void* h, int n) { fab_date_add_months(h, n * 12); }

// Compare
extern "C" long long fab_date_diff_days(void* a, void* b)   // a - b
{
    if (!a || !b) return 0;
    return to_days(D(a)) - to_days(D(b));
}
extern "C" int  fab_date_compare(void* a, void* b)   { return (int)sign_of(fab_date_diff_days(a, b)); }
extern "C" bool fab_date_equals(void* a, void* b)    { return a && b && fab_date_diff_days(a, b) == 0; }
extern "C" bool fab_date_is_before(void* a, void* b) { return a && b && fab_date_diff_days(a, b) < 0; }
extern "C" bool fab_date_is_after(void* a, void* b)  { return a && b && fab_date_diff_days(a, b) > 0; }

// Text
extern "C" char* fab_date_to_str(void* h)
{
    if (!h) return alloc_fmt3("", 0, 0, 0);
    return alloc_fmt3("%04d-%02d-%02d", D(h)->y, D(h)->m, D(h)->d);
}

// %Y %y %m %d %j %B %A
extern "C" char* fab_date_format(void* h, const char* f)
{
    if (!h || !f) return alloc_fmt3("", 0, 0, 0);
    size_t cap = strlen(f) * 6 + 16;
    char* r = (char*)malloc(cap);
    if (!r) return nullptr;
    char* p = r;

    for (; *f; ++f)
    {
        if (*f != '%' || !f[1]) { *p++ = *f; continue; }
        ++f;
        size_t left = cap - (size_t)(p - r);
        switch (*f)
        {
            case 'Y': p += snprintf(p, left, "%04d", D(h)->y); break;
            case 'y': p += snprintf(p, left, "%02d", D(h)->y % 100); break;
            case 'm': p += snprintf(p, left, "%02d", D(h)->m); break;
            case 'd': p += snprintf(p, left, "%02d", D(h)->d); break;
            case 'j': p += snprintf(p, left, "%03d", fab_date_day_of_year(h)); break;
            case 'B': p += snprintf(p, left, "%s", fab_date_month_name(h)); break;
            case 'A': p += snprintf(p, left, "%s", fab_date_weekday_name(h)); break;
            default:  *p++ = '%'; *p++ = *f; break;
        }
    }
    *p = '\0';
    return r;
}

// ====================== TIME ======================
extern "C" void* fab_time_from_seconds(int s)
{
    auto* x = (FabTime*)malloc(sizeof(FabTime));
    if (!x) return nullptr;
    x->secs = wrap_day(s);
    return x;
}

extern "C" void* fab_time_new(int h, int m, int s)
{
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) return nullptr;
    return fab_time_from_seconds(h * 3600 + m * 60 + s);
}

extern "C" void* fab_time_now()
{
    Now n = now_local();
    return fab_time_new(n.h, n.mi, n.s);
}

// "HH:MM:SS" or "HH:MM"
extern "C" void* fab_time_parse(const char* str)
{
    int h, m, s = 0;
    if (!str) return nullptr;
    int n = sscanf(str, "%d:%d:%d", &h, &m, &s);
    if (n < 2) return nullptr;
    return fab_time_new(h, m, s);
}

extern "C" void  fab_time_free(void* h) { free(h); }
extern "C" void* fab_time_copy(void* h) { return h ? fab_time_from_seconds(T(h)->secs) : nullptr; }

extern "C" int fab_time_hour(void* h)   { return h ? T(h)->secs / 3600 : 0; }
extern "C" int fab_time_minute(void* h) { return h ? T(h)->secs / 60 % 60 : 0; }
extern "C" int fab_time_second(void* h) { return h ? T(h)->secs % 60 : 0; }
extern "C" int fab_time_to_seconds(void* h) { return h ? T(h)->secs : 0; }

// add: wraps around midnight
extern "C" void fab_time_add_seconds(void* h, int n) { if (h) T(h)->secs = wrap_day((long long)T(h)->secs + n); }
extern "C" void fab_time_add_minutes(void* h, int n) { if (h) T(h)->secs = wrap_day((long long)T(h)->secs + (long long)n * 60); }
extern "C" void fab_time_add_hours(void* h, int n)   { if (h) T(h)->secs = wrap_day((long long)T(h)->secs + (long long)n * 3600); }

extern "C" int  fab_time_diff_seconds(void* a, void* b) { return (a && b) ? T(a)->secs - T(b)->secs : 0; }
extern "C" int  fab_time_compare(void* a, void* b)      { return sign_of(fab_time_diff_seconds(a, b)); }
extern "C" bool fab_time_equals(void* a, void* b)       { return a && b && T(a)->secs == T(b)->secs; }
extern "C" bool fab_time_is_before(void* a, void* b)    { return a && b && T(a)->secs < T(b)->secs; }
extern "C" bool fab_time_is_after(void* a, void* b)     { return a && b && T(a)->secs > T(b)->secs; }

extern "C" char* fab_time_to_str(void* h)
{
    if (!h) return alloc_fmt3("", 0, 0, 0);
    return alloc_fmt3("%02d:%02d:%02d", fab_time_hour(h), fab_time_minute(h), fab_time_second(h));
}

// %H %M %S
extern "C" char* fab_time_format(void* h, const char* f)
{
    if (!h || !f) return alloc_fmt3("", 0, 0, 0);
    size_t cap = strlen(f) * 3 + 16;
    char* r = (char*)malloc(cap);
    if (!r) return nullptr;
    char* p = r;

    for (; *f; ++f)
    {
        if (*f != '%' || !f[1]) { *p++ = *f; continue; }
        ++f;
        size_t left = cap - (size_t)(p - r);
        switch (*f)
        {
            case 'H': p += snprintf(p, left, "%02d", fab_time_hour(h)); break;
            case 'M': p += snprintf(p, left, "%02d", fab_time_minute(h)); break;
            case 'S': p += snprintf(p, left, "%02d", fab_time_second(h)); break;
            default:  *p++ = '%'; *p++ = *f; break;
        }
    }
    *p = '\0';
    return r;
}