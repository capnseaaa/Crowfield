// Crowfield (GBA), v0.4: scarecrow turrets.
// One-screen farm. Crops grow on a timer. The farmer plants, harvests, and fights crows with a sword.
// Each day crows fly in, eat crops (and grow tougher), and peck the farmer. At 0 hearts he is knocked out for 3 s.
// When all of the day's crows are gone, dusk comes: sell crops and buy seeds, then the next day.
// No crops, no seeds, and no coins for a seed: the farm is lost. START begins a new run.
// D-pad: move   R: run   A: plant on empty soil / harvest a ripe crop (puts the sword away)
// B: draw and swing the sword (it stays in hand until A)
// The shop also sells sickle and sword upgrades and scarecrow turrets (SELECT places one on soil).

#include "data.h"
#include "sound.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed char    s8;
typedef signed short   s16;
typedef signed int     s32;

// ---------- hardware ----------
#define REG16(a) (*(volatile u16*)(a))
#define REG_DISPCNT   REG16(0x04000000)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG0CNT    REG16(0x04000008)
#define REG_BG1CNT    REG16(0x0400000A)
#define REG_KEYINPUT  REG16(0x04000130)
#define REG32(a) (*(volatile u32*)(a))
#define REG_SNDCNT_L  REG16(0x04000080)
#define REG_SNDCNT_H  REG16(0x04000082)
#define REG_SNDCNT_X  REG16(0x04000084)
#define REG_FIFO_A    0x040000A0
#define REG_DMA1SAD   REG32(0x040000BC)
#define REG_DMA1DAD   REG32(0x040000C0)
#define REG_DMA1CNT_H REG16(0x040000C6)
#define REG_TM0CNT_L  REG16(0x04000100)
#define REG_TM0CNT_H  REG16(0x04000102)

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
       FENCE_V, FENCE_TL, FENCE_TR, FENCE_BL, FENCE_BR, CROP_YOUNG, SCARECROW, SCARECROW_OFF,
       CARROT_GROWN, CARROT_RIPE, TOMATO_GROWN, TOMATO_RIPE, PUMPKIN_GROWN, PUMPKIN_RIPE };   // FENCE_V.. are looks only; the map stores FENCE
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };
// Sprite cells in sprites.png (4 tiles each in OBJ VRAM)
enum { SPR_FARMER_DOWN, SPR_FARMER_UP, SPR_FARMER_SIDE, SPR_CROW_A, SPR_CROW_B, SPR_MARKER,
       SPR_SWORD_RIGHT, SPR_SWORD_UP, SPR_PUFF_BIG, SPR_PUFF_SMALL, SPR_FARMER_KO, SPR_SWORD_DIAG, SPR_SHOT,
       SPR_FIREFLY, SPR_FIREFLY_DIM };
// put_sprite flags
#define HFLIP 1
#define VFLIP 2
#define FLASH 0x10   // OBJ palette bank 1: all white
#define BANK(n) ((n) << 4)   // OBJ palette bank n: 2 red crow, 3 purple crow, 4 gold crow, 5 fireflies
#define BEHIND 0x100         // draw behind the text layer (BG0), e.g. fireflies behind the shop window

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

// ---------- sound: sample clips (sound/*.wav -> sound.h) on Direct Sound A ----------
// Timer 0 runs at SND_RATE (224 samples per frame). DMA 1 feeds FIFO A from a double buffer.
// Each frame, right after vsync, sound_vblank() restarts the DMA every other frame and
// sound_mix() fills the half that plays next. A frame that runs long replays stale samples.
enum { SND_WHOOSH, SND_HIT, SND_CAW, SND_HURT, SND_PLANT, SND_HARVEST, SND_NOSEEDS, SND_POOF, SND_KNOCKDOWN };   // order of SOUNDS in make_sound.py
#define MIX_LEN  224
#define MIX_CHANS 4
static s8 mix_buf[2 * MIX_LEN] __attribute__((aligned(4)));
static u32 mix_playing;                     // half the DMA plays this frame
static const s8 *ch_data[MIX_CHANS];        // 0 = channel free
static u32 ch_pos[MIX_CHANS], ch_len[MIX_CHANS], ch_snd[MIX_CHANS], ch_age[MIX_CHANS];
static u32 snd_count;                       // sounds started; tests read it

static void sound_init(void) {
    REG_SNDCNT_X = 0x0080;   // master on
    REG_SNDCNT_L = 0;        // PSG channels off
    REG_SNDCNT_H = 0x0B04 | 0x0002;   // DMA A 100%, left + right, timer 0, FIFO reset; PSG 100%
    REG_TM0CNT_L = (u16)(65536 - 1254);   // 1254 cycles per sample = 280896 per frame / MIX_LEN
    REG_TM0CNT_H = 0x0080;
}
static void sound_vblank(void) {
    if (mix_playing == 0) {   // the DMA ran through both halves: restart at the first
        REG_DMA1CNT_H = 0;
        REG_DMA1SAD = (u32)mix_buf;
        REG_DMA1DAD = REG_FIFO_A;
        REG_DMA1CNT_H = 0xB640;   // enable, sound FIFO timing, 32-bit, repeat, fixed destination
    }
}
static void sound_mix(void) {
    static s16 acc[MIX_LEN];
    s8 *out = mix_buf + (mix_playing ^ 1) * MIX_LEN;
    for (u32 i = 0; i < MIX_LEN; i++) acc[i] = 0;
    for (u32 c = 0; c < MIX_CHANS; c++) {
        if (!ch_data[c]) continue;
        u32 n = ch_len[c] - ch_pos[c];
        if (n > MIX_LEN) n = MIX_LEN;
        const s8 *src = ch_data[c] + ch_pos[c];
        for (u32 i = 0; i < n; i++) acc[i] += src[i];
        ch_pos[c] += n;
        if (ch_pos[c] >= ch_len[c]) ch_data[c] = 0;
    }
    for (u32 i = 0; i < MIX_LEN; i++) {
        s32 v = acc[i];
        out[i] = (s8)(v > 127 ? 127 : v < -128 ? -128 : v);
    }
    mix_playing ^= 1;
}
// Start a clip. The same clip restarts on its own channel; else a free channel; else the oldest.
static void play(u32 snd) {
    u32 c, pick = 0;
    for (c = 0; c < MIX_CHANS; c++) if (ch_data[c] && ch_snd[c] == snd) break;
    if (c == MIX_CHANS) {
        for (c = 0; c < MIX_CHANS; c++) if (!ch_data[c]) break;
        if (c == MIX_CHANS) {
            for (c = 1; c < MIX_CHANS; c++) if (ch_age[c] < ch_age[pick]) pick = c;
            c = pick;
        }
    }
    ch_data[c] = snd_data[snd];
    ch_len[c] = snd_len[snd];
    ch_pos[c] = 0;
    ch_snd[c] = snd;
    ch_age[c] = ++snd_count;
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
// Copies s into buf (no libc). Returns the length. Use instead of char buf[n] = "..." (that calls memset).
static s32 copy_str(char *buf, const char *s) {
    s32 n = 0;
    while (s[n]) { buf[n] = s[n]; n++; }
    buf[n] = 0;
    return n;
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
// 16x16 sprite at screen (x, y), cell from sprites.png. flags: HFLIP, VFLIP, FLASH, BANK(n), BEHIND.
static void put_sprite(s32 x, s32 y, u32 cell, u32 flags) {
    if (oam_used >= 128) return;
    u16 *o = &oam_shadow[oam_used++ * 4];
    o[0] = (y & 0xFF);                                            // square shape
    o[1] = (x & 0x1FF) | (1 << 14) | ((flags & 3) << 12);         // 16x16, flips
    o[2] = cell * 4 | (((flags >> 4) & 15) << 12) | ((flags & BEHIND) ? 1 << 10 : 0);   // tile, priority, bank
}

// ---------- farm ----------
static u8 farm[MAP_W * MAP_H] __attribute__((aligned(4)));   // aligned: tests poke it 4 bytes at a time

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
    return t == FENCE || t == BUSH || t == SCARECROW || t == SCARECROW_OFF;
}

static void set_tile(s32 mx, s32 my, u8 t) {
    farm[my * MAP_W + mx] = t;
    draw_meta(mx, my);
}

// ---------- crops ----------
// Four crops (v0.5). Stages: SOIL (empty) -> SPROUT -> CROP_YOUNG -> grown -> ripe. Sprout and young look the
// same for every crop; grown and ripe have their own tiles (corn's are CROP and CROP_RIPE). crop_kind[]
// remembers what each tile grows. Ripe crops wait to be harvested. Numbers: docs/V0.5_PLAN.md.
enum { CORN, CARROT, TOMATO, PUMPKIN, KINDS };
static const u8 grown_tile[KINDS] = { CROP, CARROT_GROWN, TOMATO_GROWN, PUMPKIN_GROWN };
static const u8 ripe_tile[KINDS] = { CROP_RIPE, CARROT_RIPE, TOMATO_RIPE, PUMPKIN_RIPE };
static const u16 grow_frames[KINDS] = { 480, 640, 800, 1120 };   // per stage; 3 stages: 24, 32, 40, 56 s
static const s32 seed_price[KINDS] = { 1, 2, 3, 5 };
static const s32 sell_price[KINDS] = { 2, 5, 8, 14 };   // Caleb: corn 3 -> 2 to balance the new crops
static const s32 unlock_day[KINDS] = { 1, 5, 12, 20 };
static const char *const crop_name[KINDS] = { "CORN", "CARROT", "TOMATO", "PUMPKIN" };
static const char *const seed_icon[KINDS] = { "^", "{", "}", "|" };   // font glyphs: one colored icon per seed
static u8 crop_kind[MAP_W * MAP_H] __attribute__((aligned(4)));
static u16 grow[MAP_W * MAP_H];   // frames since the tile's last stage change

static int is_grown(u8 t) { return t == CROP || t == CARROT_GROWN || t == TOMATO_GROWN || t == PUMPKIN_GROWN; }
static int is_ripe(u8 t) { return t == CROP_RIPE || t == CARROT_RIPE || t == TOMATO_RIPE || t == PUMPKIN_RIPE; }
static int is_crop(u8 t) { return t == SPROUT || t == CROP_YOUNG || is_grown(t) || is_ripe(t); }

static u8 next_stage(s32 i) {
    u8 t = farm[i];
    return t == SPROUT ? CROP_YOUNG : t == CROP_YOUNG ? grown_tile[crop_kind[i]] : is_grown(t) ? ripe_tile[crop_kind[i]] : t;
}
static u8 eaten_stage(s32 i) {   // one crow bite takes a crop down one stage
    u8 t = farm[i];
    return is_ripe(t) ? grown_tile[crop_kind[i]] : is_grown(t) ? CROP_YOUNG : t == CROP_YOUNG ? SPROUT : SOIL;
}

static void update_crops(void) {
    for (s32 i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = farm[i];
        if (t != SPROUT && t != CROP_YOUNG && !is_grown(t)) continue;
        if (++grow[i] < grow_frames[crop_kind[i]]) continue;
        grow[i] = 0;
        set_tile(i % MAP_W, i / MAP_W, next_stage(i));
    }
}

static void reset_farm(void) {
    for (u32 i = 0; i < MAP_W * MAP_H; i++) { farm[i] = farm_start[i]; grow[i] = 0; crop_kind[i] = CORN; }
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

// Combat state for the farmer (used by update_farmer, so declared here)
static s32 hearts, max_hearts;
static s32 hurt_timer;           // > 0: just hit, cannot be hit again (blinks)
static s32 ko_timer;             // > 0: knocked out, cannot act
static s32 pvx, pvy;             // knockback velocity, 1/16 px per frame
static s32 weapon_out;           // sword in hand: B draws it, A puts it away
#define HURT_FRAMES 60
#define KO_FRAMES 180            // 3 seconds (Caleb's call)

static void update_farmer(void) {
    if (pvx || pvy) {            // knockback slide, stopped by fences and bushes
        if (!blocked(px + pvx, py)) px += pvx;
        if (!blocked(px, py + pvy)) py += pvy;
        pvx -= pvx / 4 + (pvx > 0) - (pvx < 0);
        pvy -= pvy / 4 + (pvy > 0) - (pvy < 0);
    }
    if (hurt_timer) hurt_timer--;
    if (ko_timer) {
        if (--ko_timer == 0) { hearts = max_hearts; hurt_timer = HURT_FRAMES; }
        weapon_out = 0;
        return;
    }
    s32 speed = HELD(KEY_R) ? 32 : 20;
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
static s32 seeds[KINDS], crops[KINDS], coins, day;   // seeds and crops in the bag, per crop ($seeds = corn)
static s32 sel_seed;                                 // the seed A plants; L cycles it (Caleb)

static s32 crops_total(void) { s32 n = 0; for (s32 k = 0; k < KINDS; k++) n += crops[k]; return n; }
static s32 crops_value(void) { s32 n = 0; for (s32 k = 0; k < KINDS; k++) n += crops[k] * sell_price[k]; return n; }
static s32 seeds_total(void) { s32 n = 0; for (s32 k = 0; k < KINDS; k++) n += seeds[k]; return n; }
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
    return t == SOIL || is_crop(t) || t == SCARECROW || t == SCARECROW_OFF;
}

static void draw_hud(void);

static s32 swing;        // > 0: sword swing frames left
static s32 sickle_lv = 1;   // tiles worked per A press, in a line ahead: 1, 2, or 3 (shop; Caleb's rule)

// The sickle's tiles: the one in front, then the next ones straight ahead. Returns the count (= sickle_lv).
static s32 sickle_tiles(s32 *xs, s32 *ys) {
    s32 dx = pdir == DIR_RIGHT ? 1 : pdir == DIR_LEFT ? -1 : 0;
    s32 dy = pdir == DIR_DOWN ? 1 : pdir == DIR_UP ? -1 : 0;
    for (s32 k = 0; k < sickle_lv; k++) { xs[k] = tx + dx * k; ys[k] = ty + dy * k; }
    return sickle_lv;
}

static s32 scarecrow_bag;   // scarecrows bought but not placed (defined with the scarecrows below)

// A placed scarecrow or empty soil with a scarecrow in the bag: A waits to see if it is a hold
// (place or pick up, in scarecrow_work) or a tap (farm_tiles on release).
static int a_waits(void) {
    u8 t = tile_at(tx, ty);
    return t == SCARECROW || t == SCARECROW_OFF || (t == SOIL && scarecrow_bag > 0);
}

static void farm_tiles(void);
static void farm_work(void) {
    if (!HIT(KEY_A) || ko_timer || swing) return;
    weapon_out = 0;
    if (!a_waits()) farm_tiles();
}

// On each sickle tile, harvest a ripe crop or plant a seed in empty soil.
static void farm_tiles(void) {
    s32 xs[3], ys[3], n = sickle_tiles(xs, ys), harvested = 0, planted = 0, wanted_seed = 0;
    for (s32 k = 0; k < n; k++) {
        u8 t = tile_at(xs[k], ys[k]);
        if (is_ripe(t)) {
            set_tile(xs[k], ys[k], SOIL);
            crops[crop_kind[ys[k] * MAP_W + xs[k]]]++;
            harvested++;
        } else if (t == SOIL) {
            if (seeds[sel_seed] <= 0) { wanted_seed = 1; continue; }
            seeds[sel_seed]--;
            crop_kind[ys[k] * MAP_W + xs[k]] = sel_seed;
            grow[ys[k] * MAP_W + xs[k]] = 0;
            set_tile(xs[k], ys[k], SPROUT);
            planted++;
        }
    }
    if (harvested || planted) {
        play(harvested ? SND_HARVEST : SND_PLANT);
        draw_hud();
    } else if (wanted_seed) {
        show_message("NO SEEDS!");
        play(SND_NOSEEDS);
    }
}

// L: plant the next unlocked seed (Caleb). Shows its name and count.
static void seed_work(void) {
    if (!HIT(KEY_L) || ko_timer) return;
    do sel_seed = sel_seed + 1 < KINDS ? sel_seed + 1 : 0; while (day < unlock_day[sel_seed]);
    char buf[24];
    s32 n = copy_str(buf, crop_name[sel_seed]);
    n += copy_str(buf + n, " SEEDS: ");
    fmt_num(buf + n, seeds[sel_seed]);
    show_message(buf);
    draw_hud();
}

static void draw_marker(void) {
    s32 xs[3], ys[3], n = sickle_tiles(xs, ys);
    for (s32 k = 0; k < n; k++)
        if (is_field(tile_at(xs[k], ys[k]))) put_sprite(xs[k] * 16, ys[k] * 16, SPR_MARKER, 0);
}

// Top row: day, coins, crops, seeds, hearts. Row 0 of the map is bushes, so nothing is hidden.
static void draw_hud(void) {
    char buf[8];
    put_text(0, 0, "                              ");
    put_text(1, 0, "DAY");
    fmt_num(buf, day);    put_text(5, 0, buf);
    put_text(9, 0, "$");  fmt_num(buf, coins); put_text(10, 0, buf);
    put_text(15, 0, "&"); fmt_num(buf, crops_total()); put_text(16, 0, buf);
    put_text(20, 0, seed_icon[sel_seed]); fmt_num(buf, seeds[sel_seed]); put_text(21, 0, buf);   // the chosen seed
    for (s32 i = 0; i < hearts && i < 5; i++) put_text(25 + i, 0, "*");
}

static u32 frame;   // game loop counter; the test harness steps by it

static void draw_farmer(void) {
    if (ko_timer) { put_sprite(px >> 4, py >> 4, SPR_FARMER_KO, 0); return; }
    if (hurt_timer && (frame & 4)) return;   // blink while hurt
    u32 cell = pdir == DIR_DOWN ? SPR_FARMER_DOWN : pdir == DIR_UP ? SPR_FARMER_UP : SPR_FARMER_SIDE;
    s32 bob = (pstep >> 3) & 1;   // 1 px walk bob
    put_sprite(px >> 4, (py >> 4) - bob, cell, pdir == DIR_LEFT ? HFLIP : 0);
}

// ---------- crows ----------
// Each day, crows fly in one by one from the screen edges. Each flies to the nearest free crop, lands, and eats it one
// stage per bite. Crows have levels (v0.5): Lv.1 black, Lv.2 red, Lv.3 purple, Lv.4 gold; the day table decides
// how many of each come. A crow that has eaten its 3rd whole crop becomes Lv.3 (purple; Caleb). With no
// crops left, crows go for the farmer. Any crow pecks the farmer when he comes close.
#define MAX_CROWS 24
enum { CROW_NONE, CROW_FLY, CROW_HURT, CROW_DEAD, CROW_EAT };
// Separate arrays (not structs) so tests can poke $crow_hp+4 etc. Positions in 1/16 px, sprite top-left.
static s32 crow_state[MAX_CROWS], crow_x[MAX_CROWS], crow_y[MAX_CROWS], crow_hp[MAX_CROWS];
static s32 crow_vx[MAX_CROWS], crow_vy[MAX_CROWS], crow_timer[MAX_CROWS];
static s32 crow_flash[MAX_CROWS], crow_cool[MAX_CROWS], crow_hit_by[MAX_CROWS];
static s32 crow_target[MAX_CROWS];   // field index of its crop, or -1: the farmer
static s32 crow_lv[MAX_CROWS];       // 1 black, 2 red, 3 purple, 4 gold (0, from a test poke, acts as 1)
static s32 crow_ate[MAX_CROWS];      // whole crops eaten; the 3rd makes it Lv.3
static s32 spawning;                 // 1: days run (tests set 0 and place crows by hand)
static s32 day_left, spawn_timer;    // crows still to come today; frames toward the next one
static s32 day_done;                 // > 0: all of today's crows are gone; frames until dusk
// Per level (index 0 = a poked crow, same as Lv.1): HP, speed in 1/16 px per frame, frames per bite.
static const s32 crow_hp_of[5] = { 1, 1, 2, 3, 5 };          // gold 5 (Caleb, v0.5.1; was 4)
static const s32 crow_speed_of[5] = { 6, 6, 7, 8, 9 };          // +15%, +30%, +45% (docs/V0.5_PLAN.md)
static const s32 crow_bite_of[5] = { 120, 120, 120, 90, 60 };
#define CROW_GOLD 4
#define GOLD_COINS 5       // a gold crow drops 5 coins when killed
#define PECK_COOL 90
#define MAX_DAY 30           // crows in one day; only MAX_CROWS fit on screen, the rest wait for a free slot
static s32 day_queue[MAX_DAY];       // levels of today's crows still to come, spawned from the end
#define DAY_START_DELAY 120   // 2 s from a new day to its first crow
#define DAY_DONE_FRAMES 900   // 15 s to harvest and plant after the last crow, before dusk (Caleb)

static s32 abs32(s32 v) { return v < 0 ? -v : v; }

static int field_has_crops(void) {
    for (s32 i = 0; i < MAP_W * MAP_H; i++) if (is_crop(farm[i])) return 1;
    return 0;
}

// Nearest crop no other crow is after; else the nearest crop; else -1 (go for the farmer).
static s32 pick_target(s32 c) {
    s32 best = -1, best_d = 0x7FFFFFFF, shared = -1, shared_d = 0x7FFFFFFF;
    s32 cx = crow_x[c] >> 4, cy = crow_y[c] >> 4;
    for (s32 y = 0; y < MAP_H; y++)
        for (s32 x = 0; x < MAP_W; x++) {
            s32 t = y * MAP_W + x;
            if (!is_crop(farm[t])) continue;
            s32 d = abs32(x * 16 - cx) + abs32(y * 16 - cy);
            s32 taken = 0;
            for (s32 o = 0; o < MAX_CROWS; o++)
                if (o != c && (crow_state[o] == CROW_FLY || crow_state[o] == CROW_EAT || crow_state[o] == CROW_HURT)
                    && crow_target[o] == t) taken = 1;
            if (!taken && d < best_d) { best = t; best_d = d; }
            if (d < shared_d) { shared = t; shared_d = d; }
        }
    return best >= 0 ? best : shared;
}

static int spawn_crow(void) {   // 0 if all MAX_CROWS slots are busy (the crow waits)
    for (s32 i = 0; i < MAX_CROWS; i++) {
        if (crow_state[i] != CROW_NONE) continue;
        u32 side = rng() & 3, along = rng();
        s32 x = side == 0 ? -16 : side == 1 ? 240 : 16 + along % 208;
        s32 y = side == 2 ? -16 : side == 3 ? 160 : 24 + along % 112;
        crow_x[i] = x * 16; crow_y[i] = y * 16;
        crow_state[i] = CROW_FLY;
        crow_lv[i] = day_queue[day_left > 0 ? day_left - 1 : 0];
        if (crow_lv[i] < 1) crow_lv[i] = 1;
        crow_hp[i] = crow_hp_of[crow_lv[i]];
        crow_ate[i] = 0;
        crow_flash[i] = 0; crow_cool[i] = 60; crow_vx[i] = crow_vy[i] = 0;
        crow_target[i] = pick_target(i);
        play(SND_CAW);
        return 1;
    }
    return 0;
}

static void hurt_farmer(s32 from_x, s32 from_y) {
    if (hurt_timer || ko_timer) return;
    hearts--;
    s32 dx = px - from_x, dy = py - from_y;
    pvx = dx > 0 ? 48 : dx < 0 ? -48 : 0;
    pvy = dy > 0 ? 48 : dy < 0 ? -48 : 0;
    play(SND_HURT);
    if (hearts <= 0) { hearts = 0; ko_timer = KO_FRAMES; show_message("KNOCKED OUT!"); play(SND_KNOCKDOWN); }
    else hurt_timer = HURT_FRAMES;
    draw_hud();
}

// Peck the farmer if he is within reach px. Returns 1 if the crow pecked.
static int try_peck(s32 i, s32 reach) {
    s32 dx = px - crow_x[i], dy = py - crow_y[i];
    if (crow_cool[i] || abs32(dx) >= reach * 16 || abs32(dy) >= reach * 16) return 0;
    hurt_farmer(crow_x[i], crow_y[i]);
    crow_cool[i] = PECK_COOL;
    crow_state[i] = CROW_HURT;   // bounce back after the peck
    crow_timer[i] = 12;
    crow_vx[i] = dx > 0 ? -40 : 40;
    crow_vy[i] = dy > 0 ? -40 : 40;
    return 1;
}

// Move crow i toward (x, y) in 1/16 px. Returns 1 on arrival.
static int fly_to(s32 i, s32 x, s32 y) {
    s32 dx = x - crow_x[i], dy = y - crow_y[i];
    s32 ax = abs32(dx), ay = abs32(dy);
    s32 v = crow_speed_of[crow_lv[i]];
    s32 sx = ax >= ay / 2 ? v : 0, sy = ay >= ax / 2 ? v : 0;
    if (ax > sx) crow_x[i] += dx > 0 ? sx : -sx; else crow_x[i] = x;
    if (ay > sy) crow_y[i] += dy > 0 ? sy : -sy; else crow_y[i] = y;
    if (dx) crow_vx[i] = dx;   // remembered for facing
    return crow_x[i] == x && crow_y[i] == y;
}

static void bite(s32 i) {
    s32 t = crow_target[i];
    u8 left = eaten_stage(t);
    grow[t] = 0;
    set_tile(t % MAP_W, t / MAP_W, left);
    if (left != SOIL) return;
    crow_state[i] = CROW_FLY;                     // crop gone
    crow_target[i] = pick_target(i);
    if (++crow_ate[i] >= 3 && crow_lv[i] < 3) {   // its 3rd whole crop: it becomes Lv.3 (purple)
        crow_hp[i] += crow_hp_of[3] - crow_hp_of[crow_lv[i]];
        crow_lv[i] = 3;
    }
}

// ---------- dusk look (v0.5, Caleb's pick) ----------
// During the 15 s "DUSK IN n" countdown the field warms toward sunset orange; when the shop opens it is night
// (dark blue) with fireflies around the window. The next day starts bright again.
static u16 tint(u16 c, s32 warm, s32 night) {   // warm, night: 0..256
    s32 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    r += ((31 - r) * warm >> 8) - (r * warm >> 10);   // toward orange: more red,
    g += ((15 - g) * warm >> 8);                      // a middle green,
    b -= (b * warm >> 8) * 2 / 3;                     // less blue
    r -= r * night * 11 >> 12;                        // night: about 30% red, 45% green, 70% blue
    g -= g * night * 9 >> 12;
    b -= b * night * 5 >> 12;
    b += 3 * night >> 8;
    r = r < 0 ? 0 : r > 31 ? 31 : r;
    g = g < 0 ? 0 : g > 31 ? 31 : g;
    b = b < 0 ? 0 : b > 31 ? 31 : b;
    return (u16)(r | (g << 5) | (b << 10));
}
static void set_light(s32 warm, s32 night) {   // field palette, and the farmer's while it is still day
    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = tint(bg_pal[i], warm, night);
        PAL_OBJ[i] = night ? obj_pal[i] : tint(obj_pal[i], warm, 0);
    }
}
#define SUNSET_MAX 96    // how orange the field gets at the end of the countdown (of 256)

#define FIREFLIES 9
static const u8 fly_x[FIREFLIES] = { 6, 8, 224, 226, 220, 52, 150, 108, 190 };   // around the shop window
static const u8 fly_y[FIREFLIES] = { 36, 96, 28, 88, 128, 138, 142, 6, 10 };
static void draw_fireflies(void) {
    for (s32 k = 0; k < FIREFLIES; k++) {
        u32 t = frame + k * 37;
        s32 x = fly_x[k] - 8 + (sin64[(t >> 3) & 63] * 6 >> 8);         // drift slowly
        s32 y = fly_y[k] - 8 + (sin64[((t >> 2) + k * 9) & 63] * 4 >> 8);
        u32 phase = (t >> 1) & 63;                                     // blink: bright, dim, dark
        if (phase < 28) put_sprite(x, y, SPR_FIREFLY, BANK(5) | BEHIND);
        else if (phase < 44) put_sprite(x, y, SPR_FIREFLY_DIM, BANK(5) | BEHIND);
    }
}

static void open_shop(void);

// Random number in [a, b] (b >= a).
static s32 rand_in(s32 a, s32 b) { return b > a ? a + (s32)(rng() % (u32)(b - a + 1)) : a; }

// Today's crows (docs/V0.5_PLAN.md day table): a random total in the day's range and a random mix of levels,
// so runs differ. Days 1-9 black only; red from Day 10, purple from 20, gold from 30; black is a minority
// from Day 25; a few more crows from Day 35; from Day 40 all gold. Fills day_queue (shuffled), returns the count.
static s32 plan_day(s32 d) {
    s32 n[5];   // crows per level; assigned one by one (an initializer would call memset)
    n[0] = n[1] = n[2] = n[3] = n[4] = 0;
    s32 lo, hi, total;
    if (d >= 40) {
        lo = 12 + (d - 40) / 2;
        if (lo > 22) lo = 22;
        hi = lo + 2 > MAX_CROWS ? MAX_CROWS : lo + 2;
        n[4] = total = rand_in(lo, hi);
    } else {
        lo = 2 + d / 2 + (d >= 35 ? 2 : 0);   // Caleb, v0.5.1: a few more from Day 35
        hi = lo + (d < 10 ? 2 : d < 30 ? 3 : 4);
        if (hi > MAX_DAY) hi = MAX_DAY;
        total = rand_in(lo, hi);
        if (d >= 20) n[3] = rand_in(1 + (d - 20) / 3, 1 + (d - 20) / 3 + (d >= 25));
        if (d >= 30) n[4] = rand_in(1 + (d - 30) / 3, 1 + (d - 30) / 3 + (d >= 33));
        if (d < 10) n[1] = total;
        else if (d < 25) {
            n[2] = rand_in(1 + (d - 10) / 2, d > 10 ? 2 + (d - 10) / 2 : 1);
            n[1] = total - n[2] - n[3];
        } else {
            n[1] = rand_in(lo / 5, hi / 3);
            n[2] = total - n[1] - n[3] - n[4];
        }
        for (s32 k = 1; k <= 2; k++) if (n[k] < 0) { n[3 - k] += n[k]; n[k] = 0; }   // never negative
    }
    s32 c = 0;
    for (s32 lv = 1; lv <= 4; lv++)
        for (s32 k = 0; k < n[lv] && c < MAX_DAY; k++) day_queue[c++] = lv;
    for (s32 k = c - 1; k > 0; k--) {   // shuffle
        s32 j = rand_in(0, k), t = day_queue[k];
        day_queue[k] = day_queue[j];
        day_queue[j] = t;
    }
    return c;
}

static s32 spawn_gap(void) {                        // frames between crows: 2.5 s on day 1, then faster
    s32 g = 160 - 10 * day;
    return g < 45 ? 45 : g;
}

static void start_day(void) {
    char buf[12];
    fmt_num(buf + copy_str(buf, "DAY "), day);
    show_message(buf);
    draw_hud();
    day_left = plan_day(day);
    spawn_timer = spawn_gap() - DAY_START_DELAY;    // first crow after 2 s
    day_done = 0;
}

// A day ends when all of its crows have come and gone. Then dusk: the shop.
static void update_days(s32 alive) {
    if (!spawning) return;
    if (day_left > 0) {
        if (++spawn_timer >= spawn_gap() && spawn_crow()) { spawn_timer = 0; day_left--; }
        return;
    }
    if (alive) return;
    if (!day_done) {
        char buf[16];
        s32 n = copy_str(buf, "DAY ");
        n += fmt_num(buf + n, day);
        copy_str(buf + n, " DONE!");
        show_message(buf);
        day_done = DAY_DONE_FRAMES;
        return;
    }
    if (ko_timer) return;                     // wait until he is back on his feet
    if (--day_done > 0) {
        if ((day_done & 3) == 0)              // the sun goes down
            set_light((DAY_DONE_FRAMES - day_done) * SUNSET_MAX / DAY_DONE_FRAMES, 0);
        if (day_done % 60 == 0) {             // once a second: "DUSK IN n"
            char buf[16];
            fmt_num(buf + copy_str(buf, "DUSK IN "), day_done / 60);
            show_message(buf);
        }
        return;
    }
    open_shop();
}

static void update_crows(void) {
    s32 alive = 0;
    for (s32 i = 0; i < MAX_CROWS; i++) {
        s32 st = crow_state[i];
        if (st == CROW_NONE) continue;
        if (crow_flash[i]) crow_flash[i]--;
        if (crow_cool[i]) crow_cool[i]--;
        if (st == CROW_DEAD) {
            if (--crow_timer[i] <= 0) crow_state[i] = CROW_NONE;
            continue;
        }
        alive++;
        if (st == CROW_HURT) {   // knocked back, sliding to a stop
            crow_x[i] += crow_vx[i];
            crow_y[i] += crow_vy[i];
            crow_vx[i] -= crow_vx[i] / 4;
            crow_vy[i] -= crow_vy[i] / 4;
            if (--crow_timer[i] <= 0) crow_state[i] = CROW_FLY;
            continue;
        }
        s32 t = crow_target[i];
        if (st == CROW_EAT) {    // crows eat freely while the farmer is knocked out
            if (!is_crop(farm[t])) { crow_state[i] = CROW_FLY; crow_target[i] = pick_target(i); continue; }   // harvested
            if (!ko_timer && try_peck(i, 12)) continue;
            if (--crow_timer[i] <= 0) { crow_timer[i] = crow_bite_of[crow_lv[i]]; bite(i); }
            continue;
        }
        // Flying. Check the target: the crop may be gone, or a crow after the farmer may see new crops.
        if (t >= 0 ? !is_crop(farm[t]) : ((frame + i) & 31) == 0) t = crow_target[i] = pick_target(i);
        if (t < 0) {             // no crops: go for the farmer; hover while he is knocked out
            if (ko_timer) continue;
            fly_to(i, px, py);
            try_peck(i, 10);
            continue;
        }
        if (!ko_timer && try_peck(i, 10)) continue;
        if (fly_to(i, (t % MAP_W) * 256, (t / MAP_W) * 256 - 4 * 16)) {   // land on the crop, standing a bit high
            crow_state[i] = CROW_EAT;
            crow_timer[i] = crow_bite_of[crow_lv[i]];
        }
    }
    update_days(alive);
}

// Damage crow i; if it lives, knock it back with velocity (kvx, kvy) in 1/16 px per frame.
static void hit_crow(s32 i, s32 dmg, s32 kvx, s32 kvy) {
    crow_hp[i] -= dmg;
    crow_flash[i] = 8;
    if (crow_hp[i] <= 0) {
        crow_state[i] = CROW_DEAD;
        crow_timer[i] = 16;
        play(SND_POOF);
        if (crow_lv[i] == CROW_GOLD) {   // gold crows drop coins
            coins += GOLD_COINS;
            show_message("+5 COINS");
            draw_hud();
        }
        return;
    }
    crow_state[i] = CROW_HURT;
    crow_timer[i] = 14;
    crow_vx[i] = kvx;
    crow_vy[i] = kvy;
    play(SND_HIT);
}

static int crow_targetable(s32 i) {
    return crow_state[i] == CROW_FLY || crow_state[i] == CROW_HURT || crow_state[i] == CROW_EAT;
}

// ---------- scarecrow turrets ----------
// Bought at dusk, placed with SELECT on an empty soil tile (it takes that crop slot; SELECT again picks it
// up). Each shoots a seed at the nearest crow in range; every shot uses 1 of the farmer's corn seeds (Caleb:
// corn only), so with no corn seeds they stop. Holding B while facing one switches it off or on (Caleb); an off scarecrow
// is the gray SCARECROW_OFF tile and does not shoot. Levels (shop) apply to every scarecrow.
#define MAX_SCARECROWS 8
#define MAX_SHOTS 12
#define SHOT_SPEED 40      // 1/16 px per frame (2.5 px)
#define SHOT_LIFE 48
static s32 sc_x[MAX_SCARECROWS], sc_y[MAX_SCARECROWS], sc_cool[MAX_SCARECROWS];   // tile, frames to next shot
static s32 scarecrows;                   // placed (scarecrow_bag: bought, not placed)
static s32 scarecrow_lv = 1;             // 1. 2: shoots faster. 3: wider range, 2 shots
static s32 shot_x[MAX_SHOTS], shot_y[MAX_SHOTS], shot_vx[MAX_SHOTS], shot_vy[MAX_SHOTS], shot_life[MAX_SHOTS];
#define SWITCH_HOLD 30         // frames of held B that switch a scarecrow off or on
#define PLACE_HOLD 20          // frames of held A that place or pick up a scarecrow (Caleb: SELECT was not enough)
static s32 a_hold;           // > 0: frames A has been held on a waiting tile; -1: this hold already acted
static s32 sc_hold;          // frames B has been held while facing a scarecrow (stops counting after a switch)
static s32 sc_warned;                    // 1: "SCARECROWS NEED CORN" shown; cleared once corn comes back

static s32 scarecrow_reload(void) { return scarecrow_lv >= 2 ? 50 : 90; }   // frames between shots
static s32 scarecrow_range(void) { return scarecrow_lv >= 3 ? 88 : 64; }    // px

// Rough distance without a square root: the longer side plus half the shorter one.
static s32 approx_dist(s32 dx, s32 dy) {
    s32 ax = abs32(dx), ay = abs32(dy);
    return ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);
}

// Nearest targetable crow within range of point (x, y) in px, other than skip. -1 if none.
static s32 nearest_crow(s32 x, s32 y, s32 range, s32 skip) {
    s32 best = -1, best_d = range + 1;
    for (s32 i = 0; i < MAX_CROWS; i++) {
        if (i == skip || !crow_targetable(i)) continue;
        s32 d = approx_dist((crow_x[i] >> 4) + 8 - x, (crow_y[i] >> 4) + 8 - y);
        if (d < best_d) { best = i; best_d = d; }
    }
    return best;
}

static void fire_shot(s32 x, s32 y, s32 c) {   // from (x, y) px toward crow c's center; costs 1 corn seed
    if (seeds[CORN] <= 0) return;
    for (s32 k = 0; k < MAX_SHOTS; k++) {
        if (shot_life[k]) continue;
        s32 dx = (crow_x[c] >> 4) + 8 - x, dy = (crow_y[c] >> 4) + 8 - y;
        s32 d = approx_dist(dx, dy);
        if (d < 1) d = 1;
        shot_x[k] = x * 16; shot_y[k] = y * 16;
        shot_vx[k] = dx * SHOT_SPEED / d;   // one division per shot, not per frame
        shot_vy[k] = dy * SHOT_SPEED / d;
        shot_life[k] = SHOT_LIFE;
        seeds[CORN]--;
        draw_hud();
        return;
    }
}

static void update_scarecrows(void) {
    for (s32 s = 0; s < scarecrows; s++) {
        if (sc_cool[s]) { sc_cool[s]--; continue; }
        if (farm[sc_y[s] * MAP_W + sc_x[s]] == SCARECROW_OFF) continue;
        s32 x = sc_x[s] * 16 + 8, y = sc_y[s] * 16 + 5;   // the scarecrow's head
        s32 c = nearest_crow(x, y, scarecrow_range(), -1);
        if (c < 0) continue;
        if (seeds[CORN] <= 0) {  // out of ammo: tell him once
            if (!sc_warned) { show_message("SCARECROWS NEED CORN"); sc_warned = 1; }
            continue;
        }
        sc_warned = 0;
        fire_shot(x, y, c);
        if (scarecrow_lv >= 3) {
            s32 c2 = nearest_crow(x, y, scarecrow_range(), c);   // a 2nd crow, or the same one if it
            if (c2 < 0 && crow_hp[c] > 1) c2 = c;                 // needs 2 hits; never waste a seed
            if (c2 >= 0) fire_shot(x, y, c2);
        }
        sc_cool[s] = scarecrow_reload();
    }
    for (s32 k = 0; k < MAX_SHOTS; k++) {
        if (!shot_life[k]) continue;
        shot_x[k] += shot_vx[k];
        shot_y[k] += shot_vy[k];
        shot_life[k]--;
        s32 x = shot_x[k] >> 4, y = shot_y[k] >> 4;
        for (s32 i = 0; i < MAX_CROWS; i++) {
            if (!crow_targetable(i)) continue;
            if (abs32((crow_x[i] >> 4) + 8 - x) >= 8 || abs32((crow_y[i] >> 4) + 8 - y) >= 8) continue;
            hit_crow(i, 1, shot_vx[k], shot_vy[k]);
            shot_life[k] = 0;
            break;
        }
    }
}

static void pick_up_scarecrow(void) {
    for (s32 s = 0; s < scarecrows; s++)
        if (sc_x[s] == tx && sc_y[s] == ty) {
            scarecrows--;
            sc_x[s] = sc_x[scarecrows]; sc_y[s] = sc_y[scarecrows]; sc_cool[s] = sc_cool[scarecrows];
        }
    set_tile(tx, ty, SOIL);
    scarecrow_bag++;
    show_message("SCARECROW PICKED UP");
    play(SND_HARVEST);
}

static void place_scarecrow(void) {
    if (!scarecrow_bag) { show_message("BUY SCARECROWS AT DUSK"); play(SND_NOSEEDS); return; }
    if (scarecrows >= MAX_SCARECROWS) { show_message("NO ROOM FOR MORE"); play(SND_NOSEEDS); return; }
    sc_x[scarecrows] = tx; sc_y[scarecrows] = ty; sc_cool[scarecrows] = 30;
    scarecrows++;
    scarecrow_bag--;
    set_tile(tx, ty, SCARECROW);
    show_message("SCARECROW PLACED");
    play(SND_PLANT);
}

// Hold A (or press SELECT) facing empty soil: place a scarecrow; facing one: pick it up.
// A tap of A there farms as usual, on release. Hold B facing one for half a second: switch it off or on.
static void scarecrow_work(void) {
    u8 t = tile_at(tx, ty);
    int facing = t == SCARECROW || t == SCARECROW_OFF;
    if (!HELD(KEY_B) || !facing || ko_timer) sc_hold = 0;
    else if (sc_hold < SWITCH_HOLD && ++sc_hold == SWITCH_HOLD) {
        set_tile(tx, ty, t == SCARECROW ? SCARECROW_OFF : SCARECROW);
        show_message(t == SCARECROW ? "SCARECROW OFF" : "SCARECROW ON");
        play(SND_PLANT);
    }
    if (ko_timer || swing) { a_hold = 0; return; }
    if (HIT(KEY_A)) a_hold = a_waits() ? 1 : 0;
    else if (a_hold > 0 && HELD(KEY_A)) {
        if (++a_hold >= PLACE_HOLD) {
            a_hold = -1;
            if (facing) pick_up_scarecrow();
            else if (t == SOIL) place_scarecrow();
        }
    } else if (!HELD(KEY_A)) {
        if (a_hold > 0 && !facing) farm_tiles();   // it was a tap
        a_hold = 0;
    }
    if (!HIT(KEY_SELECT)) return;
    if (facing) pick_up_scarecrow();
    else if (t == SOIL) place_scarecrow();
}

static void draw_shots(void) {
    for (s32 k = 0; k < MAX_SHOTS; k++)
        if (shot_life[k]) put_sprite((shot_x[k] >> 4) - 8, (shot_y[k] >> 4) - 8, SPR_SHOT, 0);
}

static void draw_crows(void) {
    for (s32 i = 0; i < MAX_CROWS; i++) {
        s32 st = crow_state[i];
        if (st == CROW_NONE) continue;
        s32 x = crow_x[i] >> 4, y = crow_y[i] >> 4;
        if (st == CROW_DEAD) {
            put_sprite(x, y, crow_timer[i] > 8 ? SPR_PUFF_SMALL : SPR_PUFF_BIG, 0);
            continue;
        }
        u32 flags = (crow_vx[i] < 0 ? HFLIP : 0) | (crow_flash[i] ? FLASH : crow_lv[i] >= 2 ? BANK(crow_lv[i]) : 0);
        if (st == CROW_EAT) {    // wings folded; dips its head for the last few frames before a bite
            put_sprite(x, y + (crow_timer[i] < 10), SPR_CROW_B, flags);
            continue;
        }
        put_sprite(x, y, ((frame + i * 3) >> 3) & 1 ? SPR_CROW_A : SPR_CROW_B, flags);
    }
}

// ---------- sword ----------
#define SWING_FRAMES 12
static s32 swing_cool, swing_id;
static s32 sword_lv = 1;                         // 1, 2: more damage, 3: longer reach (shop)
static s32 sword_damage = 1, sword_reach = 10;   // set by set_sword_lv
static void set_sword_lv(s32 lv) {
    sword_lv = lv;
    sword_damage = lv >= 2 ? 2 : 1;
    sword_reach = lv >= 3 ? 16 : 10;
}
static s32 slash_x, slash_y;                     // center of the hit box, screen px

static void update_sword(void) {
    if (swing_cool) swing_cool--;
    if (!swing && HIT(KEY_B) && !swing_cool && !ko_timer) {
        swing = SWING_FRAMES;
        weapon_out = 1;
        swing_id++;
        play(SND_WHOOSH);
    }
    if (!swing) return;
    s32 cx = (px >> 4) + 8, cy = (py >> 4) + 8;
    slash_x = cx + (pdir == DIR_RIGHT ? sword_reach : pdir == DIR_LEFT ? -sword_reach : 0);
    slash_y = cy + (pdir == DIR_DOWN ? sword_reach : pdir == DIR_UP ? -sword_reach : 0);
    for (s32 i = 0; i < MAX_CROWS; i++) {
        if (crow_state[i] != CROW_FLY && crow_state[i] != CROW_HURT && crow_state[i] != CROW_EAT) continue;
        if (crow_hit_by[i] == swing_id) continue;   // one hit per swing
        s32 ex = (crow_x[i] >> 4) + 8 - slash_x, ey = (crow_y[i] >> 4) + 8 - slash_y;
        if (abs32(ex) > 14 || abs32(ey) > 14) continue;
        crow_hit_by[i] = swing_id;
        hit_crow(i, sword_damage, pdir == DIR_RIGHT ? 64 : pdir == DIR_LEFT ? -64 : 0,   // knocked away
                 pdir == DIR_DOWN ? 64 : pdir == DIR_UP ? -64 : 0);
    }
    if (--swing == 0) swing_cool = 6;
}

// Sword poses: which cell and flips point the blade each way.
enum { P_U, P_D, P_L, P_R, P_UR, P_UL, P_DR, P_DL };
static const u8 pose_cell[8] = { SPR_SWORD_UP, SPR_SWORD_UP, SPR_SWORD_RIGHT, SPR_SWORD_RIGHT,
                                 SPR_SWORD_DIAG, SPR_SWORD_DIAG, SPR_SWORD_DIAG, SPR_SWORD_DIAG };
static const u8 pose_flags[8] = { 0, VFLIP, HFLIP, 0, 0, HFLIP, VFLIP, HFLIP | VFLIP };
// Where the hand grips each unflipped cell (px in the 16x16 cell): right, up, up-right.
static s32 grip_x(u32 cell) { return cell == SPR_SWORD_RIGHT ? 2 : cell == SPR_SWORD_UP ? 8 : 4; }
static s32 grip_y(u32 cell) { return cell == SPR_SWORD_RIGHT ? 8 : 13; }
// Per facing (down, up, left, right): the swing's 3 poses, then the resting pose.
static const u8 swing_pose[4][4] = {
    { P_R, P_DR, P_D, P_DR },   // down
    { P_L, P_UL, P_U, P_UL },   // up
    { P_U, P_UL, P_L, P_DL },   // left
    { P_U, P_UR, P_R, P_DR },   // right
};
// The farmer's hand, relative to his sprite's top-left, per facing.
static const signed char hand_x[4] = { 12, 3, 3, 12 };
static const signed char hand_y[4] = { 12, 9, 10, 10 };

static void draw_sword(void) {
    if (!weapon_out || ko_timer) return;
    if (hurt_timer && (frame & 4)) return;   // blinks with the farmer
    u32 step = swing ? (SWING_FRAMES - swing) * 3 / SWING_FRAMES : 3;
    u32 pose = swing_pose[pdir][step];
    u32 cell = pose_cell[pose], flags = pose_flags[pose];
    s32 gx = grip_x(cell), gy = grip_y(cell);
    if (flags & HFLIP) gx = 15 - gx;
    if (flags & VFLIP) gy = 15 - gy;
    s32 bob = (pstep >> 3) & 1;
    put_sprite((px >> 4) + hand_x[pdir] - gx, (py >> 4) - bob + hand_y[pdir] - gy, cell, flags);
}

// ---------- dusk shop ----------
// Seed and crop prices: seed_price[] and sell_price[] with the crops.
// Upgrade prices for Lv.2 and Lv.3: about as many crops as the concept's 120 / 150 at 8 coins a crop.
static const s32 sickle_price[2] = { 45, 90 };
static const s32 sword_price[2] = { 60, 120 };
#define SCARECROW_PRICE 40   // the concept's 100 at 8-coin crops, rescaled to 3-coin crops
static const s32 scarecrow_price[2] = { 60, 120 };
// The shop has a main page and two sub-pages (Caleb: the one long list was too long). A opens a page or buys,
// B goes back to the main page. EXPANSION (bigger field) comes later.
enum { PAGE_MAIN, PAGE_SEEDS, PAGE_WEAPONS };
enum { MAIN_SELL, MAIN_SEEDS, MAIN_WEAPONS, MAIN_NEXT, MAIN_ITEMS };
enum { W_SICKLE, W_SWORD, W_SCARECROW, W_SCARECROW_LV, W_ITEMS };
static const s32 page_items[3] = { MAIN_ITEMS, KINDS, W_ITEMS };
static s32 shop_open, shop_page, shop_cursor, main_cursor;
// Window: text columns 3..26, rows 3..16 (centered between the HUD and the message row). One blank row
// above the title and below "YOU HAVE", so nothing sits on the edge (Caleb). The hint goes on row 19.
#define SHOP_L 3
#define SHOP_W 24
#define SHOP_T 3
#define SHOP_ROWS 14
#define SHOP_EQ 16     // column of "=" in an item line

static void shop_line(s32 row, const char *s) {
    put_text(SHOP_L, row, "                        ");
    put_text(SHOP_L + 1, row, s);
}
static void shop_centered(s32 row, const char *s) {   // a line centered in the window
    s32 n = 0;
    while (s[n]) n++;
    put_text(SHOP_L, row, "                        ");
    put_text(SHOP_L + (SHOP_W - n) / 2, row, s);
}
// Item line k (rows 7, 9, 11, 13): "  NAME" padded to SHOP_EQ, then "= $price" (price >= 0) or "MAX" (-1);
// price -2: the name only.
static void shop_item(s32 k, const char *name, s32 price) {
    char buf[26];
    s32 n = copy_str(buf, "  ");
    n += copy_str(buf + n, name);
    if (price != -2) while (n < SHOP_EQ) buf[n++] = ' ';
    buf[n] = 0;
    if (price == -1) copy_str(buf + n, "MAX");
    else if (price >= 0) fmt_num(buf + n + copy_str(buf + n, "= $"), price);
    shop_line(SHOP_T + 4 + 2 * k, buf);
}
// A tool line: "SICKLE LV2" and its price, or "SICKLE LV3" and MAX.
static void shop_level(s32 k, const char *name, s32 lv, const s32 *price) {
    char buf[20];
    s32 n = copy_str(buf, name);
    fmt_num(buf + n, lv >= 3 ? 3 : lv + 1);
    shop_item(k, buf, lv >= 3 ? -1 : price[lv - 1]);
}

static void draw_shop(void) {
    char buf[26];
    s32 n;
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) shop_line(r, "");
    if (shop_page == PAGE_MAIN) {
        n = copy_str(buf, "DUSK. DAY ");
        n += fmt_num(buf + n, day);
        copy_str(buf + n, " IS OVER.");
        shop_centered(SHOP_T + 1, buf);
        n = copy_str(buf, "SELL CROPS &");
        fmt_num(buf + n, crops_total());
        shop_item(MAIN_SELL, buf, crops_value());
        shop_item(MAIN_SEEDS, "SEEDS", -2);
        shop_item(MAIN_WEAPONS, "WEAPONS", -2);
        shop_item(MAIN_NEXT, "NEXT DAY", -2);
    } else if (shop_page == PAGE_SEEDS) {
        shop_centered(SHOP_T + 1, "SEEDS");
        for (s32 k = 0; k < KINDS; k++) {   // "^ CORN    X3 = $1", or "| PUMPKIN DAY 20" while locked
            n = copy_str(buf, seed_icon[k]);
            n += copy_str(buf + n, " ");
            n += copy_str(buf + n, crop_name[k]);
            while (n < 10) buf[n++] = ' ';
            buf[n] = 0;
            if (day < unlock_day[k]) {
                fmt_num(buf + n + copy_str(buf + n, "DAY "), unlock_day[k]);
                shop_item(k, buf, -2);
            } else {
                buf[n++] = 'X';
                fmt_num(buf + n, seeds[k]);
                shop_item(k, buf, seed_price[k]);
            }
        }
        s32 k = shop_cursor;   // the bottom line tells about the seed under the cursor
        if (day < unlock_day[k]) {
            fmt_num(buf + copy_str(buf, "UNLOCKS ON DAY "), unlock_day[k]);
        } else {
            n = copy_str(buf, "SELLS $");
            n += fmt_num(buf + n, sell_price[k]);
            n += copy_str(buf + n, "  GROWS ");
            n += fmt_num(buf + n, grow_frames[k] * 3 / 60);   // sprout -> young -> grown -> ripe
            copy_str(buf + n, " SEC");
        }
        shop_centered(SHOP_T + 12, buf);
    } else {
        shop_centered(SHOP_T + 1, "WEAPONS");
        shop_level(W_SICKLE, "SICKLE LV", sickle_lv, sickle_price);
        shop_level(W_SWORD, "SWORD LV", sword_lv, sword_price);
        shop_item(W_SCARECROW, "SCARECROW  #1", SCARECROW_PRICE);
        shop_level(W_SCARECROW_LV, "SCARECROW LV", scarecrow_lv, scarecrow_price);
    }
    put_text(SHOP_L + 1, SHOP_T + 4 + 2 * shop_cursor, ">");
    if (shop_page != PAGE_SEEDS) {   // (the SEEDS page uses this line for the seed under the cursor)
        n = copy_str(buf, "YOU HAVE $");
        n += fmt_num(buf + n, coins);
        n += copy_str(buf + n, " &");
        n += fmt_num(buf + n, crops_total());
        n += copy_str(buf + n, " ");
        n += copy_str(buf + n, seed_icon[sel_seed]);
        n += fmt_num(buf + n, seeds[sel_seed]);
        n += copy_str(buf + n, " #");
        fmt_num(buf + n, scarecrow_bag);
        shop_centered(SHOP_T + 12, buf);
    }
    clear_row(19);
    put_text(4, 19, shop_page == PAGE_MAIN ? "UP/DOWN PICK  A CHOOSE" : "A BUY  B BACK");
}

static void open_shop(void) {
    shop_open = 1;
    shop_page = PAGE_MAIN;
    shop_cursor = crops_total() ? MAIN_SELL : MAIN_SEEDS;
    swing = 0;
    set_light(0, 256);   // night
    msg_timer = 0;
    draw_shop();
}

static void close_shop(void) {
    shop_open = 0;
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) clear_row(r);
    clear_row(19);
    set_light(0, 0);     // a new morning
    day++;
    start_day();
    if (scarecrow_bag) show_message("HOLD A ON SOIL: SCARECROW");
}

// Buy the next level of a tool: 1 on success.
static int buy_level(s32 *lv, const s32 *price) {
    if (*lv >= 3 || coins < price[*lv - 1]) return 0;
    coins -= price[*lv - 1];
    (*lv)++;
    return 1;
}

// While the shop is open the farm is paused: no crows, no growth.
static void update_shop(void) {
    if (HIT(KEY_UP) && shop_cursor > 0) { shop_cursor--; draw_shop(); }
    if (HIT(KEY_DOWN) && shop_cursor < page_items[shop_page] - 1) { shop_cursor++; draw_shop(); }
    if (HIT(KEY_B) && shop_page != PAGE_MAIN) {
        shop_page = PAGE_MAIN;
        shop_cursor = main_cursor;
        draw_shop();
        return;
    }
    if (!HIT(KEY_A)) return;
    s32 ok = 1;
    if (shop_page == PAGE_MAIN) {
        if (shop_cursor == MAIN_SELL) {
            if ((ok = crops_total() > 0)) {
                coins += crops_value();
                for (s32 k = 0; k < KINDS; k++) crops[k] = 0;
            }
        } else if (shop_cursor == MAIN_NEXT) {
            close_shop();
            return;
        } else {                 // open a sub-page
            main_cursor = shop_cursor;
            shop_page = shop_cursor == MAIN_SEEDS ? PAGE_SEEDS : PAGE_WEAPONS;
            shop_cursor = 0;
            draw_shop();
            return;
        }
    } else if (shop_page == PAGE_SEEDS) {
        s32 k = shop_cursor;
        if ((ok = day >= unlock_day[k] && coins >= seed_price[k])) { coins -= seed_price[k]; seeds[k]++; }
    } else if (shop_cursor == W_SICKLE) {
        ok = buy_level(&sickle_lv, sickle_price);
    } else if (shop_cursor == W_SWORD) {
        s32 lv = sword_lv;
        if ((ok = buy_level(&lv, sword_price))) set_sword_lv(lv);
    } else if (shop_cursor == W_SCARECROW) {
        if ((ok = coins >= SCARECROW_PRICE)) { coins -= SCARECROW_PRICE; scarecrow_bag++; }
    } else {
        ok = buy_level(&scarecrow_lv, scarecrow_price);
    }
    if (!ok) { play(SND_NOSEEDS); return; }
    play(shop_page == PAGE_SEEDS ? SND_PLANT : SND_HARVEST);
    draw_hud();
    draw_shop();
}

// ---------- run: start and game over ----------
static s32 game_over;

static void new_run(void) {
    reset_farm();
    for (s32 i = 0; i < MAX_CROWS; i++) crow_state[i] = CROW_NONE;
    for (s32 r = 1; r < 20; r++) clear_row(r);
    msg_timer = 0;
    set_light(0, 0);
    px = PLAYER_START_X * 16 * 16;
    py = PLAYER_START_Y * 16 * 16;
    pdir = DIR_UP;
    pvx = pvy = hurt_timer = ko_timer = swing = weapon_out = 0;
    for (s32 k = 0; k < KINDS; k++) seeds[k] = crops[k] = 0;
    day = 1; seeds[CORN] = 3; coins = 0; hearts = max_hearts = 3;   // DESIGN.md first guesses
    sel_seed = CORN;
    sickle_lv = 1;
    set_sword_lv(1);
    scarecrows = scarecrow_bag = sc_warned = 0;
    scarecrow_lv = 1;
    for (s32 k = 0; k < MAX_SHOTS; k++) shot_life[k] = 0;
    spawning = 1; shop_open = 0;
    game_over = 0;
    start_day();
}

// Lose: no crops in the field, no seeds, and not enough coins for a seed (DESIGN.md). Crops in the bag
// count as the coins they sell for at dusk.
static void check_game_over(void) {
    if (seeds_total() > 0 || coins + crops_value() >= seed_price[CORN] || field_has_crops()) return;
    char buf[24];
    fmt_num(buf + copy_str(buf, "YOU REACHED DAY "), day);
    game_over = 1;
    msg_timer = 0;
    for (s32 r = 7; r <= 13; r++) clear_row(r);
    put_text(7, 8, "THE FARM IS LOST");
    put_text(7, 10, buf);
    put_text(9, 12, "PRESS START");
}

// ---------- main ----------
int main(void) {
    REG_DISPCNT = 0x0080;   // forced blank while loading

    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = bg_pal[i];
        PAL_OBJ[i] = obj_pal[i];
        PAL_OBJ[16 + i] = 0x7FFF;   // bank 1: hit flash, all white
        PAL_OBJ[32 + i] = obj_pal[i];   // bank 2: Lv.2 crow, crow colors tinted red below
    }
    PAL_OBJ[32 + 1] = (9) | (3 << 5) | (3 << 10);      // Lv.2 crow body: dark red-black
    PAL_OBJ[32 + 9] = (17) | (5 << 5) | (5 << 10);     // wing: deep red
    PAL_OBJ[32 + 10] = (24) | (9 << 5) | (8 << 10);    // sheen: red
    for (u32 i = 0; i < 16; i++) { PAL_OBJ[48 + i] = obj_pal[i]; PAL_OBJ[64 + i] = obj_pal[i]; }
    PAL_OBJ[48 + 1] = (7) | (2 << 5) | (10 << 10);     // Lv.3 crow body: dark purple
    PAL_OBJ[48 + 9] = (13) | (5 << 5) | (19 << 10);    // wing: purple
    PAL_OBJ[48 + 10] = (21) | (11 << 5) | (27 << 10);  // sheen: lilac
    PAL_OBJ[64 + 1] = (14) | (10 << 5) | (2 << 10);    // gold crow body: dark gold
    PAL_OBJ[64 + 9] = (26) | (20 << 5) | (4 << 10);    // wing: gold
    PAL_OBJ[64 + 10] = (31) | (28 << 5) | (12 << 10);  // sheen: bright gold
    for (u32 i = 0; i < 16; i++) PAL_OBJ[80 + i] = obj_pal[i];
    PAL_OBJ[80 + 13] = (31) | (31 << 5) | (20 << 10);  // firefly core: pale yellow
    PAL_OBJ[80 + 14] = (18) | (28 << 5) | (8 << 10);   // firefly glow: yellow-green
    PAL_BG[16 + 1] = (29) | (28 << 5) | (22 << 10);   // text: cream
    PAL_BG[16 + 2] = (3) | (4 << 5) | (6 << 10);      // text background: dark slate
    PAL_BG[16 + 3] = (28) | (8 << 5) | (6 << 10);     // heart red
    PAL_BG[16 + 4] = (29) | (25 << 5) | (10 << 10);   // coin gold
    PAL_BG[16 + 5] = (13) | (21 << 5) | (9 << 10);    // crop green
    PAL_BG[16 + 6] = (29) | (16 << 5) | (5 << 10);    // orange (carrot and pumpkin seed icons)
    copy32(CHARBLOCK(0), terrain_tiles, sizeof(terrain_tiles) / 4);
    copy32(CHARBLOCK(1), font_tiles, sizeof(font_tiles) / 4);
    copy32(OBJ_TILES, sprite_tiles, sizeof(sprite_tiles) / 4);

    for (u32 i = 0; i < 128; i++) oam_shadow[i * 4] = 0x0200;
    oam_flush();

    for (u32 i = 0; i < 32 * 32; i++) SCREENBLOCK(TEXT_SB)[i] = 0;
    REG_BG0CNT = 0 | (1 << 2) | (TEXT_SB << 8);   // text: priority 0
    REG_BG1CNT = 2 | (0 << 2) | (MAP_SB << 8);    // map: priority 2, 32x32 tiles
    sound_init();
    new_run();

    REG_DISPCNT = 0x0000 | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);   // mode 0, 1D OBJ, BG0, BG1, OBJ

    for (;;) {
        vsync();
        sound_vblank();
        oam_flush();
        sound_mix();

        poll_keys();
        frame++;
        rng_state += keys_now;
        (void)rng;

        if (game_over) {
            if (HIT(KEY_START)) new_run();
            else if (((frame >> 5) & 1) != ((frame - 1) >> 5 & 1))   // blink PRESS START
                put_text(9, 12, (frame >> 5) & 1 ? "           " : "PRESS START");
            continue;
        }
        if (shop_open) {
            update_shop();
            oam_begin();   // only the fireflies; they sit behind the window
            if (shop_open) draw_fireflies();
            oam_end();
            continue;
        }
        update_farmer();
        update_target();
        farm_work();
        seed_work();
        scarecrow_work();
        update_sword();
        update_crows();
        update_scarecrows();
        update_crops();
        update_message();
        check_game_over();

        oam_begin();
        if (pdir != DIR_UP) draw_sword();   // in front of the farmer
        draw_farmer();
        if (pdir == DIR_UP) draw_sword();   // behind him when he faces away
        draw_crows();
        draw_shots();
        draw_marker();
        oam_end();
    }
}
