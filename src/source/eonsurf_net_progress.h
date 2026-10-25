#ifndef EONSURF_NET_PROGRESS_H
#define EONSURF_NET_PROGRESS_H

#include <stdint.h>
#include <sys/time.h>

/* Estado de progreso de carga en vivo */
static double g_load_total_bytes = 0.0;
static double g_load_now_bytes = 0.0;
static float  g_load_percent = 0.0f;
static float  g_load_speed_kbps = 0.0f;
static uint64_t g_last_progress_time_us = 0;

/* Callback de progreso de libcurl */
static inline int eonsurf_curl_progress_cb(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;
    g_load_total_bytes = dltotal;
    g_load_now_bytes = dlnow;

    if (dltotal > 0.0) {
        g_load_percent = (float)((dlnow / dltotal) * 100.0);
        if (g_load_percent > 100.0f) g_load_percent = 100.0f;
    } else if (dlnow > 0.0) {
        /* Si el servidor no envía Content-Length, simular avance */
        float fake = (float)(dlnow / 102400.0) * 50.0f;
        if (fake > 90.0f) fake = 90.0f;
        g_load_percent = fake;
    } else {
        g_load_percent = 0.0f;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
    if (g_last_progress_time_us > 0 && now_us > g_last_progress_time_us) {
        double elapsed_sec = (double)(now_us - g_last_progress_time_us) / 1000000.0;
        if (elapsed_sec > 0.2) {
            g_load_speed_kbps = (float)((dlnow / 1024.0) / elapsed_sec);
        }
    }
    return 0;
}

#endif /* EONSURF_NET_PROGRESS_H */
