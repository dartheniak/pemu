/*
 * nh_bg.c — Neon Horizon animated background (see nh_bg.h).
 * Port of the web generator's canvas renderer to a small float software rasterizer.
 *
 * Build options:
 *   -DNH_THREADS=0   build new palettes on the calling thread instead of a worker
 *   -DNH_CACHE=n     number of palettes kept in memory (default 6, ~9 MB each at 1080p)
 *   -DNH_FADE=s      crossfade length in seconds (default 0.45)
 */
#include "nh_bg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef NH_THREADS
#define NH_THREADS 1
#endif
#ifndef NH_CACHE
#define NH_CACHE 12     /* pppEmu: all 11 console pages stay built (was 6) */
#endif
#ifndef NH_FADE
#define NH_FADE 0.45
#endif
#if NH_THREADS
#include <pthread.h>
#endif

#define NH_PI 3.14159265358979323846

/* ======================================================================
 * Settings and palettes
 * ==================================================================== */
static const nh_settings DEFAULT_SETTINGS = {
    1920, 1080, 642793,
    /* sun grid mountains palms stars vignette */
    1, 1, 1, 1, 1, 1,
    0.71f,            /* horizon      */
    0.17f, 5.0f, 0.85f, 2.0f, 1.6f,   /* sunSize gridDensity mountainH palmCount palmSpread */
    0.1f, 1.6f,       /* starDensity glow */
    1.0f,             /* sunSpeed     */
    0.0f, 0.75f, 1.3f, 1.05f,         /* hue sat contrast bright */
    0.2f              /* driveSpeed (pppEmu) */
};

/* pppEmu: grid scroll, in grid depth units per second for driveSpeed 1 */
#ifndef NH_DRIVE_RATE
#define NH_DRIVE_RATE 5.0
#endif
static const nh_palette BASE_PALETTE = {   /* "Arctic" */
    0x020b1f, 0x4f9dff, 0xffffff, 0x7fd8ff, 0x7fd8ff, 0x030c1c, 0x4f9dff, 0xc38cff, 0xe0f7ff
};
#define BASE_HUE 213.0f   /* hue of the Arctic horizon color */

static const nh_system SYSTEMS[] = {
    { "default", "All games (your Arctic setup)", 213, 1.00f },
    { "nes",     "NES / Famicom",                 355, 1.00f },
    { "sms",     "Master System",                  15, 1.00f },
    { "pce",     "PC Engine / TurboGrafx-16",      30, 1.00f },
    { "neogeo",  "Neo Geo",                        45, 1.00f },
    { "gb",      "Game Boy",                       80, 0.70f },
    { "n64",     "Nintendo 64",                   135, 1.00f },
    { "gg",      "Game Gear",                     172, 1.00f },
    { "genesis", "Mega Drive / Genesis",          196, 1.15f },
    { "segacd",  "Sega CD / Mega-CD",             108, 1.00f },   /* pppEmu */
    { "psx",     "PlayStation",                   222, 0.30f },
    { "gba",     "Game Boy Advance",              242, 1.00f },
    { "snes",    "SNES / Super Famicom",          266, 1.00f },
    { "gbc",     "Game Boy Color",                292, 1.00f },
    { "arcade",  "Arcade / MAME",                 322, 1.00f },
    { "nds",     "Nintendo DS",                     0, 0.00f },
};
#define NSYS ((int)(sizeof(SYSTEMS) / sizeof(SYSTEMS[0])))

const nh_settings *nh_default_settings(void) { return &DEFAULT_SETTINGS; }
const nh_palette  *nh_base_palette(void)     { return &BASE_PALETTE; }
int                nh_system_count(void)     { return NSYS; }
const nh_system   *nh_system_at(int i)       { return (i >= 0 && i < NSYS) ? &SYSTEMS[i] : 0; }

/* ======================================================================
 * Color helpers (rounded exactly like the web version's hex strings)
 * ==================================================================== */
typedef struct { int r, g, b; } col;
static const col WHITE = { 255, 255, 255 };

static double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
static double clamp01(double v) { return clampd(v, 0, 1); }
static double smooth(double a, double b, double x) { double t = clamp01((x - a) / (b - a)); return t * t * (3 - 2 * t); }
static double jround(double x) { return floor(x + 0.5); }
static col hexc(uint32_t h) { col c = { (int)((h >> 16) & 255), (int)((h >> 8) & 255), (int)(h & 255) }; return c; }
static uint32_t chex(col c) { return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b; }
static col mixc(col a, col b, double t) {
    col c = { (int)jround(a.r + (b.r - a.r) * t), (int)jround(a.g + (b.g - a.g) * t), (int)jround(a.b + (b.b - a.b) * t) };
    return c;
}
static void rgb2hsl(col c, double *H, double *S, double *L) {
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    double mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b)), l = (mx + mn) / 2, h = 0, s = 0;
    if (mx != mn) {
        double d = mx - mn;
        s = l > .5 ? d / (2 - mx - mn) : d / (mx + mn);
        if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
        else if (mx == g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;
        h /= 6;
    }
    *H = h; *S = s; *L = l;
}
static double hue2(double p, double q, double t) {
    t = fmod(fmod(t, 1) + 1, 1);
    return t < 1. / 6 ? p + (q - p) * 6 * t : t < .5 ? q : t < 2. / 3 ? p + (q - p) * (2. / 3 - t) * 6 : p;
}
static col hsl2col(double h, double s, double l) {
    h = fmod(fmod(h, 1) + 1, 1); s = clamp01(s); l = clamp01(l);
    double q = l < .5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
    col c = { (int)jround(hue2(p, q, h + 1. / 3) * 255), (int)jround(hue2(p, q, h) * 255), (int)jround(hue2(p, q, h - 1. / 3) * 255) };
    return c;
}
static nh_palette adjust_palette(const nh_palette *in, double hueDeg, double sat) {
    nh_palette o = *in;
    if (hueDeg == 0 && sat == 1) return o;
    uint32_t *v = (uint32_t *)&o;
    for (int i = 0; i < 9; i++) {
        double h, s, l; rgb2hsl(hexc(v[i]), &h, &s, &l);
        v[i] = chex(hsl2col(h + hueDeg / 360.0, s * sat, l));
    }
    return o;
}
nh_palette nh_system_palette(const char *id) {
    const nh_system *s = &SYSTEMS[0];
    for (int i = 0; id && i < NSYS; i++) if (!strcmp(SYSTEMS[i].id, id)) { s = &SYSTEMS[i]; break; }
    return adjust_palette(&BASE_PALETTE, s->hue - BASE_HUE, s->sat);
}

/* ======================================================================
 * Seeded RNG and 1-D noise (bit-exact with the web version)
 * ==================================================================== */
typedef struct { uint32_t a; } rng_t;
static rng_t rng_new(double s) {
    double m = fmod(s * 2654435761.0, 4294967296.0);
    if (m < 0) m += 4294967296.0;
    rng_t r = { (uint32_t)m };
    return r;
}
static double rnd(rng_t *r) {
    r->a += 0x6D2B79F5u;
    uint32_t t = r->a;
    t = (t ^ (t >> 15)) * (1u | t);
    t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}
typedef struct { double v[64]; } noise_t;
static void noise_init(noise_t *n, rng_t *r) { for (int i = 0; i < 64; i++) n->v[i] = rnd(r); }
static double noise_at(const noise_t *n, double x) {
    int i = (int)floor(x); double f = x - i;
    double a = n->v[((i % 64) + 64) % 64], b = n->v[(((i + 1) % 64) + 64) % 64], s = (1 - cos(f * NH_PI)) / 2;
    return a + (b - a) * s;
}

/* ======================================================================
 * Paints
 * ==================================================================== */
enum { P_SOLID, P_VGRAD, P_RGRAD };
typedef struct { float pos, r, g, b, a; } stop_t;
typedef struct {
    int kind; float r, g, b, a;
    float y0, y1, cx, cy, r0, r1;
    int ns; stop_t st[4];
} paint_t;

static paint_t solid(col c, double a) {
    paint_t p; memset(&p, 0, sizeof p);
    p.kind = P_SOLID; p.r = c.r / 255.f; p.g = c.g / 255.f; p.b = c.b / 255.f; p.a = (float)a;
    return p;
}
static paint_t vgrad(double y0, double y1) { paint_t p; memset(&p, 0, sizeof p); p.kind = P_VGRAD; p.y0 = (float)y0; p.y1 = (float)y1; return p; }
static paint_t rgrad(double cx, double cy, double r0, double r1) {
    paint_t p; memset(&p, 0, sizeof p); p.kind = P_RGRAD;
    p.cx = (float)cx; p.cy = (float)cy; p.r0 = (float)r0; p.r1 = (float)r1; return p;
}
static void addstop(paint_t *p, double pos, col c, double a) {
    stop_t s = { (float)pos, c.r / 255.f, c.g / 255.f, c.b / 255.f, (float)a };
    p->st[p->ns++] = s;
}
static void paint_eval(const paint_t *p, int x, int y, float *o) {
    if (p->kind == P_SOLID) { o[0] = p->r; o[1] = p->g; o[2] = p->b; o[3] = p->a; return; }
    float t;
    if (p->kind == P_VGRAD) t = (y + .5f - p->y0) / (p->y1 - p->y0);
    else { float dx = x + .5f - p->cx, dy = y + .5f - p->cy; t = (sqrtf(dx * dx + dy * dy) - p->r0) / (p->r1 - p->r0); }
    const stop_t *a = &p->st[0], *b = &p->st[p->ns - 1];
    if (t <= a->pos) { o[0] = a->r; o[1] = a->g; o[2] = a->b; o[3] = a->a; return; }
    if (t >= b->pos) { o[0] = b->r; o[1] = b->g; o[2] = b->b; o[3] = b->a; return; }
    for (int i = 0; i + 1 < p->ns; i++) {
        if (t >= p->st[i].pos && t <= p->st[i + 1].pos) { a = &p->st[i]; b = &p->st[i + 1]; break; }
    }
    float f = (b->pos > a->pos) ? (t - a->pos) / (b->pos - a->pos) : 0;
    o[0] = a->r + (b->r - a->r) * f; o[1] = a->g + (b->g - a->g) * f;
    o[2] = a->b + (b->b - a->b) * f; o[3] = a->a + (b->a - a->a) * f;
}

/* ======================================================================
 * Rasterizer: coverage mask + composite onto 1-2 targets
 * ==================================================================== */
typedef struct { float *px; int x0, y0, w, h, ch; } tgt_t;   /* ch 3 = opaque RGB, 4 = premultiplied RGBA */

typedef struct {
    int W, H; double u, hy;
    float *M; int mx0, my0, mx1, my1;     /* mask + dirty box (exclusive max) */
    tgt_t tg[2]; int ntg;
    float clipY;                          /* >0: clip drawing to y < clipY */
} B;

static void mask_touch(B *b, int x0, int y0, int x1, int y1) {
    if (x0 < b->mx0) b->mx0 = x0;
    if (y0 < b->my0) b->my0 = y0;
    if (x1 > b->mx1) b->mx1 = x1;
    if (y1 > b->my1) b->my1 = y1;
}
static void mask_reset(B *b) {
    if (b->mx1 > b->mx0 && b->my1 > b->my0)
        for (int y = b->my0; y < b->my1; y++) memset(b->M + (size_t)y * b->W + b->mx0, 0, sizeof(float) * (b->mx1 - b->mx0));
    b->mx0 = b->W; b->my0 = b->H; b->mx1 = 0; b->my1 = 0;
}
static void mset(B *b, int x, int y, float c) {
    if (c <= 0) return;
    if (c > 1) c = 1;
    float *m = b->M + (size_t)y * b->W + x;
    if (c > *m) *m = c;
}
static double overlap(double a0, double a1, double b0, double b1) {
    double v = fmin(a1, b1) - fmax(a0, b0);
    return v > 0 ? v : 0;
}

static void mask_rect(B *b, double x0, double y0, double x1, double y1) {
    int ya = (int)floor(fmax(0, y0)), yb = (int)ceil(fmin(b->H, y1));
    int xa = (int)floor(fmax(0, x0)), xb = (int)ceil(fmin(b->W, x1));
    if (ya >= yb || xa >= xb) return;
    for (int y = ya; y < yb; y++) {
        double cy = overlap(y, y + 1, y0, y1);
        for (int x = xa; x < xb; x++) mset(b, x, y, (float)(cy * overlap(x, x + 1, x0, x1)));
    }
    mask_touch(b, xa, ya, xb, yb);
}

/* stroke segment with round caps; width w */
static void mask_seg(B *b, double ax, double ay, double bx, double by, double w) {
    double r = w * .5, R = r + 1.0;
    double dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy, L = sqrt(L2);
    int y0 = (int)floor(fmin(ay, by) - R), y1 = (int)ceil(fmax(ay, by) + R);
    if (y0 < 0) y0 = 0;
    if (y1 > b->H) y1 = b->H;
    if (y0 >= y1) return;
    double nx = 0, ny = 0;
    if (L > 1e-9) { nx = -dy / L; ny = dx / L; }
    int tx0 = b->W, tx1 = 0;
    for (int y = y0; y < y1; y++) {
        double yc = y + .5, xl = fmin(ax, bx) - R, xr = fmax(ax, bx) + R;
        if (L > 1e-9 && fabs(nx) > 1e-3) {
            double xc = ax - (yc - ay) * ny / nx, hw = R / fabs(nx);
            xl = fmax(xl, xc - hw); xr = fmin(xr, xc + hw);
        }
        int xa = (int)floor(xl), xb = (int)ceil(xr);
        if (xa < 0) xa = 0;
        if (xb > b->W - 1) xb = b->W - 1;
        for (int x = xa; x <= xb; x++) {
            double px = x + .5, t = L2 > 0 ? ((px - ax) * dx + (yc - ay) * dy) / L2 : 0;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            double ex = ax + t * dx - px, ey = ay + t * dy - yc, d = sqrt(ex * ex + ey * ey);
            double c = fmin(d + r, .5) - fmax(d - r, -.5);
            if (c > 0) { mset(b, x, y, (float)c); if (x < tx0) tx0 = x; if (x + 1 > tx1) tx1 = x + 1; }
        }
    }
    if (tx1 > tx0) mask_touch(b, tx0, y0, tx1, y1);
}

/* filled circle */
static void mask_circle(B *b, double cx, double cy, double r) {
    int x0 = (int)floor(cx - r - 1), x1 = (int)ceil(cx + r + 1), y0 = (int)floor(cy - r - 1), y1 = (int)ceil(cy + r + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > b->W) x1 = b->W;
    if (y1 > b->H) y1 = b->H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        double c;
        if (r >= 1.5) { double d = hypot(x + .5 - cx, y + .5 - cy); c = r + .5 - d; }
        else {
            int n = 0;
            for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
                double ddx = x + (i + .5) / 4 - cx, ddy = y + (j + .5) / 4 - cy;
                if (ddx * ddx + ddy * ddy <= r * r) n++;
            }
            c = n / 16.0;
        }
        mset(b, x, y, (float)c);
    }
    mask_touch(b, x0, y0, x1, y1);
}

/* circle outline */
static void mask_ring(B *b, double cx, double cy, double r, double w) {
    double hw = w * .5, R = r + hw + 1;
    int x0 = (int)floor(cx - R), x1 = (int)ceil(cx + R), y0 = (int)floor(cy - R), y1 = (int)ceil(cy + R);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > b->W) x1 = b->W;
    if (y1 > b->H) y1 = b->H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        double d = fabs(hypot(x + .5 - cx, y + .5 - cy) - r);
        mset(b, x, y, (float)(fmin(d + hw, .5) - fmax(d - hw, -.5)));
    }
    mask_touch(b, x0, y0, x1, y1);
}

/* filled polygon, nonzero winding, 5 sub-scanlines with exact horizontal coverage */
static int cmp_cross(const void *a, const void *b) {
    const double *x = (const double *)a, *y = (const double *)b;
    return x[0] < y[0] ? -1 : x[0] > y[0];
}
static void mask_poly(B *b, const double *pts, int n) {
    double ymin = 1e30, ymax = -1e30, xmin = 1e30, xmax = -1e30;
    for (int i = 0; i < n; i++) {
        ymin = fmin(ymin, pts[2 * i + 1]); ymax = fmax(ymax, pts[2 * i + 1]);
        xmin = fmin(xmin, pts[2 * i]);     xmax = fmax(xmax, pts[2 * i]);
    }
    int y0 = (int)floor(fmax(0, ymin)), y1 = (int)ceil(fmin(b->H, ymax));
    int x0 = (int)floor(fmax(0, xmin)), x1 = (int)ceil(fmin(b->W, xmax));
    if (y0 >= y1 || x0 >= x1) return;
    const int S = 5;
    float *acc = (float *)calloc((size_t)(x1 - x0), sizeof(float));
    double *cr = (double *)malloc(sizeof(double) * 2 * (size_t)n);
    for (int y = y0; y < y1; y++) {
        memset(acc, 0, sizeof(float) * (size_t)(x1 - x0));
        for (int k = 0; k < S; k++) {
            double yc = y + (k + .5) / S; int nc = 0;
            for (int i = 0; i < n; i++) {
                double ax = pts[2 * i], ay = pts[2 * i + 1];
                double bx = pts[2 * ((i + 1) % n)], by = pts[2 * ((i + 1) % n) + 1];
                if ((ay <= yc && by > yc) || (by <= yc && ay > yc)) {
                    cr[2 * nc] = ax + (yc - ay) / (by - ay) * (bx - ax);
                    cr[2 * nc + 1] = by > ay ? 1 : -1;
                    nc++;
                }
            }
            qsort(cr, (size_t)nc, sizeof(double) * 2, cmp_cross);
            int wind = 0; double start = 0;
            for (int i = 0; i < nc; i++) {
                int was = wind; wind += (int)cr[2 * i + 1];
                if (!was && wind) start = cr[2 * i];
                else if (was && !wind) {
                    double xa = fmax(start, x0), xb = fmin(cr[2 * i], x1);
                    for (int px = (int)floor(xa); px < (int)ceil(xb); px++) {
                        double c = overlap(px, px + 1, xa, xb);
                        if (c > 0) acc[px - x0] += (float)(c / S);
                    }
                }
            }
        }
        for (int x = x0; x < x1; x++) mset(b, x, y, acc[x - x0]);
    }
    free(acc); free(cr);
    mask_touch(b, x0, y0, x1, y1);
}

/* Gaussian blur approximated by 3 box blurs (zero outside) */
static void box_h(const float *s, float *d, int w, int h, int r) {
    float inv = 1.f / (2 * r + 1);
    for (int y = 0; y < h; y++) {
        const float *si = s + (size_t)y * w; float *di = d + (size_t)y * w; float acc = 0;
        for (int x = 0; x <= r && x < w; x++) acc += si[x];
        for (int x = 0; x < w; x++) {
            di[x] = acc * inv;
            if (x + r + 1 < w) acc += si[x + r + 1];
            if (x - r >= 0) acc -= si[x - r];
        }
    }
}
static void box_v(const float *s, float *d, int w, int h, int r) {
    float inv = 1.f / (2 * r + 1);
    for (int x = 0; x < w; x++) {
        float acc = 0;
        for (int y = 0; y <= r && y < h; y++) acc += s[(size_t)y * w + x];
        for (int y = 0; y < h; y++) {
            d[(size_t)y * w + x] = acc * inv;
            if (y + r + 1 < h) acc += s[(size_t)(y + r + 1) * w + x];
            if (y - r >= 0) acc -= s[(size_t)(y - r) * w + x];
        }
    }
}
static void blur(float *buf, int w, int h, double sigma) {
    if (sigma < .5) return;
    int n = 3, sz[3];
    double wIdeal = sqrt(12 * sigma * sigma / n + 1);
    int wl = (int)floor(wIdeal); if (wl % 2 == 0) wl--;
    int wu = wl + 2;
    double mIdeal = (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4.0 * wl - 4);
    int m = (int)jround(mIdeal);
    for (int i = 0; i < n; i++) sz[i] = i < m ? wl : wu;
    float *tmp = (float *)malloc(sizeof(float) * (size_t)w * h);
    for (int i = 0; i < n; i++) { int r = (sz[i] - 1) / 2; box_h(buf, tmp, w, h, r); box_v(tmp, buf, w, h, r); }
    free(tmp);
}

static void put(B *b, int x, int y, float r, float g, float bl, float a) {
    if (a <= 0) return;
    if (a > 1) a = 1;
    for (int i = 0; i < b->ntg; i++) {
        tgt_t *t = &b->tg[i];
        int lx = x - t->x0, ly = y - t->y0;
        if (lx < 0 || ly < 0 || lx >= t->w || ly >= t->h) continue;
        float *p = t->px + ((size_t)ly * t->w + lx) * t->ch, k = 1 - a;
        p[0] = r * a + p[0] * k; p[1] = g * a + p[1] * k; p[2] = bl * a + p[2] * k;
        if (t->ch == 4) p[3] = a + p[3] * k;
    }
}
static float clipcov(B *b, int y) { return b->clipY > 0 ? (float)clamp01(b->clipY - y) : 1.f; }

/* Draw the current mask with paint p at global alpha ga. Optional glow (canvas shadowBlur). */
static void composite(B *b, const paint_t *p, double ga, const col *shadow, double blurPx, int shapeToo) {
    if (b->mx1 <= b->mx0 || b->my1 <= b->my0) { mask_reset(b); return; }
    float o[4];
    if (shadow && blurPx > 0) {
        double sigma = blurPx / 2;
        int pad = (int)ceil(3 * sigma) + 1;
        int x0 = b->mx0 - pad, y0 = b->my0 - pad, x1 = b->mx1 + pad, y1 = b->my1 + pad;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > b->W) x1 = b->W;
        if (y1 > b->H) y1 = b->H;
        int w = x1 - x0, h = y1 - y0;
        float *s = (float *)calloc((size_t)w * h, sizeof(float));
        for (int y = b->my0; y < b->my1; y++) for (int x = b->mx0; x < b->mx1; x++) {
            float m = b->M[(size_t)y * b->W + x];
            if (m <= 0) continue;
            paint_eval(p, x, y, o);
            s[(size_t)(y - y0) * w + (x - x0)] = m * o[3] * (float)ga;
        }
        blur(s, w, h, sigma);
        float sr = shadow->r / 255.f, sg = shadow->g / 255.f, sb = shadow->b / 255.f;
        for (int y = y0; y < y1; y++) {
            float cc = clipcov(b, y);
            if (cc <= 0) continue;
            for (int x = x0; x < x1; x++) {
                float a = s[(size_t)(y - y0) * w + (x - x0)] * cc;
                if (a > 1.f / 8192) put(b, x, y, sr, sg, sb, a);
            }
        }
        free(s);
    }
    if (shapeToo) {
        for (int y = b->my0; y < b->my1; y++) {
            float cc = clipcov(b, y);
            if (cc <= 0) continue;
            for (int x = b->mx0; x < b->mx1; x++) {
                float m = b->M[(size_t)y * b->W + x];
                if (m <= 0) continue;
                paint_eval(p, x, y, o);
                put(b, x, y, o[0], o[1], o[2], m * o[3] * (float)ga * cc);
            }
        }
    }
    mask_reset(b);
}
static void draw(B *b, paint_t p, double ga) { composite(b, &p, ga, 0, 0, 1); }
static void draw_glow(B *b, paint_t p, double ga, col sh, double blurPx) { composite(b, &p, ga, &sh, blurPx, 1); }

/* path = list of segments */
typedef struct { double *s; int n, cap; } path_t;
static void path_seg(path_t *p, double ax, double ay, double bx, double by) {
    if (p->n == p->cap) { p->cap = p->cap ? p->cap * 2 : 64; p->s = (double *)realloc(p->s, sizeof(double) * 4 * (size_t)p->cap); }
    double *q = p->s + 4 * (size_t)p->n++; q[0] = ax; q[1] = ay; q[2] = bx; q[3] = by;
}
static void quadpt(double x0, double y0, double cx, double cy, double x1, double y1, double t, double *ox, double *oy) {
    double a = 1 - t; *ox = a * a * x0 + 2 * a * t * cx + t * t * x1; *oy = a * a * y0 + 2 * a * t * cy + t * t * y1;
}
static void path_quad(path_t *p, double x0, double y0, double cx, double cy, double x1, double y1) {
    double px = x0, py = y0;
    for (int i = 1; i <= 20; i++) { double qx, qy; quadpt(x0, y0, cx, cy, x1, y1, i / 20.0, &qx, &qy); path_seg(p, px, py, qx, qy); px = qx; py = qy; }
}
static void stroke(B *b, const path_t *p, double w) {
    for (int i = 0; i < p->n; i++) mask_seg(b, p->s[4 * i], p->s[4 * i + 1], p->s[4 * i + 2], p->s[4 * i + 3], w);
}

/* ======================================================================
 * Scene
 * ==================================================================== */
typedef struct { col skyTop, horizon, sunTop, sunBottom, grid, floor_, nebulaA, nebulaB, objects; } pal_t;

struct nh_layers {
    int W, H; nh_pixfmt fmt;
    uint32_t *baked;                  /* full frame at t = 0 */
    int rx0, ry0, rw, rh;             /* animated region (sun) */
    float *x0, *x1, *vig;             /* pre-tone rgb without / with full sun, vignette factor */
    int *c0, *c1;                     /* per row: columns covered by the sun disc */
    float *gap0;                      /* stripe gaps baked into the t = 0 frame */
    float lut[1025];                  /* tone curve */
    double sx, sy, R, hy, sunSpeed;
    float sun0[3], sun1[3], sun2[3];
    int sunOn; float contrast, bright;
    /* pppEmu: moving grid lines. Floor rows [fy0, H): baked has them without the horizontal
     * lines, floorG with the floor fully covered by the grid colour (palms etc. still in
     * front); each frame a row is a mix of both by the lines' coverage of that row. */
    int driveOn, fy0; uint32_t *floorG;
    double gD, gS, gU, gSigma, driveSpeed;
};

static float tone(float v, float c, float br) {
    if (c != 1) { v = (v - .5f) * c + .5f; v = v < 0 ? 0 : (v > 1 ? 1 : v); }
    if (br != 1) { v *= br; v = v > 1 ? 1 : v; }
    return v;
}
static uint32_t pack(float r, float g, float b, nh_pixfmt fmt) {
    int R = (int)(r * 255 + .5f), G = (int)(g * 255 + .5f), Bc = (int)(b * 255 + .5f);
    R = R < 0 ? 0 : (R > 255 ? 255 : R); G = G < 0 ? 0 : (G > 255 ? 255 : G); Bc = Bc < 0 ? 0 : (Bc > 255 ? 255 : Bc);
    return fmt == NH_ARGB8888 ? 0xFF000000u | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)Bc
                              : 0xFF000000u | ((uint32_t)Bc << 16) | ((uint32_t)G << 8) | (uint32_t)R;
}

/* per-row stripe gaps of the sun at time t */
static void sun_stripes(const nh_layers *L, double t, float *rowGap) {
    double fr = fmod(t * .18 * L->sunSpeed, 1.0);
    if (fr < 0) fr += 1;
    for (int j = 0; j < L->rh; j++) {
        int y = L->ry0 + j; double g = 0;
        for (int i = -1; i < 11; i++) {
            double p = (i + fr) / 10;
            if (p < 0) continue;
            double ys = L->sy + L->R * (-.05 + p * 1.05), gap = L->R * (.008 + .05 * p);
            g += overlap(y, y + 1, ys, ys + gap);
        }
        rowGap[j] = (float)(g > 1 ? 1 : g);
    }
}
/* sun color + coverage at pixel */
static float sun_px(const nh_layers *L, int x, int y, float gap, float *c) {
    double d = hypot(x + .5 - L->sx, y + .5 - L->sy);
    double cov = clamp01(L->R + .5 - d) * (1 - gap) * clamp01(L->hy - y);
    if (cov <= 0) return 0;
    double t = (y + .5 - (L->sy - L->R)) / (2 * L->R);
    if (t <= 0) { memcpy(c, L->sun0, sizeof(float) * 3); }
    else if (t < .55) { float f = (float)(t / .55); for (int k = 0; k < 3; k++) c[k] = L->sun0[k] + (L->sun1[k] - L->sun0[k]) * f; }
    else if (t < 1) { float f = (float)((t - .55) / .45); for (int k = 0; k < 3; k++) c[k] = L->sun1[k] + (L->sun2[k] - L->sun1[k]) * f; }
    else memcpy(c, L->sun2, sizeof(float) * 3);
    return (float)cov;
}

static void draw_stars(B *b, const nh_settings *S, const pal_t *C) {
    double W = b->W, hy = b->hy, u = b->u;
    rng_t r = rng_new((double)S->seed * 17 + 3);
    int count = (int)jround(S->starDensity * W * hy / (u * u) / 2200);
    for (int i = 0; i < count; i++) {
        double sx = rnd(&r) * W, sy = pow(rnd(&r), 1.25) * hy, sz = (.35 + pow(rnd(&r), 4) * 1.9) * u;
        double fade = 1 - smooth(.55, 1, sy / hy) * .85, a = (.35 + rnd(&r) * .65) * fade, tint = rnd(&r), fl = rnd(&r);
        col c = tint < .12 ? mixc(WHITE, C->nebulaA, .4) : tint < .2 ? mixc(WHITE, C->sunTop, .4) : WHITE;
        mask_circle(b, sx, sy, sz); draw(b, solid(c, a), 1);
        if (fl < .022) {
            double Lr = sz * 9;
            mask_seg(b, sx - Lr, sy, sx + Lr, sy, fmax(.6, u * .6));
            mask_seg(b, sx, sy - Lr, sx, sy + Lr, fmax(.6, u * .6));
            draw(b, solid(c, a * .6), 1);
            paint_t g = rgrad(sx, sy, 0, sz * 6); addstop(&g, 0, c, a * .5); addstop(&g, 1, c, 0);
            mask_rect(b, sx - sz * 6, sy - sz * 6, sx + sz * 6, sy + sz * 6); draw(b, g, 1);
        }
    }
}

static void draw_mountains(B *b, const nh_settings *S, const pal_t *C) {
    double W = b->W, H = b->H, hy = b->hy, u = b->u, cx = W / 2;
    struct { double mh; col c; double la; int k, M; } Lr[2] = {
        { .17, mixc(C->floor_, C->skyTop, .6), .32, 1, 8 },
        { .10, C->floor_, .55, 2, 6 } };
    enum { N = 130 };
    for (int li = 0; li < 2; li++) {
        rng_t r = rng_new((double)S->seed * 19 + Lr[li].k * 101);
        noise_t n1, n2, n3; noise_init(&n1, &r); noise_init(&n2, &r); noise_init(&n3, &r);
        double pts[N][2];
        for (int i = 0; i < N; i++) {
            double t = i / (double)(N - 1);
            double nv = .6 * noise_at(&n1, t * 4.2) + .3 * noise_at(&n2, t * 9.5) + .1 * noise_at(&n3, t * 23);
            double mask = .12 + .88 * smooth(.06, .4, fabs(t - .5));
            pts[i][0] = t * W; pts[i][1] = hy - H * Lr[li].mh * S->mountainH * (.3 + .7 * nv) * mask;
        }
        /* fill */
        double poly[(N + 2) * 2];
        poly[0] = 0; poly[1] = hy + 1;
        for (int i = 0; i < N; i++) { poly[2 + 2 * i] = pts[i][0]; poly[3 + 2 * i] = pts[i][1]; }
        poly[2 * N + 2] = W; poly[2 * N + 3] = hy + 1;
        mask_poly(b, poly, N + 2);
        paint_t g = vgrad(hy - H * Lr[li].mh * S->mountainH, hy);
        addstop(&g, 0, Lr[li].c, 1); addstop(&g, 1, mixc(Lr[li].c, C->horizon, .25), 1);
        draw(b, g, 1);
        /* mesh */
        int M = Lr[li].M; double la = Lr[li].la, lw = fmax(.6, u * .75);
#define LV(j) pow((j) / (double)M, .8)
#define ATX(i, f) (pts[i][0] + (pts[i][0] - cx) * .05 * (1 - (f)))
#define ATY(i, f) (hy - (hy - pts[i][1]) * (f))
        for (int j = 1; j < M; j++) {
            double f = LV(j);
            for (int i = 0; i + 1 < N; i++) mask_seg(b, ATX(i, f), ATY(i, f), ATX(i + 1, f), ATY(i + 1, f), lw);
            draw(b, solid(C->grid, la * (.2 + .65 * f)), 1);
        }
        for (int i = 0; i < N; i += 2)
            for (int j = M; j > 0; j--) mask_seg(b, ATX(i, LV(j)), ATY(i, LV(j)), ATX(i, LV(j - 1)), ATY(i, LV(j - 1)), lw);
        draw(b, solid(C->grid, la * .55), 1);
        for (int i = 0; i < N - 2; i += 2)
            for (int j = 0; j < M; j++) mask_seg(b, ATX(i, LV(j)), ATY(i, LV(j)), ATX(i + 2, LV(j + 1)), ATY(i + 2, LV(j + 1)), lw);
        draw(b, solid(C->grid, la * .3), 1);
#undef LV
#undef ATX
#undef ATY
        /* ridge */
        for (int i = 0; i + 1 < N; i++) mask_seg(b, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], fmax(1, u * 1.6));
        draw_glow(b, solid(C->grid, fmin(1, la + .35)), 1, C->grid, 12 * u * S->glow);
    }
}

static void draw_floor_base(B *b, const nh_settings *S, const pal_t *C, int hlines) {
    double W = b->W, H = b->H, hy = b->hy, u = b->u, cx = W / 2, D = H - hy;
    paint_t g = vgrad(hy, H);
    addstop(&g, 0, mixc(C->floor_, C->horizon, .35), 1); addstop(&g, .3, C->floor_, 1); addstop(&g, 1, C->floor_, 1);
    mask_rect(b, 0, hy, W, H + 1); draw(b, g, 1);
    if (!S->grid) return;
    double s = 4 / S->gridDensity, glowPx = 9 * u * S->glow;
    double syN = hy + D / .8;
    int jm = (int)ceil(W * 6 / (D * s));
    for (int j = -jm; j <= jm; j++) {
        double X = j * s, x0 = cx + X * D / .8;
        mask_seg(b, x0, syN, cx + (x0 - cx) * .002, hy + (syN - hy) * .002, fmax(1, 1.5 * u));
    }
    paint_t vg = vgrad(H, hy);
    addstop(&vg, 0, C->grid, 1); addstop(&vg, .7, C->grid, .55); addstop(&vg, 1, C->grid, .08);
    draw_glow(b, vg, 1, C->grid, glowPx);
    if (!hlines) return;    /* pppEmu: moving lines, drawn every frame (nh_draw_part) */
    for (int k = 0; k < 2000; k++) {
        double z = k * s + s * .5;
        if (z < .9) continue;
        if (z > 200) break;
        double y = hy + D / z, dy = D * s / (z * z), a = clamp01(dy / (5 * u));
        if (a < .02) break;
        double lw = fmax(.8, u * (.8 + 1.6 / sqrt(z)));
        mask_rect(b, 0, y - lw / 2, W, y + lw / 2);
        draw_glow(b, solid(C->grid, a), 1, C->grid, glowPx);
    }
}

static void draw_horizon(B *b, const nh_settings *S, const pal_t *C) {
    double W = b->W, H = b->H, hy = b->hy, u = b->u, D = H - hy;
    double top = hy - H * .07, bot = hy + D * .35, hs = (hy - top) / (bot - top);
    paint_t g = vgrad(top, bot);
    addstop(&g, 0, C->horizon, 0); addstop(&g, hs, C->horizon, .38); addstop(&g, 1, C->horizon, 0);
    mask_rect(b, 0, top, W, bot); draw(b, g, 1);
    mask_seg(b, 0, hy, W, hy, fmax(1, 2 * u));
    draw_glow(b, solid(mixc(C->horizon, WHITE, .4), 1), 1, C->horizon, 22 * u * S->glow);
}

static void palm(B *b, double x, double y, double h, double lean, rng_t *r, col fill, col rim, double blurPx, col wireC, double ga) {
    double tx = x + lean * h * .32, ty = y - h, qx = x + lean * h * .04, qy = y - h * .5;
    enum { TS = 28 };
    double Lp[TS + 1][2], Rp[TS + 1][2];
    for (int i = 0; i <= TS; i++) {
        double t = i / (double)TS, px, py, p2x, p2y, p1x, p1y;
        quadpt(x, y, qx, qy, tx, ty, t, &px, &py);
        quadpt(x, y, qx, qy, tx, ty, fmin(1, t + .01), &p2x, &p2y);
        quadpt(x, y, qx, qy, tx, ty, fmax(0, t - .01), &p1x, &p1y);
        double dx = p2x - p1x, dy = p2y - p1y, l = hypot(dx, dy); if (l == 0) l = 1;
        double nx = -dy / l, ny = dx / l, w = h * (.03 - .014 * t);
        Lp[i][0] = px + nx * w; Lp[i][1] = py + ny * w; Rp[i][0] = px - nx * w; Rp[i][1] = py - ny * w;
    }
    double trunk[(TS + 1) * 4];
    for (int i = 0; i <= TS; i++) { trunk[2 * i] = Lp[i][0]; trunk[2 * i + 1] = Lp[i][1]; }
    for (int i = 0; i <= TS; i++) { trunk[2 * (TS + 1) + 2 * i] = Rp[TS - i][0]; trunk[2 * (TS + 1) + 2 * i + 1] = Rp[TS - i][1]; }

    path_t wire = { 0 }, spine = { 0 }, leaf = { 0 }, tips = { 0 };
    for (int i = 0; i < TS; i++) { path_seg(&wire, Lp[i][0], Lp[i][1], Lp[i + 1][0], Lp[i + 1][1]); path_seg(&wire, Rp[i][0], Rp[i][1], Rp[i + 1][0], Rp[i + 1][1]); }
    for (int i = 1; i < TS; i++) { double mx, my; quadpt(x, y, qx, qy, tx, ty, i / (double)TS, &mx, &my); path_quad(&wire, Lp[i][0], Lp[i][1], mx, my + h * .009, Rp[i][0], Rp[i][1]); }
    for (int i = 1; i < TS; i += 2) path_seg(&wire, Lp[i][0], Lp[i][1], Rp[i + 1][0], Rp[i + 1][1]);

    int n = 7 + (int)floor(rnd(r) * 3);
    for (int i = 0; i < n; i++) {
        double th = (200 - 220.0 * i / (n - 1) + (rnd(r) - .5) * 14) * NH_PI / 180, cs = cos(th), sn = sin(th);
        double len = h * (.36 + rnd(r) * .16) * (1 - .3 * fmax(0, sn));
        double ex = tx + cs * len, ey = ty - sn * len * .7 + len * .45, cxp = tx + cs * len * .6, cyp = ty - sn * len * .6 - len * .15;
        path_quad(&spine, tx, ty, cxp, cyp, ex, ey);
        enum { MM = 22 };
        double side[2][MM][2]; int ns = 0;
        for (int j = 2; j <= MM; j++) {
            double t = j / (double)MM, px, py, p2x, p2y;
            quadpt(tx, ty, cxp, cyp, ex, ey, t, &px, &py);
            quadpt(tx, ty, cxp, cyp, ex, ey, fmin(1, t + .02), &p2x, &p2y);
            double dx = p2x - px, dy = p2y - py, l = hypot(dx, dy); if (l == 0) l = 1;
            dx /= l; dy /= l;
            double ll = h * .085 * sin(NH_PI * fmin(1, t * 1.1)) * (1 - .3 * t) + h * .012;
            for (int si = 0; si < 2; si++) {
                double sd = si ? 1 : -1;
                double e0 = px - dy * sd * ll - dx * ll * .35, e1 = py + dx * sd * ll - dy * ll * .35 + ll * .45;
                path_seg(&leaf, px, py, e0, e1);
                side[si][ns][0] = e0; side[si][ns][1] = e1;
            }
            ns++;
        }
        for (int si = 0; si < 2; si++) {
            for (int k = 0; k + 1 < ns; k++) path_seg(&tips, side[si][k][0], side[si][k][1], side[si][k + 1][0], side[si][k + 1][1]);
            path_seg(&tips, side[si][ns - 1][0], side[si][ns - 1][1], ex, ey);
        }
    }
    double nut[3][2], nr = h * .016;
    for (int k = 0; k < 3; k++) { nut[k][0] = tx + (k - 1) * h * .022; nut[k][1] = ty + h * .03; }

    paint_t pf = solid(fill, 1);
    mask_poly(b, trunk, 2 * (TS + 1)); draw_glow(b, pf, ga, rim, blurPx);
    stroke(b, &spine, h * .011); draw_glow(b, pf, ga, rim, blurPx);
    stroke(b, &leaf, h * .0065); draw_glow(b, pf, ga, rim, blurPx);
    for (int k = 0; k < 3; k++) mask_circle(b, nut[k][0], nut[k][1], nr);
    draw_glow(b, pf, ga, rim, blurPx);
    double lw = fmax(.5, h * .0032), wb = blurPx * .6;
    stroke(b, &wire, lw); draw_glow(b, solid(wireC, .85), ga, wireC, wb);
    for (int k = 0; k < 3; k++) mask_ring(b, nut[k][0], nut[k][1], nr, lw);
    draw_glow(b, solid(wireC, .85), ga, wireC, wb);
    stroke(b, &spine, lw * 1.2); draw_glow(b, solid(wireC, .95), ga, wireC, wb);
    stroke(b, &leaf, lw * .8);   draw_glow(b, solid(wireC, .5), ga, wireC, wb);
    stroke(b, &tips, lw * .7);   draw_glow(b, solid(wireC, .35), ga, wireC, wb);
    free(wire.s); free(spine.s); free(leaf.s); free(tips.s);
}

typedef struct { double X, z, lean, hgt; uint32_t ss; } palm_t;
static int cmp_palm(const void *a, const void *b) {
    double za = ((const palm_t *)a)->z, zb = ((const palm_t *)b)->z;
    return za > zb ? -1 : za < zb;
}
static void draw_palms(B *b, const nh_settings *S, const pal_t *C) {
    double W = b->W, H = b->H, hy = b->hy, u = b->u, cx = W / 2, D = H - hy;
    rng_t r = rng_new((double)S->seed * 7 + 5);
    int cnt = (int)S->palmCount;
    double R = .25 + .35, gz = 1.6, span = cnt * gz, edge = W / (2 * D);
    palm_t *list = (palm_t *)malloc(sizeof(palm_t) * 2 * (size_t)(cnt > 0 ? cnt : 1)); int n = 0;
    for (int k = 0; k < cnt; k++) for (int si = 0; si < 2; si++) {
        double side = si ? 1 : -1;
        double off = fmax(R + .3, S->palmSpread * edge * (.8 + rnd(&r) * .3));
        double lean = -side * (.05 + rnd(&r) * .4), hgt = 1.25 + rnd(&r) * .55;
        uint32_t ss = (uint32_t)floor(rnd(&r) * 1e9);
        double z = fmod(fmod(k * gz + (side > 0 ? gz / 2 : 0), span) + span, span) + .75;
        palm_t p = { side * off, z, lean, hgt, ss }; list[n++] = p;
    }
    qsort(list, (size_t)n, sizeof(palm_t), cmp_palm);
    col fill = mixc(C->floor_, hexc(0), .45);
    for (int i = 0; i < n; i++) {
        palm_t *p = &list[i];
        double a = smooth(.75, 1.05, p->z) * (1 - smooth(span * .7, span + .75, p->z));
        if (a < .01) continue;
        rng_t pr = rng_new((double)p->ss);
        palm(b, cx + p->X * D / p->z, hy + D / p->z, p->hgt * D / p->z, p->lean, &pr, fill, C->horizon, 12 * u * S->glow, C->grid, a);
    }
    free(list);
}

nh_layers *nh_build(const nh_settings *S, const nh_palette *P, nh_pixfmt fmt) {
    int W = S->w, H = S->h;
    nh_palette adj = adjust_palette(P, S->hue, S->sat);
    pal_t C = { hexc(adj.skyTop), hexc(adj.horizon), hexc(adj.sunTop), hexc(adj.sunBottom), hexc(adj.grid),
                hexc(adj.floor), hexc(adj.nebulaA), hexc(adj.nebulaB), hexc(adj.objects) };
    B b; memset(&b, 0, sizeof b);
    b.W = W; b.H = H; b.u = (W < H ? W : H) / 1000.0; b.hy = H * S->horizon;
    b.M = (float *)calloc((size_t)W * H, sizeof(float));
    b.mx0 = W; b.my0 = H;
    float *canvas = (float *)calloc((size_t)W * H * 3, sizeof(float));
    nh_layers *L = (nh_layers *)calloc(1, sizeof(nh_layers));
    if (!b.M || !canvas || !L) { free(b.M); free(canvas); free(L); return 0; }
    L->W = W; L->H = H; L->fmt = fmt; L->hy = b.hy; L->sunOn = S->sun;
    L->contrast = S->contrast; L->bright = S->bright; L->sunSpeed = S->sunSpeed;
    L->R = (W < H ? W : H) * S->sunSize; L->sx = W / 2.0; L->sy = b.hy - L->R * .42;
    col s1 = mixc(C.sunTop, C.sunBottom, .6);
    L->sun0[0] = C.sunTop.r / 255.f; L->sun0[1] = C.sunTop.g / 255.f; L->sun0[2] = C.sunTop.b / 255.f;
    L->sun1[0] = s1.r / 255.f;       L->sun1[1] = s1.g / 255.f;       L->sun1[2] = s1.b / 255.f;
    L->sun2[0] = C.sunBottom.r / 255.f; L->sun2[1] = C.sunBottom.g / 255.f; L->sun2[2] = C.sunBottom.b / 255.f;

    /* animated region = sun box clipped to the horizon */
    if (S->sun) {
        int x0 = (int)floor(L->sx - L->R - 2), x1 = (int)ceil(L->sx + L->R + 2);
        int y0 = (int)floor(L->sy - L->R - 2), y1 = (int)ceil(b.hy) + 1;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > W) x1 = W;
        if (y1 > H) y1 = H;
        L->rx0 = x0; L->ry0 = y0; L->rw = x1 - x0; L->rh = y1 - y0;
        if (L->rw < 0) L->rw = 0;
        if (L->rh < 0) L->rh = 0;
    }
    size_t rn = (size_t)L->rw * L->rh;

    b.tg[0].px = canvas; b.tg[0].w = W; b.tg[0].h = H; b.tg[0].ch = 3; b.ntg = 1;

    /* ---- behind the sun ---- */
    paint_t sky = vgrad(0, b.hy);
    addstop(&sky, 0, C.skyTop, 1); addstop(&sky, .5, mixc(C.skyTop, C.horizon, .2), 1);
    addstop(&sky, .86, mixc(C.skyTop, C.horizon, .6), 1); addstop(&sky, 1, C.horizon, 1);
    mask_rect(&b, 0, 0, W, b.hy + 2); draw(&b, sky, 1);
    if (S->stars && S->starDensity > 0) draw_stars(&b, S, &C);
    if (S->sun) {
        double R = L->R;
        b.clipY = (float)b.hy;
        paint_t halo = rgrad(L->sx, L->sy, R * .6, R * 2.4);
        addstop(&halo, 0, C.sunBottom, .35 * clampd(S->glow, 0, 1.5)); addstop(&halo, 1, C.sunBottom, 0);
        mask_rect(&b, L->sx - R * 2.4, L->sy - R * 2.4, L->sx + R * 2.4, L->sy + R * 2.4); draw(&b, halo, 1);
        /* glow of the striped disc, approximated once (stripes remove ~12% of the disc) */
        mask_circle(&b, L->sx, L->sy, R);
        paint_t sp = solid(C.sunBottom, .88);
        composite(&b, &sp, 1, &C.sunBottom, 55 * b.u * S->glow, 0);
        b.clipY = 0;
    }
    float *back = 0, *front = 0, *sunc = 0;   /* region scratch: sky rgb, foreground rgba, sun rgb + coverage */
    if (rn) {
        back = (float *)malloc(sizeof(float) * 3 * rn);
        sunc = (float *)calloc(4 * rn, sizeof(float));
        for (int j = 0; j < L->rh; j++)
            memcpy(back + (size_t)j * L->rw * 3, canvas + ((size_t)(L->ry0 + j) * W + L->rx0) * 3, sizeof(float) * 3 * (size_t)L->rw);
        /* sun at t = 0 */
        L->gap0 = (float *)malloc(sizeof(float) * (size_t)L->rh);
        sun_stripes(L, 0, L->gap0);
        for (int j = 0; j < L->rh; j++) for (int i = 0; i < L->rw; i++) {
            int x = L->rx0 + i, y = L->ry0 + j;
            float *sc = sunc + 4 * ((size_t)j * L->rw + i);
            sc[3] = sun_px(L, x, y, 0, sc);
            float a = sc[3] * (1 - L->gap0[j]);
            if (a > 0) put(&b, x, y, sc[0], sc[1], sc[2], a);
        }
        front = (float *)calloc(4 * rn, sizeof(float));
        b.tg[1].px = front; b.tg[1].x0 = L->rx0; b.tg[1].y0 = L->ry0; b.tg[1].w = L->rw; b.tg[1].h = L->rh; b.tg[1].ch = 4;
        b.ntg = 2;
    }
    /* ---- in front of the sun ---- */
    if (S->mountains) draw_mountains(&b, S, &C);
    L->driveOn = S->grid && S->driveSpeed > 0;
    draw_floor_base(&b, S, &C, !L->driveOn);
    draw_horizon(&b, S, &C);
    if (S->palms && S->palmCount >= 1) draw_palms(&b, S, &C);
    float *floorFull = 0;   /* pppEmu: floor rows with the grid colour everywhere, pre-tone */
    if (L->driveOn) {
        L->fy0 = (int)floor(b.hy); if (L->fy0 < 0) L->fy0 = 0; if (L->fy0 > H) L->fy0 = H;
        L->gD = H - b.hy; L->gS = 4 / S->gridDensity; L->gU = b.u; L->gSigma = 9 * b.u * S->glow / 2;
        L->driveSpeed = S->driveSpeed;
        size_t fn = (size_t)W * (H - L->fy0) * 3;
        float *saved = (float *)malloc(sizeof(float) * (size_t)W * H * 3);
        floorFull = (float *)malloc(sizeof(float) * (fn ? fn : 1));
        if (!saved || !floorFull) { free(saved); free(floorFull); floorFull = 0; L->driveOn = 0; }
        else {
            /* second pass: floor = grid colour, then what's in front of it again */
            memcpy(saved, canvas, sizeof(float) * (size_t)W * H * 3);
            float gr = C.grid.r / 255.f, gg = C.grid.g / 255.f, gb = C.grid.b / 255.f;
            for (int y = L->fy0; y < H; y++) for (int x = 0; x < W; x++) {
                float *p = canvas + ((size_t)y * W + x) * 3; p[0] = gr; p[1] = gg; p[2] = gb;
            }
            int ntg = b.ntg; b.ntg = 1;     /* the sun's front layer is already done */
            draw_horizon(&b, S, &C);
            if (S->palms && S->palmCount >= 1) draw_palms(&b, S, &C);
            b.ntg = ntg;
            memcpy(floorFull, canvas + (size_t)L->fy0 * W * 3, sizeof(float) * fn);
            memcpy(canvas, saved, sizeof(float) * (size_t)W * H * 3);
            free(saved);
        }
    }

    /* ---- tone + vignette + pack ---- */
    L->baked = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)W * H);
    if (rn) L->vig = (float *)malloc(sizeof(float) * rn);
    double vr0 = (W < H ? W : H) * .35, vr1 = (W > H ? W : H) * .75;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float *p = canvas + ((size_t)y * W + x) * 3, v = 1;
        if (S->vignette) v = 1 - .55f * (float)clamp01((hypot(x + .5 - W / 2.0, y + .5 - H / 2.0) - vr0) / (vr1 - vr0));
        if (rn && x >= L->rx0 && x < L->rx0 + L->rw && y >= L->ry0 && y < L->ry0 + L->rh)
            L->vig[(size_t)(y - L->ry0) * L->rw + (x - L->rx0)] = v;
        L->baked[(size_t)y * W + x] = pack(tone(p[0], L->contrast, L->bright) * v, tone(p[1], L->contrast, L->bright) * v,
                                           tone(p[2], L->contrast, L->bright) * v, fmt);
        if (floorFull && y >= L->fy0) {
            if (!L->floorG) L->floorG = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)W * (H - L->fy0));
            if (L->floorG) {
                const float *q = floorFull + ((size_t)(y - L->fy0) * W + x) * 3;
                L->floorG[(size_t)(y - L->fy0) * W + x] = pack(tone(q[0], L->contrast, L->bright) * v,
                                                               tone(q[1], L->contrast, L->bright) * v,
                                                               tone(q[2], L->contrast, L->bright) * v, fmt);
            }
        }
    }
    free(floorFull);
    if (L->driveOn && !L->floorG) L->driveOn = 0;
    /* The animated pixel is linear in the stripe gap g:
     *   x(g) = front + (1-fa) * (back + disc*(1-g)*(sun-back))  =  x1 - g*(x1-x0)
     * so store x0 (no sun) and x1 (full sun) and the disc's column span per row. */
    if (rn) {
        L->x0 = (float *)malloc(sizeof(float) * 3 * rn);
        L->x1 = (float *)malloc(sizeof(float) * 3 * rn);
        L->c0 = (int *)malloc(sizeof(int) * (size_t)L->rh);
        L->c1 = (int *)malloc(sizeof(int) * (size_t)L->rh);
        for (int j = 0; j < L->rh; j++) {
            L->c0[j] = L->rw; L->c1[j] = 0;
            for (int i = 0; i < L->rw; i++) {
                size_t q = (size_t)j * L->rw + i;
                const float *bk = back + 3 * q, *fr = front + 4 * q, *sc = sunc + 4 * q;
                float k = 1 - fr[3];
                for (int ch = 0; ch < 3; ch++) {
                    L->x0[3 * q + ch] = fr[ch] + k * bk[ch];
                    L->x1[3 * q + ch] = fr[ch] + k * (bk[ch] + sc[3] * (sc[ch] - bk[ch]));
                }
                if (sc[3] > 0) { if (i < L->c0[j]) L->c0[j] = i; if (i + 1 > L->c1[j]) L->c1[j] = i + 1; }
            }
        }
    }
    for (int i = 0; i <= 1024; i++) L->lut[i] = tone(i / 1024.f, L->contrast, L->bright);
    free(back); free(front); free(sunc);
    free(canvas); free(b.M);
    return L;
}

static float lut_tone(const nh_layers *L, float v) {
    if (v <= 0) return L->lut[0];
    if (v >= 1) return L->lut[1024];
    float f = v * 1024; int i = (int)f; f -= i;
    return L->lut[i] + (L->lut[i + 1] - L->lut[i]) * f;
}

/* pppEmu: moving grid lines. Coverage of each floor row by the horizontal lines at time t
 * (same lines as the still grid: solid line + its blurred glow, one over the other), then
 * each row = baked floor mixed with the grid coloured floor by that coverage. */
static double nh_erf(double x) {
    /* Abramowitz-Stegun 7.1.26, plenty for a glow */
    double s = x < 0 ? -1 : 1; x = fabs(x);
    double t = 1 / (1 + .3275911 * x);
    double y = 1 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - .284496736) * t + .254829592) * t * exp(-x * x);
    return s * y;
}
static void draw_floor_lines(const nh_layers *L, double t, uint32_t *dst, int pitch) {
    float A[4096];          /* per floor row: part not covered by any line */
    const int W = L->W, H = L->H, rows = H - L->fy0 < 4096 ? H - L->fy0 : 4096;
    for (int j = 0; j < rows; j++) A[j] = 1;
    const double D = L->gD, s = L->gS, u = L->gU, sg = L->gSigma, hy = L->hy;
    const double phase = fmod(t * L->driveSpeed * NH_DRIVE_RATE, s);
    const double k2 = 1 / (sg * sqrt(2.0));
    for (int k = 0; k < 2000; k++) {
        double z = k * s + s * .5 - phase;
        if (z < .9) continue;
        if (z > 200) break;
        double y = hy + D / z, dy = D * s / (z * z), a = clamp01(dy / (5 * u));
        if (a < .02) break;
        double lw = fmax(.8, u * (.8 + 1.6 / sqrt(z))), top = y - lw / 2, bot = y + lw / 2;
        int r0 = (int)floor(top - 3 * sg) - L->fy0, r1 = (int)ceil(bot + 3 * sg) - L->fy0;
        if (r0 < 0) r0 = 0;
        if (r1 > rows) r1 = rows;
        for (int j = r0; j < r1; j++) {
            double py = L->fy0 + j, c = py + .5;
            double glow = .5 * (nh_erf((bot - c) * k2) - nh_erf((top - c) * k2)) * a;
            double solid = overlap(py, py + 1, top, bot) * a;
            A[j] *= (float)((1 - glow) * (1 - solid));
        }
    }
    for (int j = 0; j < rows; j++) {
        const unsigned kk = (unsigned)((1 - A[j]) * 256 + .5f), ik = 256 - kk;
        const uint32_t *q0 = L->baked + (size_t)(L->fy0 + j) * W, *q1 = L->floorG + (size_t)j * W;
        uint32_t *o = dst + (size_t)(L->fy0 + j) * pitch;
        if (kk == 0) { memcpy(o, q0, sizeof(uint32_t) * (size_t)W); continue; }
        for (int x = 0; x < W; x++) {
            uint32_t a = q0[x], c = q1[x];
            uint32_t rb = (((a & 0xFF00FF) * ik + (c & 0xFF00FF) * kk) >> 8) & 0xFF00FF;
            uint32_t g = (((a & 0x00FF00) * ik + (c & 0x00FF00) * kk) >> 8) & 0x00FF00;
            o[x] = 0xFF000000u | rb | g;
        }
    }
}

/* pppEmu: "full" 0 = dst already holds this scene from the last frame: only the sun area is
 * redrawn (much less to copy, and to upload to the GPU). */
static void nh_draw_part(const nh_layers *L, double t, uint32_t *dst, int pitch, int full) {
    if (full) {
        if (pitch == L->W) memcpy(dst, L->baked, sizeof(uint32_t) * (size_t)L->W * L->H);
        else for (int y = 0; y < L->H; y++) memcpy(dst + (size_t)y * pitch, L->baked + (size_t)y * L->W, sizeof(uint32_t) * (size_t)L->W);
    } else if (L->sunOn && L->rw && L->rh) {
        for (int j = 0; j < L->rh; j++) {
            const size_t y = (size_t)(L->ry0 + j);
            memcpy(dst + y * pitch + L->rx0, L->baked + y * L->W + L->rx0, sizeof(uint32_t) * (size_t)L->rw);
        }
    }
    if (L->sunOn && L->rw && L->rh) {
        float gap[4096];
        int rh = L->rh < 4096 ? L->rh : 4096;
        sun_stripes(L, t, gap);
        for (int j = 0; j < rh; j++) {
            if (gap[j] == L->gap0[j]) continue;          /* row looks the same as the baked frame */
            float g = gap[j];
            size_t base = (size_t)j * L->rw;
            uint32_t *o = dst + (size_t)(L->ry0 + j) * pitch + L->rx0;
            for (int i = L->c0[j]; i < L->c1[j]; i++) {
                const float *a = L->x0 + 3 * (base + i), *b = L->x1 + 3 * (base + i);
                float v = L->vig[base + i];
                o[i] = pack(lut_tone(L, b[0] - g * (b[0] - a[0])) * v,
                            lut_tone(L, b[1] - g * (b[1] - a[1])) * v,
                            lut_tone(L, b[2] - g * (b[2] - a[2])) * v, L->fmt);
            }
        }
    }
    if (L->driveOn) draw_floor_lines(L, t, dst, pitch);
}

void nh_draw(const nh_layers *L, double t, uint32_t *dst, int pitch) {
    nh_draw_part(L, t, dst, pitch, 1);
}

void nh_free(nh_layers *L) {
    if (!L) return;
    free(L->floorG);
    free(L->baked); free(L->x0); free(L->x1); free(L->vig); free(L->c0); free(L->c1); free(L->gap0); free(L);
}

/* ======================================================================
 * Manager: cache, worker thread, crossfade
 * ==================================================================== */
typedef struct { char key[32]; nh_layers *L; unsigned used; } slot_t;

struct nh_bg {
    nh_settings s; nh_pixfmt fmt; double t, fade;
    slot_t cache[NH_CACHE]; unsigned tick;
    nh_layers *cur, *prev; char curKey[32], wantKey[32];
    const nh_layers *lastDrawn;   /* pppEmu: scene fully in the caller's buffer since last frame */
#if NH_THREADS
    pthread_t th; pthread_mutex_t mu; pthread_cond_t cv; int quit;
    int jobPending; char jobKey[32]; nh_palette jobPal;
    nh_layers *doneL; char doneKey[32];
    /* pppEmu: palettes to build and save to the disk cache while idle (not kept in memory) */
    char prebuild[24][32]; int prebuildN, prebuildI, idleAllowed;
#endif
    char cacheDir[256];           /* pppEmu: built scenes saved here ("" = no disk cache) */
};

/* ======================================================================
 * pppEmu: disk cache. A built scene is saved once ("<dir>/<key>_<hash>.nhb") and loaded
 * the next times (a fraction of a second instead of 1-3 s on the ps4). The hash covers
 * the settings, the palette, the pixel format and this file's layout, so any change
 * builds it again.
 * ==================================================================== */
#define NH_FILE_VERSION 2u

static uint32_t fnv(uint32_t h, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}
static void cache_path(const nh_bg *bg, const char *key, const nh_palette *p, char *out, size_t n) {
    uint32_t v = NH_FILE_VERSION, sz = (uint32_t)sizeof(nh_layers), f = (uint32_t)bg->fmt;
    uint32_t h = 2166136261u;
    h = fnv(h, &bg->s, sizeof bg->s); h = fnv(h, p, sizeof *p);
    h = fnv(h, &f, sizeof f); h = fnv(h, &v, sizeof v); h = fnv(h, &sz, sizeof sz);
    snprintf(out, n, "%s/%s_%08x.nhb", bg->cacheDir, key, (unsigned)h);
}
static int wr(FILE *f, const void *p, size_t n) { return !n || fwrite(p, 1, n, f) == n; }
static int rd(FILE *f, void *p, size_t n) { return !n || fread(p, 1, n, f) == n; }
static void layer_sizes(const nh_layers *L, size_t *rn, size_t *fn) {
    *rn = (size_t)L->rw * L->rh;
    *fn = L->driveOn ? (size_t)L->W * (L->H - L->fy0) : 0;
}
static void cache_save(const nh_bg *bg, const char *key, const nh_palette *p, const nh_layers *L) {
    if (!bg->cacheDir[0] || !L) return;
    char path[320], tmp[330];
    cache_path(bg, key, p, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    size_t rn, fn; layer_sizes(L, &rn, &fn);
    int ok = wr(f, "NHB2", 4) && wr(f, L, sizeof *L)
             && wr(f, L->baked, sizeof(uint32_t) * (size_t)L->W * L->H);
    if (ok && rn) ok = wr(f, L->x0, sizeof(float) * 3 * rn) && wr(f, L->x1, sizeof(float) * 3 * rn)
                       && wr(f, L->vig, sizeof(float) * rn) && wr(f, L->c0, sizeof(int) * (size_t)L->rh)
                       && wr(f, L->c1, sizeof(int) * (size_t)L->rh) && wr(f, L->gap0, sizeof(float) * (size_t)L->rh);
    if (ok && fn) ok = wr(f, L->floorG, sizeof(uint32_t) * fn);
    if (fclose(f) != 0) ok = 0;
    if (ok) { remove(path); ok = rename(tmp, path) == 0; }
    if (!ok) remove(tmp);
}
static nh_layers *cache_load(const nh_bg *bg, const char *key, const nh_palette *p) {
    if (!bg->cacheDir[0]) return 0;
    char path[320], magic[4];
    cache_path(bg, key, p, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    nh_layers *L = (nh_layers *)calloc(1, sizeof(nh_layers));
    int ok = L && rd(f, magic, 4) && !memcmp(magic, "NHB2", 4) && rd(f, L, sizeof *L)
             && L->W == bg->s.w && L->H == bg->s.h && L->fmt == bg->fmt;
    if (ok) {
        size_t rn, fn; layer_sizes(L, &rn, &fn);
        L->baked = 0; L->x0 = L->x1 = L->vig = L->gap0 = 0; L->c0 = L->c1 = 0; L->floorG = 0;
        L->baked = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)L->W * L->H);
        ok = L->baked && rd(f, L->baked, sizeof(uint32_t) * (size_t)L->W * L->H);
        if (ok && rn) {
            L->x0 = (float *)malloc(sizeof(float) * 3 * rn); L->x1 = (float *)malloc(sizeof(float) * 3 * rn);
            L->vig = (float *)malloc(sizeof(float) * rn);
            L->c0 = (int *)malloc(sizeof(int) * (size_t)L->rh); L->c1 = (int *)malloc(sizeof(int) * (size_t)L->rh);
            L->gap0 = (float *)malloc(sizeof(float) * (size_t)L->rh);
            ok = L->x0 && L->x1 && L->vig && L->c0 && L->c1 && L->gap0
                 && rd(f, L->x0, sizeof(float) * 3 * rn) && rd(f, L->x1, sizeof(float) * 3 * rn)
                 && rd(f, L->vig, sizeof(float) * rn) && rd(f, L->c0, sizeof(int) * (size_t)L->rh)
                 && rd(f, L->c1, sizeof(int) * (size_t)L->rh) && rd(f, L->gap0, sizeof(float) * (size_t)L->rh);
        }
        if (ok && fn) {
            L->floorG = (uint32_t *)malloc(sizeof(uint32_t) * fn);
            ok = L->floorG && rd(f, L->floorG, sizeof(uint32_t) * fn);
        }
    } else if (L) {
        L->baked = 0; L->x0 = L->x1 = L->vig = L->gap0 = 0; L->c0 = L->c1 = 0; L->floorG = 0;
    }
    fclose(f);
    if (!ok) { nh_free(L); remove(path); return 0; }
    return L;
}
static int cache_exists(const nh_bg *bg, const char *key, const nh_palette *p) {
    if (!bg->cacheDir[0]) return 1;
    char path[320];
    cache_path(bg, key, p, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != 0;
}
/* loaded from the disk cache, or built (and saved) */
static nh_layers *load_or_build(nh_bg *bg, const char *key, const nh_palette *p) {
    nh_layers *L = cache_load(bg, key, p);
    if (L) return L;
    L = nh_build(&bg->s, p, bg->fmt);
    if (L) cache_save(bg, key, p, L);
    return L;
}

static nh_layers *cache_get(nh_bg *bg, const char *key) {
    for (int i = 0; i < NH_CACHE; i++) if (bg->cache[i].L && !strcmp(bg->cache[i].key, key)) { bg->cache[i].used = ++bg->tick; return bg->cache[i].L; }
    return 0;
}
static void cache_put(nh_bg *bg, const char *key, nh_layers *L) {
    int best = -1;
    for (int i = 0; i < NH_CACHE; i++) if (!bg->cache[i].L) { best = i; break; }
    if (best < 0) for (int i = 0; i < NH_CACHE; i++) {
        if (bg->cache[i].L == bg->cur || bg->cache[i].L == bg->prev) continue;
        if (best < 0 || bg->cache[i].used < bg->cache[best].used) best = i;
    }
    if (best < 0) { nh_free(L); return; }
    nh_free(bg->cache[best].L);
    strncpy(bg->cache[best].key, key, 31); bg->cache[best].key[31] = 0;
    bg->cache[best].L = L; bg->cache[best].used = ++bg->tick;
}
static void show(nh_bg *bg, const char *key, nh_layers *L) {
    if (L == bg->cur) return;
    bg->prev = bg->cur; bg->cur = L; bg->fade = bg->prev ? 0 : 1;
    strncpy(bg->curKey, key, 31); bg->curKey[31] = 0;
}

#if NH_THREADS
static void *worker(void *arg) {
    nh_bg *bg = (nh_bg *)arg;
    pthread_mutex_lock(&bg->mu);
    for (;;) {
        while (!bg->quit && !bg->jobPending
               && !(bg->idleAllowed && bg->prebuildI < bg->prebuildN))
            pthread_cond_wait(&bg->cv, &bg->mu);
        if (bg->quit) break;
        if (!bg->jobPending) {
            /* pppEmu: idle: one more palette into the disk cache (built, saved, freed) */
            char key[32]; memcpy(key, bg->prebuild[bg->prebuildI++], 32);
            pthread_mutex_unlock(&bg->mu);
            nh_palette pal = nh_system_palette(key);
            if (!cache_exists(bg, key, &pal)) {
                nh_layers *L = nh_build(&bg->s, &pal, bg->fmt);
                if (L) { cache_save(bg, key, &pal, L); nh_free(L); }
            }
            pthread_mutex_lock(&bg->mu);
            continue;
        }
        char key[32]; nh_palette pal = bg->jobPal; memcpy(key, bg->jobKey, 32); bg->jobPending = 0;
        pthread_mutex_unlock(&bg->mu);
        nh_layers *L = load_or_build(bg, key, &pal);
        pthread_mutex_lock(&bg->mu);
        if (bg->doneL) nh_free(bg->doneL);
        bg->doneL = L; memcpy(bg->doneKey, key, 32);
    }
    pthread_mutex_unlock(&bg->mu);
    return 0;
}
#endif

nh_bg *nh_bg_create(const nh_settings *s, nh_pixfmt fmt) {
    nh_bg *bg = (nh_bg *)calloc(1, sizeof(nh_bg));
    if (!bg) return 0;
    bg->s = s ? *s : DEFAULT_SETTINGS; bg->fmt = fmt; bg->fade = 1;
    nh_palette p = nh_system_palette("default");
    nh_layers *L = nh_build(&bg->s, &p, fmt);
    if (!L) { free(bg); return 0; }
    cache_put(bg, "default", L); show(bg, "default", L);
    strcpy(bg->wantKey, "default");
#if NH_THREADS
    pthread_mutex_init(&bg->mu, 0); pthread_cond_init(&bg->cv, 0);
    pthread_create(&bg->th, 0, worker, bg);
#endif
    return bg;
}

/* pppEmu: same as nh_bg_create, but the first palette is built on the worker thread so
 * startup doesn't wait for it: nh_bg_render draws nothing (returns 0) until it's ready. */
nh_bg *nh_bg_create_async(const nh_settings *s, nh_pixfmt fmt, const char *firstId, const char *cacheDir) {
#if NH_THREADS
    nh_bg *bg = (nh_bg *)calloc(1, sizeof(nh_bg));
    if (!bg) return 0;
    bg->s = s ? *s : DEFAULT_SETTINGS; bg->fmt = fmt; bg->fade = 1; bg->idleAllowed = 1;
    if (cacheDir) { strncpy(bg->cacheDir, cacheDir, sizeof bg->cacheDir - 1); }
    strcpy(bg->wantKey, "default");
    pthread_mutex_init(&bg->mu, 0); pthread_cond_init(&bg->cv, 0);
    if (pthread_create(&bg->th, 0, worker, bg) != 0) {
        pthread_mutex_destroy(&bg->mu); pthread_cond_destroy(&bg->cv); free(bg);
        return nh_bg_create(s, fmt);
    }
    nh_bg_set_system(bg, firstId ? firstId : "default");
    return bg;
#else
    nh_bg *bg = nh_bg_create(s, fmt);
    if (bg && cacheDir) strncpy(bg->cacheDir, cacheDir, sizeof bg->cacheDir - 1);
    if (bg && firstId) nh_bg_set_system(bg, firstId);
    return bg;
#endif
}

void nh_bg_prebuild(nh_bg *bg, const char *const *systemIds, int count) {
#if NH_THREADS
    pthread_mutex_lock(&bg->mu);
    bg->prebuildN = bg->prebuildI = 0;
    for (int i = 0; i < count && bg->prebuildN < 24; i++) {
        strncpy(bg->prebuild[bg->prebuildN], systemIds[i], 31); bg->prebuild[bg->prebuildN++][31] = 0;
    }
    pthread_cond_signal(&bg->cv);
    pthread_mutex_unlock(&bg->mu);
#else
    (void)bg; (void)systemIds; (void)count;
#endif
}

void nh_bg_set_idle_work(nh_bg *bg, int allowed) {
#if NH_THREADS
    if (bg->idleAllowed == allowed) return;
    pthread_mutex_lock(&bg->mu);
    bg->idleAllowed = allowed;
    pthread_cond_signal(&bg->cv);
    pthread_mutex_unlock(&bg->mu);
#else
    (void)bg; (void)allowed;
#endif
}

void nh_bg_set_palette(nh_bg *bg, const char *key, const nh_palette *p) {
    strncpy(bg->wantKey, key, 31); bg->wantKey[31] = 0;
    nh_layers *L = cache_get(bg, key);
    if (L) { show(bg, key, L); return; }
#if NH_THREADS
    pthread_mutex_lock(&bg->mu);
    bg->jobPal = *p; strncpy(bg->jobKey, key, 31); bg->jobKey[31] = 0; bg->jobPending = 1;
    pthread_cond_signal(&bg->cv);
    pthread_mutex_unlock(&bg->mu);
#else
    L = load_or_build(bg, key, p);
    if (L) { cache_put(bg, key, L); show(bg, key, L); }
#endif
}

void nh_bg_set_system(nh_bg *bg, const char *id) {
    nh_palette p = nh_system_palette(id);
    nh_bg_set_palette(bg, id ? id : "default", &p);
}

void nh_bg_prewarm(nh_bg *bg, const char *const *ids, int count) {
    for (int i = 0; i < count && i < NH_CACHE - 1; i++) {
        if (cache_get(bg, ids[i])) continue;
        nh_palette p = nh_system_palette(ids[i]);
        nh_layers *L = nh_build(&bg->s, &p, bg->fmt);
        if (L) cache_put(bg, ids[i], L);
    }
}

int nh_bg_render_rows(nh_bg *bg, double dt, uint32_t *dst, int pitch, int *y0, int *y1) {
    if (y0) *y0 = 0;
    if (y1) *y1 = 0;
#if NH_THREADS
    pthread_mutex_lock(&bg->mu);
    nh_layers *L = bg->doneL; char key[32];
    if (L) { memcpy(key, bg->doneKey, 32); bg->doneL = 0; }
    pthread_mutex_unlock(&bg->mu);
    if (L) {
        cache_put(bg, key, L);
        /* pppEmu: nothing shown yet (async start): the first one built is shown */
        if (!strcmp(key, bg->wantKey) || !bg->cur) show(bg, key, L);
    }
#endif
    if (!bg->cur) return 0;
    bg->t += dt;
    const int fading = bg->prev && bg->fade < 1;
    const int full = fading || bg->lastDrawn != bg->cur;
    nh_draw_part(bg->cur, bg->t, dst, pitch, full);
    bg->lastDrawn = bg->cur;
    if (full) {
        if (y1) *y1 = bg->cur->H;
    } else {
        const nh_layers *C = bg->cur;
        int a = C->H, z = 0;
        if (C->sunOn && C->rh) { a = C->ry0; z = C->ry0 + C->rh; }
        if (C->driveOn) { if (C->fy0 < a) a = C->fy0; z = C->H; }
        if (z > a) { if (y0) *y0 = a; if (y1) *y1 = z; }
    }
    if (fading) {
        bg->fade += dt / NH_FADE;
        if (bg->fade >= 1) { bg->fade = 1; bg->prev = 0; return 1; }
        float f = (float)bg->fade; f = f * f * (3 - 2 * f);
        unsigned k = (unsigned)(f * 256), ik = 256 - k;
        const nh_layers *P = bg->prev;
        for (int y = 0; y < P->H; y++) {
            uint32_t *o = dst + (size_t)y * pitch; const uint32_t *q = P->baked + (size_t)y * P->W;
            for (int x = 0; x < P->W; x++) {
                uint32_t a = q[x], c = o[x];
                uint32_t rb = (((a & 0xFF00FF) * ik + (c & 0xFF00FF) * k) >> 8) & 0xFF00FF;
                uint32_t g = (((a & 0x00FF00) * ik + (c & 0x00FF00) * k) >> 8) & 0x00FF00;
                o[x] = 0xFF000000u | rb | g;
            }
        }
        bg->lastDrawn = 0;    /* blended: the next frame is drawn in full */
    }
    return 1;
}

int nh_bg_render(nh_bg *bg, double dt, uint32_t *dst, int pitch) {
    return nh_bg_render_rows(bg, dt, dst, pitch, 0, 0);
}

void nh_bg_destroy(nh_bg *bg) {
    if (!bg) return;
#if NH_THREADS
    pthread_mutex_lock(&bg->mu); bg->quit = 1; pthread_cond_signal(&bg->cv); pthread_mutex_unlock(&bg->mu);
    pthread_join(bg->th, 0);
    if (bg->doneL) nh_free(bg->doneL);
    pthread_mutex_destroy(&bg->mu); pthread_cond_destroy(&bg->cv);
#endif
    for (int i = 0; i < NH_CACHE; i++) nh_free(bg->cache[i].L);
    free(bg);
}
