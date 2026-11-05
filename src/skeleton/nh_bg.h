/*
 * nh_bg — Neon Horizon animated background for OpenOrbis PS4 homebrew.
 *
 * Plain C99, no dependencies (pthreads optional). Renders the scene from the
 * Neon Horizon web generator straight into a 32-bit frame buffer:
 * gradient sky, stars, striped retro sun (animated), wireframe mountains,
 * perspective grid, wireframe palm trees, vignette, contrast/brightness.
 *
 * The heavy work (glows, mountains, palms) is done once per palette and cached.
 * Each frame is a memcpy of the cached image plus a redraw of the sun area,
 * so it costs about 1-3 ms at 1920x1080.
 */
#ifndef NH_BG_H
#define NH_BG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Colors are 0xRRGGBB. Field names match the web generator's color pickers. */
typedef struct {
    uint32_t skyTop, horizon, sunTop, sunBottom, grid, floor, nebulaA, nebulaB, objects;
} nh_palette;

typedef struct {
    int      w, h;            /* output size in pixels                         */
    uint32_t seed;            /* layout seed (stars, mountains, palms)         */
    int      sun, grid, mountains, palms, stars, vignette;   /* 0 / 1 toggles  */
    float    horizon;         /* horizon height, 0..1 of screen height         */
    float    sunSize, gridDensity, mountainH, palmCount, palmSpread;
    float    starDensity, glow;
    float    sunSpeed;        /* stripe scroll speed multiplier (1 = web look) */
    float    hue, sat, contrast, bright;  /* "Adjust" section                  */
    float    driveSpeed;      /* pppEmu: grid lines moving towards you (web "speed", 0 = still) */
} nh_settings;

typedef enum {
    NH_ARGB8888 = 0,  /* 0xAARRGGBB in a uint32 - OpenOrbis video out, SDL ARGB8888 */
    NH_ABGR8888 = 1   /* 0xAABBGGRR in a uint32 - SDL ABGR8888 / RGBA byte order    */
} nh_pixfmt;

/* Your exported settings ("Arctic", seed 642793). */
const nh_settings *nh_default_settings(void);
/* Your exported palette, before any per-system hue change. */
const nh_palette  *nh_base_palette(void);

/* ---------- per-system palettes ---------------------------------------- */
typedef struct {
    const char *id;     /* what you pass to nh_bg_set_system(), e.g. "snes"   */
    const char *name;   /* display name                                       */
    float hue;          /* target hue in degrees (your design is rotated to it) */
    float sat;          /* saturation multiplier (0 = chrome / grey)          */
} nh_system;

int               nh_system_count(void);
const nh_system  *nh_system_at(int i);
/* Palette for a system id. Unknown ids return the "default" palette. */
nh_palette        nh_system_palette(const char *id);

/* ---------- low level: one built scene per palette --------------------- */
typedef struct nh_layers nh_layers;
nh_layers *nh_build(const nh_settings *s, const nh_palette *p, nh_pixfmt fmt); /* slow, thread-safe */
void       nh_draw (const nh_layers *L, double t, uint32_t *dst, int pitchPixels);   /* fast    */
void       nh_free (nh_layers *L);

/* ---------- high level: background manager ----------------------------- */
typedef struct nh_bg nh_bg;

/* Creates the manager and builds the "default" palette (blocking). */
nh_bg *nh_bg_create(const nh_settings *s, nh_pixfmt fmt);
/* Switch playlist palette. Cached systems switch at once; new ones are built
 * on a worker thread (or right away if NH_THREADS=0), then crossfade in.     */
void   nh_bg_set_system(nh_bg *bg, const char *systemId);
/* Same, with any palette you like (key is used for caching). */
void   nh_bg_set_palette(nh_bg *bg, const char *key, const nh_palette *p);
/* Optional: build palettes at boot so first switches are instant (blocking). */
void   nh_bg_prewarm(nh_bg *bg, const char *const *systemIds, int count);
/* pppEmu: like nh_bg_create, without waiting: "firstId" is built on the worker thread. */
nh_bg *nh_bg_create_async(const nh_settings *s, nh_pixfmt fmt, const char *firstId);
/* Draw one frame. dt = seconds since last call. pitch in pixels (not bytes).
 * pppEmu: returns 0 (nothing drawn) while no palette is built yet, else 1. */
int    nh_bg_render(nh_bg *bg, double dt, uint32_t *dst, int pitchPixels);
/* pppEmu: same, for a buffer kept from frame to frame (a texture): only what changed is
 * redrawn, rows [*y0, *y1) are the ones to upload (none if *y0 == *y1). Pass the same
 * buffer every time. */
int    nh_bg_render_rows(nh_bg *bg, double dt, uint32_t *dst, int pitchPixels, int *y0, int *y1);
void   nh_bg_destroy(nh_bg *bg);

#ifdef __cplusplus
}
#endif
#endif
