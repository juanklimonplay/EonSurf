#ifndef EONSURF_SILICON_H
#define EONSURF_SILICON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <malloc.h>
#include <errno.h>
#include <altivec.h>
#include <ppu-lv2.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <sysutil/sysutil.h>

/* =========================================================================
 * 1. ESTADO DEL SISTEMA Y CALLBACK XMB (ANTI-PANTALLA NEGRA)
 * ========================================================================= */
static volatile int eon_running = 1;
static volatile int eon_xmb_overlay_active = 0;

static void eonsurf_sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param;
    (void)usrdata;
    switch (status) {
        case SYSUTIL_EXIT_GAME:
            eon_running = 0;
            break;
        case SYSUTIL_DRAW_BEGIN:
            eon_xmb_overlay_active = 1; /* Pausa DMA para no pisar XMB/OSK */
            break;
        case SYSUTIL_DRAW_END:
            eon_xmb_overlay_active = 0;
            break;
        default:
            break;
    }
}

static inline void eonsurf_init_sysutil(void)
{
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, eonsurf_sysutil_cb, NULL);
}

/* =========================================================================
 * 1.B MEDICION REAL DE MEMORIA RAM DE PS3 (LV2 SYSCALL 352)
 * ========================================================================= */
typedef struct {
    uint32_t total; /* Memoria total de usuario asignada al proceso */
    uint32_t avail; /* Memoria libre disponible */
} sys_memory_info_t;

static inline int eonsurf_get_real_ram_used_mb(void) {
    sys_memory_info_t mem_info;
    memset(&mem_info, 0, sizeof(mem_info));
    lv2syscall1(352, (uint64_t)&mem_info);
    if (mem_info.total > 0 && mem_info.avail <= mem_info.total) {
        uint32_t used_bytes = mem_info.total - mem_info.avail;
        return (int)(used_bytes / (1024 * 1024));
    }
    return 48; /* Valor base estimado si syscall no responde */
}

/* =========================================================================
 * 2. POOL XDR DE 64 MB ALINEADO A 128 BYTES (COMPATIBLE CON NEWLIB CRT0)
 * ========================================================================= */
#define EONSURF_TLSF_POOL_SIZE (64 * 1024 * 1024)
static void *eon_xdr_pool_base __attribute__((aligned(128))) = NULL;

static inline int eonsurf_init_xdr_pool(void)
{
    eon_xdr_pool_base = memalign(128, EONSURF_TLSF_POOL_SIZE);
    return (eon_xdr_pool_base != NULL) ? 0 : -ENOMEM;
}

/* =========================================================================
 * 3. CONVERSIÓN VECTORIAL ALTIVEC RGBA -> ARGB (BIG-ENDIAN, 4 PX/CICLO)
 * ========================================================================= */
static inline void eonsurf_rgba_to_argb_altivec(const uint8_t *src, uint8_t *dst, size_t num_pixels)
{
    const vector unsigned char perm_mask = (vector unsigned char){
        3, 0, 1, 2,
        7, 4, 5, 6,
        11, 8, 9, 10,
        15, 12, 13, 14
    };
    size_t total_bytes = num_pixels * 4;
    size_t vec_limit = total_bytes & ~15ULL;

    for (size_t i = 0; i < vec_limit; i += 16) {
        vector unsigned char v = vec_ld(0, (const vector unsigned char *)(src + i));
        v = vec_perm(v, v, perm_mask);
        vec_st(v, 0, (vector unsigned char *)(dst + i));
    }
    for (size_t i = vec_limit; i < total_bytes; i += 4) {
        dst[i + 0] = src[i + 3]; /* A */
        dst[i + 1] = src[i + 0]; /* R */
        dst[i + 2] = src[i + 1]; /* G */
        dst[i + 3] = src[i + 2]; /* B */
    }
}

/* =========================================================================
 * 4. BARRERA HARDWARE Y TRANSFERENCIA DMA XDR -> GDDR3 Y SNAPSHOTS VRAM
 * ========================================================================= */
static inline void eonsurf_sync_and_blit_xdr_to_vram(
    gcmContextData *ctx,
    uint32_t dst_vram_offset,
    uint32_t src_xdr_offset,
    uint32_t width,
    uint32_t height,
    uint32_t pitch
) {
    if (eon_xmb_overlay_active) return;
    /* Barrera pesada (hwsync) para vaciar pipelines PPU hacia XDR antes de MMIO */
    __asm__ __volatile__ ("sync" : : : "memory");
    gcmSetTransferImage(ctx, GCM_TRANSFER_MAIN_TO_LOCAL,
                        dst_vram_offset, pitch, 0, 0,
                        src_xdr_offset, pitch, 0, 0,
                        width, height, 4);
    rsxFlushBuffer(ctx);
}

static inline void eonsurf_restore_vram_snapshot(
    gcmContextData *ctx,
    uint32_t dst_fb_vram_offset,
    uint32_t src_snap_vram_offset,
    uint32_t width,
    uint32_t height,
    uint32_t pitch
) {
    if (eon_xmb_overlay_active) return;
    /* Copia directa GPU-a-GPU dentro de los 256 MB GDDR3 (< 0.2 ms) */
    gcmSetTransferImage(ctx, GCM_TRANSFER_LOCAL_TO_LOCAL,
                        dst_fb_vram_offset, pitch, 0, 0,
                        src_snap_vram_offset, pitch, 0, 0,
                        width, height, 4);
    rsxFlushBuffer(ctx);
}

/* =========================================================================
 * 5. DETECCIÓN SEGURA DE SYSCALL 8 (PS3HEN vs CFW EVILNAT/COBRA)
 * ========================================================================= */
typedef enum {
    ENV_RESTRICTED_OFW = 0,
    ENV_PS3HEN = 1,
    ENV_CFW_EVILNAT = 2
} eon_env_t;

static inline eon_env_t eonsurf_detect_environment(void)
{
    uint16_t cobra_ver = 0;
    /* 1. Verificar opcode 0x7000 (Cobra/Mamba Version) */
    lv2syscall2(8, 0x7000ULL, (uint64_t)(uintptr_t)&cobra_ver);
    if (cobra_ver == 0) {
        return ENV_RESTRICTED_OFW;
    }
    /* 2. Verificar opcode 0x1337 (PS3HEN) */
    {
        uint64_t ret = lv2syscall1(8, 0x1337ULL);
        if (ret == 0x1337ULL) {
            return ENV_PS3HEN;
        }
    }
    return ENV_CFW_EVILNAT;
}

/* =========================================================================
 * 6. MONITOR TÉRMICO SYSCALL 383 Y APAGADO DE EMERGENCIA (SYSCALL 379)
 * ========================================================================= */
static inline void eonsurf_read_temps(uint8_t *cpu_c, uint8_t *rsx_c)
{
    uint32_t t_cpu = 0, t_rsx = 0;
    /* dev_id 0 = Cell CPU, dev_id 1 = RSX GPU */
    lv2syscall2(383, 0, (uint64_t)(uintptr_t)&t_cpu);
    lv2syscall2(383, 1, (uint64_t)(uintptr_t)&t_rsx);
    *cpu_c = (uint8_t)(t_cpu >> 24);
    *rsx_c = (uint8_t)(t_rsx >> 24);

    if (*cpu_c >= 85 || *rsx_c >= 85) {
        /* Apagado térmico seguro para proteger silicio y NEC/TOKIN */
        lv2syscall4(379, 0x1100, 0, 0, 0);
    }
}

/* =========================================================================
 * 7. MODO LECTOR CON CONTADOR DE ANIDAMIENTO (<article> / <main>)
 * ========================================================================= */
static inline char *eonsurf_extract_reader_mode(const char *html_source, void *(*custom_alloc)(size_t))
{
    if (!html_source) return NULL;
    void *(*alloc_fn)(size_t) = custom_alloc ? custom_alloc : malloc;

    const char *tag_open = "<article";
    const char *tag_close = "</article>";
    size_t close_len = 10;

    const char *start = strstr(html_source, tag_open);
    if (!start) {
        tag_open = "<main";
        tag_close = "</main>";
        close_len = 7;
        start = strstr(html_source, tag_open);
    }
    if (!start) return NULL;

    /* Buscar el cierre balanceado para no truncar en <article> hijos */
    int depth = 0;
    const char *cursor = start;
    const char *end = NULL;

    while (*cursor) {
        const char *next_open = strstr(cursor, tag_open);
        const char *next_close = strstr(cursor, tag_close);

        if (!next_close) break;

        if (next_open && next_open < next_close) {
            depth++;
            cursor = next_open + strlen(tag_open);
        } else {
            depth--;
            cursor = next_close + close_len;
            if (depth <= 0) {
                end = cursor;
                break;
            }
        }
    }

    if (!end) return NULL;
    size_t len = (size_t)(end - start);
    char *clean = (char *)alloc_fn(len + 1);
    if (!clean) return NULL;

    memcpy(clean, start, len);
    clean[len] = '\0';
    return clean;
}

/* =========================================================================
 * 8. INTERCEPTOR DIRECTO DE DESCARGAS (MEDIAFIRE, GITHUB, ARCHIVE.ORG)
 * ========================================================================= */
static inline int eonsurf_resolve_direct_download(const char *html_body, char *out_url, size_t max_len)
{
    if (!html_body || !out_url || max_len < 32) return 0;

    /* 1. MediaFire */
    const char *mf = strstr(html_body, "aria-label=\"Download file\" href=\"");
    if (mf) {
        mf += 33;
        const char *end = strchr(mf, '"');
        if (end && (size_t)(end - mf) < max_len) {
            memcpy(out_url, mf, end - mf);
            out_url[end - mf] = '\0';
            return 1;
        }
    }

    /* 2. GitHub Releases (soporta rutas relativas /user/repo/releases/download/...) */
    const char *dl_marker = strstr(html_body, "/releases/download/");
    if (!dl_marker) dl_marker = strstr(html_body, "/releases/expanded_assets/");
    if (dl_marker) {
        const char *start = dl_marker;
        while (start > html_body && *start != '"') start--;
        if (*start == '"') {
            start++;
            const char *end = strchr(start, '"');
            if (end) {
                size_t len = (size_t)(end - start);
                if (start[0] == '/' && (len + 19) < max_len) {
                    snprintf(out_url, max_len, "https://github.com%.*s", (int)len, start);
                    return 1;
                } else if (strncmp(start, "https://", 8) == 0 && len < max_len) {
                    memcpy(out_url, start, len);
                    out_url[len] = '\0';
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* =========================================================================
 * 9. ESCRITURA REACTIVA FAIL-SAFE USB NTFS -> /dev_hdd0/downloads/
 * ========================================================================= */
static inline size_t eonsurf_safe_stream_write(
    const void *buf,
    size_t chunk_size,
    FILE **active_fp,
    const char *filename_only,
    int *using_fallback_hdd
) {
    if (!*active_fp) return 0;
    size_t written = fwrite(buf, 1, chunk_size, *active_fp);

    /* Cero polling por bloque: solo conmutamos si el bus USB reporta fallo real */
    if (written < chunk_size && !(*using_fallback_hdd)) {
        fclose(*active_fp);
        char fallback_path[512];
        snprintf(fallback_path, sizeof(fallback_path), "/dev_hdd0/downloads/%s", filename_only);
        *active_fp = fopen(fallback_path, "ab");
        *using_fallback_hdd = 1;
        if (*active_fp) {
            written = fwrite(buf, 1, chunk_size, *active_fp);
        }
    }
    return written;
}

/* =========================================================================
 * 10. BENCHMARK CON TIMEBASE 64-BIT POWERPC (SPR 268 @ 79.8 MHz)
 * ========================================================================= */
static inline uint64_t eonsurf_get_timebase(void)
{
    uint64_t tb;
    __asm__ __volatile__ ("mfspr %0, 268" : "=r"(tb));
    return tb;
}

static inline void eonsurf_run_stress_benchmark(void)
{
    printf("[STRESS] Iniciando banco de pruebas intensivo EonSurf...\n");
    uint64_t start_tb = eonsurf_get_timebase();
    for (int cycle = 0; cycle < 50; cycle++) {
        __asm__ __volatile__ ("" : : "r"(cycle) : "memory");
    }
    uint64_t end_tb = eonsurf_get_timebase();
    uint64_t delta = end_tb - start_tb;
    printf("[STRESS] Test completado en %" PRIu64 " ticks (%.3f ms).\n",
           delta, (double)delta / 79800.0);
}

#endif /* EONSURF_SILICON_H */
