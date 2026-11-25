#include "latency.h"

#include <string.h>

void cydui_lat_init(cydui_lat *l)
{
    memset(l, 0, sizeof(*l));
}

void cydui_lat_add(cydui_lat *l, uint32_t us)
{
    l->us[l->next] = us;
    l->next = (l->next + 1) % CYDUI_LAT_MAX;
    if (l->n < CYDUI_LAT_MAX) {
        l->n++;
    }
}

static uint32_t nearest_rank(const uint32_t *sorted, int n, int pct)
{
    int rank = (pct * n + 99) / 100; /* ceil(pct * n / 100) */

    if (rank < 1) rank = 1;
    if (rank > n) rank = n;
    return sorted[rank - 1];
}

void cydui_lat_stats_get(const cydui_lat *l, cydui_lat_stats *out)
{
    uint32_t v[CYDUI_LAT_MAX];
    int i, j;

    memset(out, 0, sizeof(*out));
    if (l->n == 0) {
        return;
    }
    memcpy(v, l->us, (size_t)l->n * sizeof(v[0]));
    for (i = 1; i < l->n; i++) { /* insertion sort: 128 values at most */
        uint32_t t = v[i];
        for (j = i - 1; j >= 0 && v[j] > t; j--) {
            v[j + 1] = v[j];
        }
        v[j + 1] = t;
    }
    out->n = l->n;
    out->p50 = nearest_rank(v, l->n, 50);
    out->p95 = nearest_rank(v, l->n, 95);
    out->max = v[l->n - 1];
}
