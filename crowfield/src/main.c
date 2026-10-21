// Crowfield (GBA), v0.1.0: first release (farming, sword combat, sound clips).
// One-screen farm. Crops grow on a timer. The farmer plants, harvests, and fights crows with a sword.
// Sparring crows fly at the farmer and peck him. At 0 hearts he is knocked out for 3 seconds.
// D-pad: move   R: run   A: plant on empty soil / harvest a ripe crop (puts the sword away)
// B: draw and swing the sword (it stays in hand until A)
// Later: crows that eat crops and get stronger, waves, days, shop, scarecrow turrets.

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
       FENCE_V, FENCE_TL, FENCE_TR, FENCE_BL, FENCE_BR, CROP_YOUNG };   // FENCE_V.. are looks only; the map stores FENCE
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };
// Sprite cells in sprites.png (4 tiles each in OBJ VRAM)
enum { SPR_FARMER_DOWN, SPR_FARMER_UP, SPR_FARMER_SIDE, SPR_CROW_A, SPR_CROW_B, SPR_MARKER,
       SPR_SWORD_RIGHT, SPR_SWORD_UP, SPR_PUFF_BIG, SPR_PUFF_SMALL, SPR_FARMER_KO, SPR_SWORD_DIAG };
// put_sprite flags
#define HFLIP 1
#define VFLIP 2
#define FLASH 0x10   // OBJ palette bank 1: all white

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
enum { SND_SWING, SND_HIT, SND_CAW, SND_HURT, SND_PLANT, SND_HARVEST, SND_NOSEEDS };   // order of SOUNDS in make_sound.py
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
static const signed short sin64[64] __attribute__((unused)) = {
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
// 16x16 sprite at screen (x, y), cell from sprites.png. flags: HFLIP, VFLIP, FLASH.
static void put_sprite(s32 x, s32 y, u32 cell, u32 flags) {
    if (oam_used >= 128) return;
    u16 *o = &oam_shadow[oam_used++ * 4];
    o[0] = (y & 0xFF);                                            // square shape
    o[1] = (x & 0x1FF) | (1 << 14) | ((flags & 3) << 12);         // 16x16, flips
    o[2] = cell * 4 | ((flags >> 4) << 12);                       // tile index, priority 0, palette bank
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
static s32 seeds, crops, coins, day;
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

static s32 swing;        // > 0: sword swing frames left

static void farm_work(void) {
    if (!HIT(KEY_A) || ko_timer || swing) return;
    weapon_out = 0;
    u8 t = tile_at(tx, ty);
    if (t == CROP_RIPE) {
        set_tile(tx, ty, SOIL);
        crops++;
        play(SND_HARVEST);
        draw_hud();
    } else if (t == SOIL) {
        if (seeds <= 0) { show_message("NO SEEDS!"); play(SND_NOSEEDS); return; }
        seeds--;
        grow[ty * MAP_W + tx] = 0;
        set_tile(tx, ty, SPROUT);
        play(SND_PLANT);
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

static u32 frame;   // game loop counter; the test harness steps by it

static void draw_farmer(void) {
    if (ko_timer) { put_sprite(px >> 4, py >> 4, SPR_FARMER_KO, 0); return; }
    if (hurt_timer && (frame & 4)) return;   // blink while hurt
    u32 cell = pdir == DIR_DOWN ? SPR_FARMER_DOWN : pdir == DIR_UP ? SPR_FARMER_UP : SPR_FARMER_SIDE;
    s32 bob = (pstep >> 3) & 1;   // 1 px walk bob
    put_sprite(px >> 4, (py >> 4) - bob, cell, pdir == DIR_LEFT ? HFLIP : 0);
}

// ---------- crows ----------
// v0.1 crows are sparring partners: they fly at the farmer and peck him.
// v0.2 adds crop eating, growing stronger, and waves.
#define MAX_CROWS 24
enum { CROW_NONE, CROW_FLY, CROW_HURT, CROW_DEAD };
// Separate arrays (not structs) so tests can poke $crow_hp+4 etc. Positions in 1/16 px, sprite top-left.
static s32 crow_state[MAX_CROWS], crow_x[MAX_CROWS], crow_y[MAX_CROWS], crow_hp[MAX_CROWS];
static s32 crow_vx[MAX_CROWS], crow_vy[MAX_CROWS], crow_timer[MAX_CROWS];
static s32 crow_flash[MAX_CROWS], crow_cool[MAX_CROWS], crow_hit_by[MAX_CROWS];
static s32 sparring;       // how many crows to keep alive (tests set 0 and place crows by hand)
static s32 spawn_timer;
#define CROW_SPEED 10      // 1/16 px per frame
#define CROW_HP 1          // Normal crow: 1 hit (DESIGN.md)
#define PECK_COOL 90

static s32 abs32(s32 v) { return v < 0 ? -v : v; }

static void spawn_crow(void) {
    for (s32 i = 0; i < MAX_CROWS; i++) {
        if (crow_state[i] != CROW_NONE) continue;
        u32 side = rng() & 3, along = rng();
        s32 x = side == 0 ? -16 : side == 1 ? 240 : 16 + along % 208;
        s32 y = side == 2 ? -16 : side == 3 ? 160 : 24 + along % 112;
        crow_x[i] = x * 16; crow_y[i] = y * 16;
        crow_state[i] = CROW_FLY;
        crow_hp[i] = CROW_HP;
        crow_flash[i] = 0; crow_cool[i] = 60; crow_vx[i] = crow_vy[i] = 0;
        return;
    }
}

static void hurt_farmer(s32 from_x, s32 from_y) {
    if (hurt_timer || ko_timer) return;
    hearts--;
    s32 dx = px - from_x, dy = py - from_y;
    pvx = dx > 0 ? 48 : dx < 0 ? -48 : 0;
    pvy = dy > 0 ? 48 : dy < 0 ? -48 : 0;
    play(SND_HURT);
    if (hearts <= 0) { hearts = 0; ko_timer = KO_FRAMES; show_message("KNOCKED OUT!"); }
    else hurt_timer = HURT_FRAMES;
    draw_hud();
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
        // Fly toward the farmer. Hover in place while he is knocked out.
        s32 dx = px - crow_x[i], dy = py - crow_y[i];
        if (ko_timer) continue;
        s32 ax = abs32(dx), ay = abs32(dy);
        s32 sx = ax >= ay / 2 ? CROW_SPEED : 0, sy = ay >= ax / 2 ? CROW_SPEED : 0;
        if (ax > sx) crow_x[i] += dx > 0 ? sx : -sx; else crow_x[i] = px;
        if (ay > sy) crow_y[i] += dy > 0 ? sy : -sy; else crow_y[i] = py;
        crow_vx[i] = dx;   // remembered for facing
        if (ax < 10 * 16 && ay < 10 * 16 && !crow_cool[i]) {   // close enough: peck
            hurt_farmer(crow_x[i], crow_y[i]);
            crow_cool[i] = PECK_COOL;
            crow_state[i] = CROW_HURT;   // bounce back after the peck
            crow_timer[i] = 12;
            crow_vx[i] = dx > 0 ? -40 : 40;
            crow_vy[i] = dy > 0 ? -40 : 40;
        }
    }
    if (alive < sparring && ++spawn_timer >= 90) { spawn_timer = 0; spawn_crow(); }
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
        u32 flags = (crow_vx[i] < 0 ? HFLIP : 0) | (crow_flash[i] ? FLASH : 0);
        put_sprite(x, y, ((frame + i * 3) >> 3) & 1 ? SPR_CROW_A : SPR_CROW_B, flags);
    }
}

// ---------- sword ----------
#define SWING_FRAMES 12
static s32 swing_cool, swing_id;
static s32 sword_damage = 1, sword_reach = 14;   // Lv.1; the shop raises these later
static s32 slash_x, slash_y;                     // center of the hit box, screen px

static void update_sword(void) {
    if (swing_cool) swing_cool--;
    if (!swing && HIT(KEY_B) && !swing_cool && !ko_timer) {
        swing = SWING_FRAMES;
        weapon_out = 1;
        swing_id++;
        play(SND_SWING);
    }
    if (!swing) return;
    s32 cx = (px >> 4) + 8, cy = (py >> 4) + 8;
    slash_x = cx + (pdir == DIR_RIGHT ? sword_reach : pdir == DIR_LEFT ? -sword_reach : 0);
    slash_y = cy + (pdir == DIR_DOWN ? sword_reach : pdir == DIR_UP ? -sword_reach : 0);
    for (s32 i = 0; i < MAX_CROWS; i++) {
        if (crow_state[i] != CROW_FLY && crow_state[i] != CROW_HURT) continue;
        if (crow_hit_by[i] == swing_id) continue;   // one hit per swing
        s32 ex = (crow_x[i] >> 4) + 8 - slash_x, ey = (crow_y[i] >> 4) + 8 - slash_y;
        if (abs32(ex) > 14 || abs32(ey) > 14) continue;
        crow_hit_by[i] = swing_id;
        crow_hp[i] -= sword_damage;
        crow_flash[i] = 8;
        if (crow_hp[i] <= 0) {
            crow_state[i] = CROW_DEAD;
            crow_timer[i] = 16;
            play(SND_CAW);
            continue;
        }
        crow_state[i] = CROW_HURT;   // knockback away from the farmer
        crow_timer[i] = 14;
        crow_vx[i] = pdir == DIR_RIGHT ? 64 : pdir == DIR_LEFT ? -64 : 0;
        crow_vy[i] = pdir == DIR_DOWN ? 64 : pdir == DIR_UP ? -64 : 0;
        play(SND_HIT);
    }
    if (--swing == 0) swing_cool = 6;
}

// Sword poses: which cell and flips point the blade each way.
enum { P_U, P_D, P_L, P_R, P_UR, P_UL, P_DR, P_DL };
static const u8 pose_cell[8] = { SPR_SWORD_UP, SPR_SWORD_UP, SPR_SWORD_RIGHT, SPR_SWORD_RIGHT,
                                 SPR_SWORD_DIAG, SPR_SWORD_DIAG, SPR_SWORD_DIAG, SPR_SWORD_DIAG };
static const u8 pose_flags[8] = { 0, VFLIP, HFLIP, 0, 0, HFLIP, VFLIP, HFLIP | VFLIP };
// Where the hand grips each unflipped cell (px in the 16x16 cell): right, up, up-right.
static s32 grip_x(u32 cell) { return cell == SPR_SWORD_RIGHT ? 2 : cell == SPR_SWORD_UP ? 8 : 3; }
static s32 grip_y(u32 cell) { return cell == SPR_SWORD_RIGHT ? 8 : cell == SPR_SWORD_UP ? 13 : 12; }
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

// ---------- main ----------
int main(void) {
    REG_DISPCNT = 0x0080;   // forced blank while loading

    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = bg_pal[i];
        PAL_OBJ[i] = obj_pal[i];
        PAL_OBJ[16 + i] = 0x7FFF;   // bank 1: hit flash, all white
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
    day = 1; seeds = 3; crops = 0; coins = 0; hearts = max_hearts = 3;   // DESIGN.md first guesses
    sparring = 2;
    draw_hud();

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

        update_farmer();
        update_target();
        farm_work();
        update_sword();
        update_crows();
        update_crops();
        update_message();

        oam_begin();
        if (pdir != DIR_UP) draw_sword();   // in front of the farmer
        draw_farmer();
        if (pdir == DIR_UP) draw_sword();   // behind him when he faces away
        draw_crows();
        draw_marker();
        oam_end();
    }
}
