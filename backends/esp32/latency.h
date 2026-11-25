#ifndef CYDUI_LATENCY_H
#define CYDUI_LATENCY_H

/*
 * The last few latencies of the touch screen (NFR-02): the time from a touch event entering the queue to the
 * end of the frame that showed it, kept in a ring, with percentiles by the nearest-rank rule. Pure C.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CYDUI_LAT_MAX 128

typedef struct {
    uint32_t us[CYDUI_LAT_MAX];
    int n;    /* samples held, at most CYDUI_LAT_MAX */
    int next; /* where the next one goes */
} cydui_lat;

typedef struct {
    int n;
    uint32_t p50, p95, max;
} cydui_lat_stats;

void cydui_lat_init(cydui_lat *l);

/** Adds a sample; once CYDUI_LAT_MAX are held the oldest is overwritten. */
void cydui_lat_add(cydui_lat *l, uint32_t us);

/** Percentiles of what is held (all zero with no samples). Nearest rank: the value at ceil(pct / 100 * n). */
void cydui_lat_stats_get(const cydui_lat *l, cydui_lat_stats *out);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_LATENCY_H */
