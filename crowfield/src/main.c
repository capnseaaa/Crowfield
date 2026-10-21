// Crowfield (GBA), v0.2: crops.
// One-screen farm. Crops grow on a timer. The farmer plants seeds and harvests ripe crops.
// One test crow circles overhead (it does nothing yet).
// D-pad: move   B: run   A: plant on empty soil / harvest a ripe crop
// Later: days, crows that eat crops, sickle/sword, scarecrow turrets, shop.

#include "data.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed int     s32;

// ---------- hardware ----------
#define REG16(a) (*(volatile u16*)(a))
#define REG_DISPCNT   REG16(0x04000000)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG0CNT    REG16(0x04000008)
#define REG_BG1CNT    REG16(0x0400000A)
#define REG_KEYINPUT  REG16(0x04000130)
#define REG_SND1SWEEP REG16(0x04000060)
#define REG_SND1CNT   REG16(0x04000062)
#define REG_SND1FREQ  REG16(0x04000064)
#define REG_SNDCNT_L  REG16(0x04000080)
#define REG_SNDCNT_H  REG16(0x04000082)
#define REG_SNDCNT_X  REG16(0x04000084)

#define PAL_BG   ((volatile u16*)0x05000000)
#define PAL_OBJ  ((volatile u16*)0x05000200)
#define CHARBLOCK(n)  ((volatile u32*)(0x06000000 + (n) * 0x4000))
#define SCREENBLOCK(n) ((volatile u16*)(0x06000000 + (n) * 0x800))
#define OBJ_TILES ((volatile u32*)0x06010000)
#define OAM       ((volatile u16*)0x07000000)

#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200

#define SCREEN_W 240
#define SCREEN_H 160
#define TEXT_SB  26
#define MAP_SB   28

enum { GRASS, GRASS_TUFT, DIRT, SOIL, SPROUT, CROP, CROP_RIPE, FENCE, BUSH,
       FENCE_V, FENCE_TL, FENCE_TR, FENCE_BL, FENCE_BR, CROP_YOUNG };   // FENCE_V.. are looks only; the map stores FENCE
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };
// Sprite cells in sprites.png (4 tiles each in OBJ VRAM)
enum { SPR_FARMER_DOWN, SPR_FARMER_UP, SPR_FARMER_SIDE, SPR_CROW_A, SPR_CROW_B, SPR_MARKER };

// ---------- small helpers ----------
static void copy32(volatile u32 *dst, const u32 *src, u32 words) {
    while (words--) *dst++ = *src++;
}

static u32 rng_state = 1;
static u32 rng(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 16;
}

static u16 keys_now, keys_prev;
static void poll_keys(void) {
    keys_prev = keys_now;
    keys_now = ~REG_KEYINPUT & 0x03FF;
}
#define HELD(k) (keys_now & (k))
#define HIT(k)  ((keys_now & ~keys_prev) & (k))

static void vsync(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
    // Reload globals from memory each frame, so test scripts that poke variables take effect.
    __asm__ volatile("" ::: "memory");
}

// ---------- sound: short chimes on channel 1 ----------
static void sound_init(void) {
    REG_SNDCNT_X = 0x0080;   // master on
    REG_SNDCNT_L = 0x1177;   // channel 1 on both sides, full volume
    REG_SNDCNT_H = 0x0002;   // PSG at 100%
    REG_SND1SWEEP = 0x0008;  // sweep off
}
static void chime(u32 hz, u32 vol) {
    REG_SND1CNT = (vol << 12) | (2 << 8) | (2 << 6);      // decaying envelope, 50% duty
    REG_SND1FREQ = (u16)((2048 - 131072 / hz) | 0x8000);  // restart
}

// ---------- text (BG0) ----------
static u16 glyph_entry(char c) {
    for (u32 i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return (1 + i) | (1 << 12);   // palette bank 1
    return 1 | (1 << 12);
}
static void put_text(s32 col, s32 row, const char *s) {
    for (; *s && col < 30; s++, col++) SCREENBLOCK(TEXT_SB)[row * 32 + col] = glyph_entry(*s);
}
static void clear_row(s32 row) {
    for (s32 c = 0; c < 32; c++) SCREENBLOCK(TEXT_SB)[row * 32 + c] = 0;
}
// Writes n (0..99999) into buf, left-aligned. Returns the length.
static s32 fmt_num(char *buf, u32 n) {
    char tmp[6];
    s32 len = 0;
    do { tmp[len++] = '0' + n % 10; n /= 10; } while (n && len < 5);
    for (s32 i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = 0;
    return len;
}

static u32 msg_timer;
static void show_message(const char *s) {
    clear_row(19);
    s32 n = 0; while (s[n]) n++;
    s32 col = (30 - n - 2) / 2;
    put_text(col, 19, " ");
    put_text(col + 1, 19, s);
    put_text(col + 1 + n, 19, " ");
    msg_timer = 120;
}
static void update_message(void) {
    if (msg_timer && --msg_timer == 0) clear_row(19);
}

// sin table, 64 steps per turn, scaled by 256
static const signed short sin64[64] = {
    0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255,
    256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 121, 98, 74, 50, 25,
    0, -25, -50, -74, -98, -121, -142, -162, -181, -198, -213, -226, -237, -245, -251, -255,
    -256, -255, -251, -245, -237, -226, -213, -198, -181, -162, -142, -121, -98, -74, -50, -25,
};

// ---------- sprites ----------
static u16 oam_shadow[128 * 4];
static u32 oam_used;

static void oam_flush(void) {
    for (u32 i = 0; i < 128 * 4; i++) OAM[i] = oam_shadow[i];
}
static void oam_begin(void) { oam_used = 0; }
static void oam_end(void) {
    for (u32 i = oam_used; i < 128; i++) oam_shadow[i * 4] = 0x0200;   // hidden
}
// 16x16 sprite at screen (x, y), cell from sprites.png
static void put_sprite(s32 x, s32 y, u32 cell, u32 hflip) {
    if (oam_used >= 128) return;
    u16 *o = &oam_shadow[oam_used++ * 4];
    o[0] = (y & 0xFF);                                    // square shape
    o[1] = (x & 0x1FF) | (1 << 14) | (hflip << 12);       // 16x16
    o[2] = cell * 4;                                      // tile index, priority 0, palette 0
}

// ---------- farm ----------
static u8 farm[MAP_W * MAP_H];

static u8 tile_at(s32 mx, s32 my) {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return BUSH;
    return farm[my * MAP_W + mx];
}

// The map stores every fence as FENCE. Pick the piece that joins its fence neighbors.
// Index: up=1, down=2, left=4, right=8.
static const u8 fence_piece[16] = {
    FENCE, FENCE_V, FENCE_V, FENCE_V, FENCE, FENCE_BR, FENCE_TR, FENCE_V,
    FENCE, FENCE_BL, FENCE_TL, FENCE_V, FENCE, FENCE, FENCE, FENCE,
};

static u8 look_at(s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    if (t != FENCE) return t;
    u32 m = (tile_at(mx, my - 1) == FENCE) | (tile_at(mx, my + 1) == FENCE) << 1 |
            (tile_at(mx - 1, my) == FENCE) << 2 | (tile_at(mx + 1, my) == FENCE) << 3;
    return fence_piece[m];
}

static void draw_meta(s32 mx, s32 my) {
    u16 base = look_at(mx, my) * 4;
    for (s32 k = 0; k < 4; k++)
        SCREENBLOCK(MAP_SB)[(my * 2 + (k >> 1)) * 32 + mx * 2 + (k & 1)] = base + k;
}

static int is_solid(u8 t) {
    return t == FENCE || t == BUSH;
}

static void set_tile(s32 mx, s32 my, u8 t) {
    farm[my * MAP_W + mx] = t;
    draw_meta(mx, my);
}

// ---------- crops ----------
// Stages: SOIL (empty) -> SPROUT -> CROP_YOUNG -> CROP -> CROP_RIPE. Ripe crops wait to be harvested.
#define GROW_FRAMES 480   // frames per stage (8 s at 60 fps). DESIGN.md first guess.
static u16 grow[MAP_W * MAP_H];   // frames since the tile's last stage change

static u8 next_stage(u8 t) {
    return t == SPROUT ? CROP_YOUNG : t == CROP_YOUNG ? CROP : t == CROP ? CROP_RIPE : t;
}

static void update_crops(void) {
    for (s32 i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = farm[i];
        if (t != SPROUT && t != CROP_YOUNG && t != CROP) continue;
        if (++grow[i] < GROW_FRAMES) continue;
        grow[i] = 0;
        set_tile(i % MAP_W, i / MAP_W, next_stage(t));
    }
}

static void reset_farm(void) {
    for (u32 i = 0; i < MAP_W * MAP_H; i++) { farm[i] = farm_start[i]; grow[i] = 0; }
    for (s32 y = 0; y < MAP_H; y++)
        for (s32 x = 0; x < MAP_W; x++) draw_meta(x, y);
}

// ---------- farmer ----------
// Positions are in 1/16 pixel (fixed point) so speeds can be fractional.
static s32 px, py, pdir, pstep;

// Farmer's feet box: 10 px wide, 6 px tall, at the bottom of the 16x16 cell.
static int blocked(s32 x, s32 y) {
    s32 l = (x >> 4) + 3, r = (x >> 4) + 12, t = (y >> 4) + 10, b = (y >> 4) + 15;
    return is_solid(tile_at(l >> 4, t >> 4)) || is_solid(tile_at(r >> 4, t >> 4)) ||
           is_solid(tile_at(l >> 4, b >> 4)) || is_solid(tile_at(r >> 4, b >> 4));
}

static void update_farmer(void) {
    s32 speed = HELD(KEY_B) ? 32 : 20;
    s32 dx = 0, dy = 0;
    if (HELD(KEY_LEFT))  { dx = -speed; pdir = DIR_LEFT; }
    if (HELD(KEY_RIGHT)) { dx = speed;  pdir = DIR_RIGHT; }
    if (HELD(KEY_UP))    { dy = -speed; pdir = DIR_UP; }
    if (HELD(KEY_DOWN))  { dy = speed;  pdir = DIR_DOWN; }
    if (dx && !blocked(px + dx, py)) px += dx;
    if (dy && !blocked(px, py + dy)) py += dy;
    if (dx || dy) pstep++;
}

// ---------- farm work (A button) ----------
static s32 seeds, crops, coins, day, hearts;
static s32 tx, ty;   // the tile the farmer works on: just in front of his feet

static void update_target(void) {
    s32 fx = (px >> 4) + 8, fy = (py >> 4) + 12;   // feet center
    if (pdir == DIR_UP) fy -= 14;
    else if (pdir == DIR_DOWN) fy += 14;
    else if (pdir == DIR_LEFT) fx -= 14;
    else fx += 14;
    tx = fx >> 4;
    ty = fy >> 4;
}

static int is_field(u8 t) {
    return t == SOIL || t == SPROUT || t == CROP_YOUNG || t == CROP || t == CROP_RIPE;
}

static void draw_hud(void);

static void farm_work(void) {
    if (!HIT(KEY_A)) return;
    u8 t = tile_at(tx, ty);
    if (t == CROP_RIPE) {
        set_tile(tx, ty, SOIL);
        crops++;
        chime(1047, 12);
        draw_hud();
    } else if (t == SOIL) {
        if (seeds <= 0) { show_message("NO SEEDS!"); chime(220, 8); return; }
        seeds--;
        grow[ty * MAP_W + tx] = 0;
        set_tile(tx, ty, SPROUT);
        chime(659, 10);
        draw_hud();
    }
}

static void draw_marker(void) {
    if (is_field(tile_at(tx, ty))) put_sprite(tx * 16, ty * 16, SPR_MARKER, 0);
}

// Top row: day, coins, crops, seeds, hearts. Row 0 of the map is bushes, so nothing is hidden.
static void draw_hud(void) {
    char buf[8];
    put_text(0, 0, "                              ");
    put_text(1, 0, "DAY");
    fmt_num(buf, day);    put_text(5, 0, buf);
    put_text(9, 0, "$");  fmt_num(buf, coins); put_text(10, 0, buf);
    put_text(15, 0, "&"); fmt_num(buf, crops); put_text(16, 0, buf);
    put_text(20, 0, "^"); fmt_num(buf, seeds); put_text(21, 0, buf);
    for (s32 i = 0; i < hearts && i < 5; i++) put_text(25 + i, 0, "*");
}

static void draw_farmer(void) {
    u32 cell = pdir == DIR_DOWN ? SPR_FARMER_DOWN : pdir == DIR_UP ? SPR_FARMER_UP : SPR_FARMER_SIDE;
    s32 bob = (pstep >> 3) & 1;   // 1 px walk bob
    put_sprite(px >> 4, (py >> 4) - bob, cell, pdir == DIR_LEFT);
}

// ---------- test crow ----------
static u32 frame;   // game loop counter; the test harness steps by it

static void draw_crow(void) {
    u32 a = (frame >> 1) & 63;
    s32 x = 112 + (sin64[a] * 72 >> 8);
    s32 y = 56 + (sin64[(a + 16) & 63] * 32 >> 8);
    put_sprite(x, y, (frame >> 3) & 1 ? SPR_CROW_A : SPR_CROW_B, a < 32);
}

// ---------- main ----------
int main(void) {
    REG_DISPCNT = 0x0080;   // forced blank while loading

    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = bg_pal[i];
        PAL_OBJ[i] = obj_pal[i];
    }
    PAL_BG[16 + 1] = (29) | (28 << 5) | (22 << 10);   // text: cream
    PAL_BG[16 + 2] = (3) | (4 << 5) | (6 << 10);      // text background: dark slate
    PAL_BG[16 + 3] = (28) | (8 << 5) | (6 << 10);     // heart red
    PAL_BG[16 + 4] = (29) | (25 << 5) | (10 << 10);   // coin gold
    PAL_BG[16 + 5] = (13) | (21 << 5) | (9 << 10);    // crop green
    copy32(CHARBLOCK(0), terrain_tiles, sizeof(terrain_tiles) / 4);
    copy32(CHARBLOCK(1), font_tiles, sizeof(font_tiles) / 4);
    copy32(OBJ_TILES, sprite_tiles, sizeof(sprite_tiles) / 4);

    for (u32 i = 0; i < 128; i++) oam_shadow[i * 4] = 0x0200;
    oam_flush();

    for (u32 i = 0; i < 32 * 32; i++) SCREENBLOCK(TEXT_SB)[i] = 0;
    REG_BG0CNT = 0 | (1 << 2) | (TEXT_SB << 8);   // text: priority 0
    REG_BG1CNT = 2 | (0 << 2) | (MAP_SB << 8);    // map: priority 2, 32x32 tiles
    sound_init();
    reset_farm();
    px = PLAYER_START_X * 16 * 16;
    py = PLAYER_START_Y * 16 * 16;
    pdir = DIR_UP;
    day = 1; seeds = 3; crops = 0; coins = 0; hearts = 3;   // DESIGN.md first guesses
    draw_hud();

    REG_DISPCNT = 0x0000 | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);   // mode 0, 1D OBJ, BG0, BG1, OBJ

    for (;;) {
        vsync();
        oam_flush();

        poll_keys();
        frame++;
        rng_state += keys_now;
        (void)rng;

        update_farmer();
        update_target();
        farm_work();
        update_crops();
        update_message();

        oam_begin();
        draw_farmer();
        draw_marker();
        draw_crow();
        oam_end();
    }
}
