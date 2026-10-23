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
#include "music.h"

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
#define REG_SND2CNT   REG16(0x04000068)
#define REG_SND2FREQ  REG16(0x0400006C)
#define REG_SND3SEL   REG16(0x04000070)
#define REG_SND3CNT   REG16(0x04000072)
#define REG_SND3FREQ  REG16(0x04000074)
#define WAVE_RAM      ((volatile u16*)0x04000090)
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
       SPR_FIREFLY, SPR_FIREFLY_DIM, SPR_SPEAR_UP, SPR_SPEAR_RIGHT, SPR_SPEAR_DIAG, SPR_SCYTHE_UP,
       SPR_SCYTHE_RIGHT, SPR_SCYTHE_DIAG, SPR_SLING_UP, SPR_SLING_RIGHT, SPR_CANNON, SPR_BALL,
       SPR_IRON_UP, SPR_IRON_RIGHT, SPR_IRON_DIAG, SPR_BARREL_UP, SPR_BARREL_DIAG, SPR_BARREL_RIGHT };
// put_sprite flags
#define HFLIP 1
#define VFLIP 2
#define FLASH 0x10   // OBJ palette bank 1: all white
#define BANK(n) ((n) << 4)   // OBJ palette bank n: 2 red crow, 3 purple crow, 4 gold crow, 5 fireflies, 6 white crow,
                             // 7 steel (iron sword), 8 stone (slingshot shot)

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
    REG_SNDCNT_L = 0x6677;   // tone channels 2 (melody) and 3 (bass) on both sides, full volume (v0.9 music)
    REG_SNDCNT_H = 0x0B04 | 0x0002;   // DMA A 100%, left + right, timer 0, FIFO reset; PSG 100%
    REG_TM0CNT_L = (u16)(65536 - 1254);   // 1254 cycles per sample = 280896 per frame / MIX_LEN
    REG_TM0CNT_H = 0x0080;
    REG_SND3SEL = 0x0040;    // the bass wave goes into the bank that is not playing (bank 1 selected)
    for (u32 i = 0; i < 8; i++) WAVE_RAM[i] = bass_wave[2 * i] | (bass_wave[2 * i + 1] << 8);
    REG_SND3SEL = 0x0000;
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

// ---------- music (v0.9): melody on tone channel 2, bass on channel 3; the clips keep DirectSound A ----------
// Five songs (Caleb): title, day, dusk shop, blood moon, final nights; tools/make_music.py writes them.
static s32 song = -1;
static u32 voice_pos[2], voice_left[2];   // note index and frames left, per voice
static void music_note(s32 v, u16 freq) {
    if (v == 0) {
        if (!freq) { REG_SND2CNT = 0; REG_SND2FREQ = 0x8000; return; }   // rest: silence
        REG_SND2CNT = (9 << 12) | (5 << 8) | (2 << 6);                 // a pluck that fades, 50% duty
        REG_SND2FREQ = freq | 0x8000;
    } else {
        if (!freq) { REG_SND3SEL = 0; return; }
        REG_SND3SEL = 0x0080;                                          // on, 32-sample bank 0
        REG_SND3CNT = 2 << 13;                                         // 50% volume
        REG_SND3FREQ = freq | 0x8000;
    }
}
static void music_play(s32 s) {   // -1: silence. The same song keeps playing.
    if (s == song) return;
    song = s;
    for (s32 v = 0; v < 2; v++) { voice_pos[v] = 0; voice_left[v] = 0; music_note(v, 0); }
}
static void music_tick(void) {
    if (song < 0) return;
    for (s32 v = 0; v < 2; v++) {
        if (voice_left[v]) { voice_left[v]--; continue; }
        u32 i = voice_pos[v];
        music_note(v, song_freq[song][v][i]);
        voice_left[v] = song_len[song][v][i] * song_fps[song] - 1;
        voice_pos[v] = i + 1 == song_count[song][v] ? 0 : i + 1;
    }
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
// 16x16 sprite at screen (x, y), cell from sprites.png. flags: HFLIP, VFLIP, FLASH, BANK(n).
// Every sprite has priority 1, behind the text layer (BG0, priority 0): messages, notices, the HUD, and the
// shop window always cover the farmer, the target marker, and crows (Caleb). The map (BG1) is priority 2.
static void put_sprite(s32 x, s32 y, u32 cell, u32 flags) {
    if (oam_used >= 128) return;
    u16 *o = &oam_shadow[oam_used++ * 4];
    o[0] = (y & 0xFF);                                            // square shape
    o[1] = (x & 0x1FF) | (1 << 14) | ((flags & 3) << 12);         // 16x16, flips
    o[2] = cell * 4 | (((flags >> 4) & 15) << 12) | (1 << 10);   // tile, priority 1, palette bank
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
static const s32 sell_price[KINDS] = { 3, 7, 12, 20 };   // v0.8 money boost (Caleb); v0.5 was 2, 5, 8, 14
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

static s32 blood_night(void);
static void update_crops(void) {
    if (blood_night()) return;   // nothing grows during a blood moon
    for (s32 i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = farm[i];
        if (t != SPROUT && t != CROP_YOUNG && !is_grown(t)) continue;
        if (++grow[i] < grow_frames[crop_kind[i]]) continue;
        grow[i] = 0;
        set_tile(i % MAP_W, i / MAP_W, next_stage(i));
    }
}

static s32 farm_step;   // the field: 0 7x3, 1 7x4, 2 9x4, 3 9x5 (the expansion section)
static void reset_farm(void) {
    for (u32 i = 0; i < MAP_W * MAP_H; i++) { farm[i] = farm_start[i]; grow[i] = 0; crop_kind[i] = CORN; }
    farm_step = 0;
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
static s32 bless_attack, bless_scare;    // dawn blessings taken (v0.8): each is +0.5 damage (one half-point)
static s32 bless_pending;                // 1: the dawn shop opens on CHOOSE A BLESSING
#define MAX_CANNONS 2
static s32 cannons, can_ammo[MAX_CANNONS], can_cool[MAX_CANNONS];   // crow cannons (v0.8.1): pumpkins loaded
// Barrel pose 0 left, 1 up-left, 2 up, 3 up-right, 4 right (v0.8.2): it turns one step at a time toward the crow
// it wants (can_want) and fires only once it points there; can_kick counts down the recoil after a shot.
static s32 can_pose[MAX_CANNONS], can_want[MAX_CANNONS], can_kick[MAX_CANNONS];
static s32 hurt_timer;           // > 0: just hit, cannot be hit again (blinks)
static s32 ko_timer;             // > 0: knocked out, cannot act
static s32 pvx, pvy;             // knockback velocity, 1/16 px per frame
static s32 weapon_out;           // sword in hand: B draws it, A puts it away
#define HURT_FRAMES 60
#define KO_FRAMES 180            // 3 seconds (Caleb's call)
#define BLOOD_KO_FRAMES 360      // 6 s during a blood moon: "you don't get up as fast" (Caleb)
#define BLOOD_KOS 3              // the 3rd knockout of a blood moon loses the night (Caleb)
#define FINALE_KOS 5             // the final blood moons allow 5 knockouts (the 5th loses)
static s32 finale;               // 0, or which final blood moon (1-3); see the finale section
static s32 blood_on;             // 1: blood-moon night, 2: dawn (crows fly away, then the shop)
static s32 blood_kos;            // knockouts this blood moon

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
static s32 seeds[KINDS], crops[KINDS], coins, day;
static s32 stat_crops, stat_kills;   // crops harvested and crows defeated this run (the end screens)   // seeds and crops in the bag, per crop ($seeds = corn)
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
static int cannon_work(void);
static void farm_work(void) {
    if (!HIT(KEY_A) || ko_timer || swing) return;
    weapon_out = 0;
    if (cannon_work()) return;   // next to a crow cannon: load it
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
            stat_crops++;
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
enum { CROW_NONE, CROW_FLY, CROW_HURT, CROW_DEAD, CROW_EAT, CROW_FLEE };   // FLEE: flies away at dawn
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
// HP is counted in half-points so the Lv.2 sword can do 1.5 (Caleb): black 1, red 2, purple 3, gold 5 (v0.5.1).
#define HP(n) ((n) * 2)
// Lv.5 is the blood-moon gold crow (v0.6): gold, 20% faster, drops 10 coins. Lv.6 is the white crow of the
// final blood moons (v0.7): the hardest, 8 HP, so a Lv.3 sword needs 4 hits (Caleb).
static const s32 crow_hp_of[7] = { HP(1), HP(1), HP(2), HP(3), HP(5), HP(5), HP(8) };
static const s32 crow_speed_of[7] = { 6, 6, 7, 8, 9, 11, 11 };          // +15%, +30%, +45% (docs/V0.5_PLAN.md)
static const s32 crow_bite_of[7] = { 120, 120, 120, 90, 60, 60, 60 };
#define CROW_GOLD 4
#define CROW_BLOOD 5
#define CROW_WHITE 6
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

static int spawn_crow_lv(s32 lv);
static int spawn_crow(void) {   // today's next crow; 0 if all MAX_CROWS slots are busy (the crow waits)
    return spawn_crow_lv(day_queue[day_left > 0 ? day_left - 1 : 0]);
}
static int spawn_crow_lv(s32 lv) {
    for (s32 i = 0; i < MAX_CROWS; i++) {
        if (crow_state[i] != CROW_NONE) continue;
        u32 side = rng() & 3, along = rng();
        s32 x = side == 0 ? -16 : side == 1 ? 240 : 16 + along % 208;
        s32 y = side == 2 ? -16 : side == 3 ? 160 : 24 + along % 112;
        crow_x[i] = x * 16; crow_y[i] = y * 16;
        crow_state[i] = CROW_FLY;
        crow_lv[i] = lv < 1 ? 1 : lv;
        crow_hp[i] = crow_hp_of[crow_lv[i]];
        crow_ate[i] = 0;
        crow_flash[i] = 0; crow_cool[i] = 60; crow_vx[i] = crow_vy[i] = 0;
        crow_target[i] = pick_target(i);
        play(SND_CAW);
        return 1;
    }
    return 0;
}

static s32 dev_god, dev_money;   // dev mode: invincible, unlimited money (see the dev mode section)
static void hurt_farmer(s32 from_x, s32 from_y) {
    if (hurt_timer || ko_timer || dev_god) return;
    hearts--;
    s32 dx = px - from_x, dy = py - from_y;
    pvx = dx > 0 ? 48 : dx < 0 ? -48 : 0;
    pvy = dy > 0 ? 48 : dy < 0 ? -48 : 0;
    play(SND_HURT);
    if (hearts <= 0) {
        hearts = 0;
        ko_timer = blood_on == 1 ? BLOOD_KO_FRAMES : KO_FRAMES;
        play(SND_KNOCKDOWN);
        if (blood_on == 1) {
            char buf[20];
            s32 n = copy_str(buf, "KNOCKOUT ");
            n += fmt_num(buf + n, ++blood_kos);
            n += copy_str(buf + n, " OF ");
            fmt_num(buf + n, finale ? FINALE_KOS : BLOOD_KOS);
            show_message(buf);
        } else show_message("KNOCKED OUT!");
    }
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
    if (blood_on == 1) v = v * 6 / 5;   // every crow is 20% faster in a blood moon
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

static u16 mix(u16 a, u16 b, s32 t) {   // a to b, t 0..256
    s32 ra = a & 31, ga = (a >> 5) & 31, ba = (a >> 10) & 31;
    s32 rb = b & 31, gb = (b >> 5) & 31, bb = (b >> 10) & 31;
    return (u16)((ra + ((rb - ra) * t >> 8)) | ((ga + ((gb - ga) * t >> 8)) << 5) | ((ba + ((bb - ba) * t >> 8)) << 10));
}
static u16 blood_of(u16 c) {   // the blood-moon night: dark, red
    s32 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    r = (r * 200 >> 8) + 3;
    g = g * 60 >> 8;
    b = b * 70 >> 8;
    return (u16)((r > 31 ? 31 : r) | (g << 5) | (b << 10));
}
static u16 dawn_of(u16 c) {    // dawn: soft pink morning light
    return mix(c, (u16)(31 | (17 << 5) | (21 << 10)), 100);
}
// Blood red: t 0..256 of the way from day light (the red sunset before a blood moon ends at about 160).
static void set_light_blood(s32 t) {
    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = mix(bg_pal[i], blood_of(bg_pal[i]), t);
        PAL_OBJ[i] = mix(obj_pal[i], blood_of(obj_pal[i]), t >> 1);
    }
}
static void set_light_dawn(void) {
    for (u32 i = 0; i < 16; i++) { PAL_BG[i] = dawn_of(bg_pal[i]); PAL_OBJ[i] = obj_pal[i]; }
}

#define FIREFLIES 9
static const u8 fly_x[FIREFLIES] = { 6, 8, 224, 226, 220, 52, 150, 108, 190 };   // around the shop window
static const u8 fly_y[FIREFLIES] = { 36, 96, 28, 88, 128, 138, 142, 6, 10 };
static void draw_fireflies(void) {
    for (s32 k = 0; k < FIREFLIES; k++) {
        u32 t = frame + k * 37;
        s32 x = fly_x[k] - 8 + (sin64[(t >> 3) & 63] * 6 >> 8);         // drift slowly
        s32 y = fly_y[k] - 8 + (sin64[((t >> 2) + k * 9) & 63] * 4 >> 8);
        u32 phase = (t >> 1) & 63;                                     // blink: bright, dim, dark
        if (phase < 28) put_sprite(x, y, SPR_FIREFLY, BANK(5));
        else if (phase < 44) put_sprite(x, y, SPR_FIREFLY_DIM, BANK(5));
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

// ---------- blood moon (v0.6, Caleb; docs/V0.6_PLAN.md) ----------
// The night after Day 34, then every 4-6 nights: no dusk shop. Gold crows (Lv.5) pour in until dawn
// (30 s; the final nights 45 s). Knockouts last 6 s; the 3rd loses the night. Scarecrows fire at Lv.1 for the night. Crops do not
// grow. Each kill pays 10 coins, and dawn pays a 25-coin bonus. Then the shop opens at dawn.
// Losing one of the first 3 blood moons only costs that night's coins and the bonus (Caleb: "softer");
// after the 3rd a notice warns that from now on a lost blood moon ends the run.
#define BLOOD_FIRST 34          // the first blood moon is the night after Day 34
#define BLOOD_FRAMES 1800       // 30 s until dawn (Caleb, v0.7.2; was 90)
#define FINALE_FRAMES 2700      // each final night: 45 s
#define DAWN_FRAMES 120         // crows fly away for 2 s, then the shop
#define DAWN_BONUS 25
static s32 blood_next;          // the day whose night is the next blood moon
static s32 blood_timer;         // frames until dawn (or until the dawn shop)
static s32 blood_saved_lv;      // the scarecrow level paid for, given back at dawn
static s32 shop_dawn;           // 1: the shop opened at dawn after a blood moon
static s32 scarecrow_lv;        // (defined with the scarecrows below)
static s32 blood_count;         // blood moons over so far (won or lost)
static s32 blood_coins;         // coins earned this blood moon (taken back if it is lost)
#define BLOOD_PRACTICE 3        // losing one of the first 3 only costs its coins
static s32 notice_timer, notice_pending;   // the "training" notice over the field after the 3rd

// Up to 4 centered lines over the field for a few seconds: two pairs (rows 6-7 and 10-11) with room between.
static const u8 notice_row[4] = { 6, 7, 10, 11 };
static void show_notice(const char *const *lines, s32 count) {
    for (s32 k = 0; k < count; k++) {
        s32 n = 0;
        while (lines[k][n]) n++;
        clear_row(notice_row[k]);
        put_text((30 - n) / 2, notice_row[k], lines[k]);
    }
    notice_timer = 300;
}
static void update_notice(void) {
    if (notice_timer && --notice_timer == 0)
        for (s32 k = 0; k < 4; k++) clear_row(notice_row[k]);
}
static const char *const training_notice[4] = {
    "NOW IS THE TIME FOR", "YOUR TRAINING TO SHOW.", "LOSE A BLOOD MOON NOW", "AND THE RUN IS OVER." };

enum { END_FARM, END_BLOOD, END_FINALE, END_WON };
static void end_screen(s32 kind);
static s32 blood_night(void) { return blood_on != 0; }

// ---------- the final blood moons (v0.7, Caleb) ----------
// After the 5th blood moon, the next dusk is the last shop: +150 coins, "your last time to buy". Then 3
// blood moons in a row with no shop, with white and gold crows. 5 knockouts are allowed (the 5th loses).
// Survive all 3 and the player wins.
#define FINALE_AFTER 5          // blood moons before the finale
#define FINALE_NIGHTS 3
#define FINALE_COINS 150
#define BREAK_FRAMES 300        // 5 s between the final nights
static s32 finale_pending;      // 1: the next dusk is the last shop
static s32 shop_last;           // 1: this is the last shop

// v0.8 (Caleb): every blood moon has an exact lineup (docs/V0.8_PLAN.md), released in spurts that fill the
// whole night: 6 spurts (9 on a final night), each a little bigger than the last, one crow every 1/4 s.
// Columns: black, red, purple, gold, white. Rows: blood moons 1-5, final nights 1-3.
static const u8 bm_lineup[8][5] = {
    { 4, 8, 7, 5, 0 }, { 2, 8, 9, 7, 0 }, { 0, 8, 10, 9, 1 }, { 0, 6, 11, 11, 2 }, { 0, 4, 12, 13, 3 },
    { 0, 0, 12, 18, 6 }, { 0, 0, 10, 20, 10 }, { 0, 0, 8, 22, 15 } };
static const u8 bm_level[5] = { 1, 2, 3, 4, CROW_WHITE };
static const u8 bm_coins[7] = { 2, 2, 4, 6, 10, 10, 15 };   // coins per kill in a blood moon, by crow level
#define BM_MAX 48
#define SPURT_GAP 15            // frames between crows inside a spurt
static u8 bm_queue[BM_MAX];     // tonight's crows (levels), shuffled
static s32 bm_total, bm_sent;   // crows tonight; crows already sent
static s32 spurts, spurt_len, spurt_timer, spurt_no;   // spurts tonight, frames each, frames left, which one

// How many crows should be out by the end of spurt k (later spurts are bigger: sizes grow 1, 2, 3, ...).
// Rounded up, so the first spurt always sends at least one crow.
static s32 spurt_target(s32 k) {
    s32 den = spurts * (spurts + 1);
    return (bm_total * (k + 1) * (k + 2) + den - 1) / den;
}

static void plan_blood_moon(void) {
    s32 row = finale ? 4 + finale : blood_count < 4 ? blood_count : 4;
    bm_total = 0;
    for (s32 t = 0; t < 5; t++)
        for (s32 k = 0; k < bm_lineup[row][t] && bm_total < BM_MAX; k++) bm_queue[bm_total++] = bm_level[t];
    for (s32 k = bm_total - 1; k > 0; k--) {   // shuffle
        s32 j = rand_in(0, k);
        u8 t = bm_queue[k];
        bm_queue[k] = bm_queue[j];
        bm_queue[j] = t;
    }
    bm_sent = 0;
    spurts = finale ? 9 : 6;
    spurt_len = blood_timer / spurts;
    spurt_timer = spurt_len;
    spurt_no = 0;
}

static void start_blood_moon(void) {
    blood_on = 1;
    blood_timer = finale ? FINALE_FRAMES : BLOOD_FRAMES;
    if (finale <= 1) blood_kos = 0;   // the final nights share their 5 knockouts
    blood_coins = 0;
    blood_saved_lv = scarecrow_lv;
    scarecrow_lv = 1;            // for this night only
    plan_blood_moon();
    spawn_timer = SPURT_GAP;
    set_light_blood(256);
    if (finale) {
        char buf[20];
        s32 n = copy_str(buf, "BLOOD MOON ");
        n += fmt_num(buf + n, finale);
        copy_str(buf + n, " OF 3");
        show_message(buf);
    } else show_message("BLOOD MOON!");
    play(SND_CAW);
}

// Dawn: every crow still here flies away, scarecrows get their level back, the sky turns pink.
static void blood_dawn(void) {
    for (s32 i = 0; i < MAX_CROWS; i++)
        if (crow_state[i] != CROW_NONE && crow_state[i] != CROW_DEAD) crow_state[i] = CROW_FLEE;
    scarecrow_lv = blood_saved_lv;
    blood_count++;
    if (blood_count == BLOOD_PRACTICE) notice_pending = 1;
    if (blood_count == FINALE_AFTER && !finale) { finale_pending = 1; blood_next = 99999; }
    blood_on = 2;
    blood_timer = DAWN_FRAMES;
    set_light_dawn();
    draw_hud();
}


static void update_blood_moon(void) {
    if (blood_on == 3) {                     // a break between the final nights
        if (--blood_timer > 0) return;
        finale++;
        start_blood_moon();
        return;
    }
    if (blood_on == 1 && blood_kos >= (finale ? FINALE_KOS : BLOOD_KOS) && !ko_timer) {   // the night is lost
        if (finale) { end_screen(END_FINALE); return; }
        if (blood_count >= BLOOD_PRACTICE) { end_screen(END_BLOOD); return; }
        coins -= blood_coins;    // practice: lose the night's coins and the bonus, keep the run
        if (coins < 0) coins = 0;
        blood_dawn();
        show_message("LOST! NO COINS TONIGHT");
        return;
    }
    if (blood_on == 1) {
        if (--blood_timer > 0) {
            if (--spurt_timer <= 0 && spurt_no < spurts - 1) { spurt_no++; spurt_timer = spurt_len; }
            if (bm_sent < spurt_target(spurt_no) && ++spawn_timer >= SPURT_GAP && spawn_crow_lv(bm_queue[bm_sent])) {
                spawn_timer = 0;
                bm_sent++;
            }
            if (blood_timer % 60 == 0 && msg_timer <= 60) {   // "DAWN IN n", unless a message is fresh
                char buf[16];
                fmt_num(buf + copy_str(buf, "DAWN IN "), blood_timer / 60);
                show_message(buf);
            }
            return;
        }
        if (finale) {                        // a final night is over
            for (s32 i = 0; i < MAX_CROWS; i++)
                if (crow_state[i] != CROW_NONE && crow_state[i] != CROW_DEAD) crow_state[i] = CROW_FLEE;
            if (finale >= FINALE_NIGHTS) { end_screen(END_WON); return; }
            blood_on = 3;
            blood_timer = BREAK_FRAMES;
            show_message("THE NEXT ONE IS COMING...");
            return;
        }
        coins += DAWN_BONUS;
        blood_dawn();
        bless_pending = 1;       // survived: the dawn shop opens on CHOOSE A BLESSING
        show_message("DAWN! +25 COINS");
        return;
    }
    if (--blood_timer > 0) return;
    blood_on = 0;
    if (!finale_pending) blood_next = day + rand_in(4, 6);   // 4 to 6 nights later (Caleb); none before the finale
    shop_dawn = 1;
    open_shop();
}

// A day ends when all of its crows have come and gone. Then dusk: the shop.
static void update_days(s32 alive) {
    if (!spawning) return;
    if (blood_on) { update_blood_moon(); return; }
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
    s32 blood = day == blood_next;
    if (--day_done > 0) {
        if ((day_done & 3) == 0) {            // the sun goes down; red before a blood moon
            s32 t = DAY_DONE_FRAMES - day_done;
            if (blood) set_light_blood(t * 160 / DAY_DONE_FRAMES);
            else set_light(t * SUNSET_MAX / DAY_DONE_FRAMES, 0);
        }
        if (day_done % 60 == 0) {             // once a second: "DUSK IN n" (or "BLOOD MOON IN n")
            char buf[24];   // "BLOOD MOON TOMORROW!" is 20 characters plus the end
            s32 sec = day_done / 60;
            if (day == blood_next - 1 && sec >= 10 && sec <= 12) copy_str(buf, "BLOOD MOON TOMORROW!");
            else if (finale_pending && sec >= 10 && sec <= 12) copy_str(buf, "THE FINAL NIGHTS COME");
            else fmt_num(buf + copy_str(buf, blood ? "BLOOD MOON IN " : "DUSK IN "), sec);
            show_message(buf);
        }
        return;
    }
    if (blood) start_blood_moon();
    else {
        if (finale_pending) {                  // the last shop before the final blood moons
            finale_pending = 0;
            shop_last = 1;
            coins += FINALE_COINS;
        }
        open_shop();
    }
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
        if (st == CROW_FLEE) {   // dawn: up and off the screen
            crow_y[i] -= 40;
            if (crow_y[i] < -32 * 16) crow_state[i] = CROW_NONE;
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
        stat_kills++;
        play(SND_POOF);
        if (blood_on == 1 || crow_lv[i] >= CROW_GOLD) {   // blood-moon crows pay by type; gold pays by day
            s32 c = blood_on == 1 ? bm_coins[crow_lv[i]] : GOLD_COINS;
            char buf[12];
            coins += c;
            if (blood_on == 1) blood_coins += c;
            s32 n = copy_str(buf, "+");
            n += fmt_num(buf + n, c);
            copy_str(buf + n, " COINS");
            show_message(buf);
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
#define SHOT_DAMAGE HP(1)
static s32 sc_x[MAX_SCARECROWS], sc_y[MAX_SCARECROWS], sc_cool[MAX_SCARECROWS];   // tile, frames to next shot
static s32 scarecrows;                   // placed (scarecrow_bag: bought, not placed)
static s32 scarecrow_lv = 1;             // 1. 2: shoots faster. 3: wider range, 2 shots
static s32 shot_x[MAX_SHOTS], shot_y[MAX_SHOTS], shot_vx[MAX_SHOTS], shot_vy[MAX_SHOTS], shot_life[MAX_SHOTS];
static s32 shot_dmg[MAX_SHOTS], shot_stone[MAX_SHOTS];   // damage (half-points); 1: a slingshot stone
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
        shot_dmg[k] = SHOT_DAMAGE + bless_scare;   // +0.5 a blessing
        shot_stone[k] = 0;
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
            if (c2 < 0 && crow_hp[c] > SHOT_DAMAGE + bless_scare) c2 = c;   // needs 2 hits; never waste a seed
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
            hit_crow(i, shot_dmg[k], shot_vx[k], shot_vy[k]);
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
        if (shot_life[k]) put_sprite((shot_x[k] >> 4) - 8, (shot_y[k] >> 4) - 8, SPR_SHOT, shot_stone[k] ? BANK(8) : 0);
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
        u32 bank = crow_lv[i] == CROW_WHITE ? 6 : crow_lv[i] >= CROW_GOLD ? 4 : crow_lv[i];   // blood gold: gold colors
        u32 flags = (crow_vx[i] < 0 ? HFLIP : 0) | (crow_flash[i] ? FLASH : bank >= 2 ? BANK(bank) : 0);
        if (st == CROW_EAT) {    // wings folded; dips its head for the last few frames before a bite
            put_sprite(x, y + (crow_timer[i] < 10), SPR_CROW_B, flags);
            continue;
        }
        put_sprite(x, y, ((frame + i * 3) >> 3) & 1 ? SPR_CROW_A : SPR_CROW_B, flags);
    }
}

// ---------- weapons (v0.8.1, Caleb; docs/V0.8_PLAN.md) ----------
// The farmer holds one weapon. Buying another replaces it; each weapon keeps its own level, so switching back
// brings it back at the level it had. Damage is in half-points (plus +0.5 per ATTACK UP blessing).
// sword_lv / sword_damage / sword_reach are the held weapon's level, damage, and hit-box distance.
enum { WOOD, IRON, SPEAR, SCYTHE, SLING, WEAPONS };
static const char *const weapon_name[WEAPONS] = { "WOOD SWORD", "IRON SWORD", "SPEAR", "SCYTHE", "SLINGSHOT" };
static const s32 weapon_buy[WEAPONS] = { 0, 100, 80, 110, 120 };
static const s32 weapon_up[WEAPONS][2] = { { 40, 80 }, { 70, 140 }, { 60, 120 }, { 80, 160 }, { 80, 160 } };
static const u8 weapon_dmg[WEAPONS][3] = { { 2, 3, 4 }, { 6, 7, 8 }, { 4, 5, 6 }, { 3, 4, 5 }, { 4, 5, 6 } };
static const u8 weapon_swing[WEAPONS] = { 12, 18, 10, 18, 8 };   // frames a swing (or a shot) lasts
// The hit box: its center this far ahead of the farmer, and half its size along and across his facing.
static const u8 weapon_fwd[WEAPONS] = { 10, 14, 22, 8, 0 };      // wood Lv.3 reaches 16; iron 14 (v0.8.4, Caleb)
static const u8 weapon_along[WEAPONS] = { 14, 14, 14, 12, 0 };
static const u8 weapon_side[WEAPONS] = { 14, 14, 6, 32, 0 };     // spear narrow, scythe wide (32 since v0.8.4, Caleb)
#define SLING_RELOAD 22         // + the 8-frame shot: one stone every 0.5 s
#define STONE_SPEED 64          // 4 px a frame
#define STONE_LIFE 60
static s32 weapon;              // the weapon in hand
static s32 weapon_lv[WEAPONS];  // each weapon's level; 0 = never had it
static s32 swing_len = 12;

static s32 swing_cool, swing_id;
static s32 sword_lv = 1;                         // the held weapon's level (1-3)
static s32 sword_damage = HP(1), sword_reach = 10;   // set by set_sword_lv
static void set_sword_lv(s32 lv) {
    sword_lv = lv;
    weapon_lv[weapon] = lv;
    sword_damage = weapon_dmg[weapon][lv - 1] + bless_attack;   // wood: 1, 1.5, 2 (Caleb); +0.5 a blessing
    sword_reach = weapon == WOOD && lv >= 3 ? 16 : weapon_fwd[weapon];
    swing_len = weapon_swing[weapon];
}
static void take_weapon(s32 w) {   // switch weapons; a weapon you had before comes back at its level
    weapon = w;
    set_sword_lv(weapon_lv[w] ? weapon_lv[w] : 1);
}

static const signed char hand_x[4], hand_y[4];
static void fire_stone(void) {   // the slingshot: a stone straight ahead, out of its fork (6 px above the hand)
    for (s32 k = 0; k < MAX_SHOTS; k++) {
        if (shot_life[k]) continue;
        shot_x[k] = ((px >> 4) + hand_x[pdir]) * 16;
        shot_y[k] = ((py >> 4) + hand_y[pdir] - 6) * 16;
        shot_vx[k] = pdir == DIR_RIGHT ? STONE_SPEED : pdir == DIR_LEFT ? -STONE_SPEED : 0;
        shot_vy[k] = pdir == DIR_DOWN ? STONE_SPEED : pdir == DIR_UP ? -STONE_SPEED : 0;
        shot_life[k] = STONE_LIFE;
        shot_dmg[k] = sword_damage;
        shot_stone[k] = 1;
        return;
    }
}

static void update_sword(void) {
    if (swing_cool) swing_cool--;
    if (!swing && HIT(KEY_B) && !swing_cool && !ko_timer) {
        swing = swing_len;
        weapon_out = 1;
        swing_id++;
        play(SND_WHOOSH);
        if (weapon == SLING) fire_stone();
    }
    if (!swing) return;
    if (weapon != SLING) {
        s32 cx = (px >> 4) + 8, cy = (py >> 4) + 8;
        for (s32 i = 0; i < MAX_CROWS; i++) {
            if (crow_state[i] != CROW_FLY && crow_state[i] != CROW_HURT && crow_state[i] != CROW_EAT) continue;
            if (crow_hit_by[i] == swing_id) continue;   // one hit per swing
            s32 dx = (crow_x[i] >> 4) + 8 - cx, dy = (crow_y[i] >> 4) + 8 - cy;
            s32 along = pdir == DIR_RIGHT ? dx : pdir == DIR_LEFT ? -dx : pdir == DIR_DOWN ? dy : -dy;
            s32 side = pdir == DIR_RIGHT || pdir == DIR_LEFT ? dy : dx;
            if (abs32(along - sword_reach) > weapon_along[weapon] || abs32(side) > weapon_side[weapon]) continue;
            crow_hit_by[i] = swing_id;
            hit_crow(i, sword_damage, pdir == DIR_RIGHT ? 64 : pdir == DIR_LEFT ? -64 : 0,   // knocked away
                     pdir == DIR_DOWN ? 64 : pdir == DIR_UP ? -64 : 0);
        }
    }
    if (--swing == 0) swing_cool = weapon == SLING ? SLING_RELOAD : 6;
}

// Sword poses: which cell and flips point the blade each way.
enum { P_U, P_D, P_L, P_R, P_UR, P_UL, P_DR, P_DL };
static const u8 pose_kind[8] = { 0, 0, 1, 1, 2, 2, 2, 2 };   // 0 up cell, 1 right cell, 2 up-right cell
static const u8 pose_flags[8] = { 0, VFLIP, HFLIP, 0, 0, HFLIP, VFLIP, HFLIP | VFLIP };
// Each weapon's cells (up, right, up-right; the slingshot has no up-right pose) and where the hand grips them.
static const u8 weapon_cell[WEAPONS][3] = {
    { SPR_SWORD_UP, SPR_SWORD_RIGHT, SPR_SWORD_DIAG }, { SPR_IRON_UP, SPR_IRON_RIGHT, SPR_IRON_DIAG },
    { SPR_SPEAR_UP, SPR_SPEAR_RIGHT, SPR_SPEAR_DIAG }, { SPR_SCYTHE_UP, SPR_SCYTHE_RIGHT, SPR_SCYTHE_DIAG },
    { SPR_SLING_UP, SPR_SLING_RIGHT, SPR_SLING_RIGHT } };
static s32 grip_x(u32 kind) { return kind == 1 ? 2 : kind == 0 ? 8 : weapon <= IRON ? 4 : 3; }
static s32 grip_y(u32 kind) { return kind == 1 ? 8 : kind == 0 ? 13 : weapon <= IRON ? 13 : 14; }
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
    u32 step = swing ? (swing_len - swing) * 3 / swing_len : 3;
    if (weapon == SPEAR) step = 2;   // the spear jabs straight ahead
    u32 pose = weapon == SLING ? P_U : swing_pose[pdir][step];   // the slingshot is always held upright (Caleb)
    u32 kind = pose_kind[pose], flags = pose_flags[pose];
    u32 cell = weapon_cell[weapon][kind];
    s32 gx = grip_x(kind), gy = grip_y(kind);
    if (flags & HFLIP) gx = 15 - gx;
    if (flags & VFLIP) gy = 15 - gy;
    s32 bob = (pstep >> 3) & 1;
    s32 jab = weapon == SPEAR && swing ? 5 : 0;   // the spear thrusts forward
    s32 jx = pdir == DIR_RIGHT ? jab : pdir == DIR_LEFT ? -jab : 0, jy = pdir == DIR_DOWN ? jab : pdir == DIR_UP ? -jab : 0;
    put_sprite((px >> 4) + hand_x[pdir] - gx + jx, (py >> 4) - bob + hand_y[pdir] - gy + jy, cell,
               flags | (weapon == IRON ? BANK(7) : 0));
}

// ---------- farm expansion (v0.8, Caleb) ----------
// The fenced field grows in 3 steps, bought on the FARM page: 7x3 -> 7x4 (a row down) -> 9x4 (a column each
// side) -> 9x5 (a row up). Up to 9x4 a one-tile path goes all the way around; at 9x5 the top fence meets the
// bushes. The gate stays at the bottom middle (x 7). Crops and scarecrows inside stay where they are.
static const u8 field_x0[4] = { 4, 4, 3, 3 }, field_x1[4] = { 10, 10, 11, 11 };
static const u8 field_y0[4] = { 3, 3, 3, 2 }, field_y1[4] = { 5, 6, 6, 6 };
#define GATE_X 7

static void expand_field(void) {
    farm_step++;
    s32 x0 = field_x0[farm_step], x1 = field_x1[farm_step], y0 = field_y0[farm_step], y1 = field_y1[farm_step];
    for (s32 y = y0 - 1; y <= y1 + 1; y++)
        for (s32 x = x0 - 1; x <= x1 + 1; x++) {
            s32 i = y * MAP_W + x;
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) {   // inside: new ground becomes soil
                if (!is_field(farm[i])) { farm[i] = SOIL; grow[i] = 0; crop_kind[i] = CORN; }
            } else farm[i] = (y == y1 + 1 && x == GATE_X) ? DIRT : FENCE;   // the new fence and its gate
        }
    for (s32 y = 0; y < MAP_H; y++)   // fence pieces join their new neighbors
        for (s32 x = 0; x < MAP_W; x++) draw_meta(x, y);
}

// ---------- crow cannon (v0.8.1, Caleb: "stock it with pumpkins") ----------
// Bought on the FARM page (max 2), it sits on a bottom fence corner: no crop slot, no placing. Press A next
// to it to load the pumpkins in your bag (up to 10). Every 3 s a loaded cannon lobs a pumpkin at the
// nearest crow within 120 px; it bursts and hits every crow within one tile for 3 damage.
#define CANNON_PRICE 160
#define CANNON_AMMO 10
#define CANNON_RELOAD 180
#define CANNON_RANGE 120
#define BALLS 4
#define BALL_FRAMES 40
#define BALL_DAMAGE HP(3)
#define BURST_FRAMES 12
static s32 ball_t[BALLS], ball_x0[BALLS], ball_y0[BALLS], ball_x1[BALLS], ball_y1[BALLS];   // 0: free

static s32 cannon_x(s32 k) { return (k ? field_x1[farm_step] + 1 : field_x0[farm_step] - 1) * 16; }   // px
static s32 cannon_y(s32 k) { (void)k; return (field_y1[farm_step] + 1) * 16; }

// A next to a cannon loads it with the pumpkins in the bag. Returns 1 if a cannon was in reach.
static int cannon_work(void) {
    s32 fx = (px >> 4) + 8, fy = (py >> 4) + 12;   // feet
    if (fx >> 4 >= field_x0[farm_step] && fx >> 4 <= field_x1[farm_step] &&
        fy >> 4 >= field_y0[farm_step] && fy >> 4 <= field_y1[farm_step]) return 0;   // in the field: A farms
    for (s32 k = 0; k < cannons; k++) {
        if (abs32(cannon_x(k) + 8 - fx) > 20 || abs32(cannon_y(k) + 8 - fy) > 20) continue;
        char buf[24];
        s32 n = CANNON_AMMO - can_ammo[k];
        if (n > crops[PUMPKIN]) n = crops[PUMPKIN];
        if (!crops[PUMPKIN] && can_ammo[k] < CANNON_AMMO) { show_message("NO PUMPKINS TO LOAD"); play(SND_NOSEEDS); return 1; }
        can_ammo[k] += n;
        crops[PUMPKIN] -= n;
        n = copy_str(buf, "CANNON ");
        n += fmt_num(buf + n, can_ammo[k]);
        copy_str(buf + n, " OF 10");
        show_message(buf);
        play(SND_PLANT);
        draw_hud();
        return 1;
    }
    return 0;
}

// Where each barrel pose's muzzle sits, from the pivot (8, 9) in the cell.
static const signed char muzzle_x[5] = { -7, -5, 0, 5, 7 }, muzzle_y[5] = { 0, -5, -7, -5, 0 };
#define CANNON_TURN 5   // frames per barrel step

// The pose that points at (tx, ty) from the pivot: up within ~22 degrees of straight up, sideways within ~22
// of flat (a crow lower than the cannon counts as flat), else diagonal. No division.
static s32 aim_pose(s32 x, s32 y, s32 tx, s32 ty) {
    s32 dx = tx - x, up = y - ty, ax = abs32(dx);
    if (up < 0) up = 0;
    if (5 * ax <= 2 * up) return 2;
    if (2 * ax >= 5 * up) return dx < 0 ? 0 : 4;
    return dx < 0 ? 1 : 3;
}

static void update_cannons(void) {
    for (s32 k = 0; k < cannons; k++) {
        if (can_kick[k]) can_kick[k]--;
        if (can_cool[k]) can_cool[k]--;
        s32 x = cannon_x(k) + 8, y = cannon_y(k) + 9;   // the pivot
        s32 c = nearest_crow(x, y, CANNON_RANGE, -1);
        if (c >= 0) can_want[k] = aim_pose(x, y, (crow_x[c] >> 4) + 8, (crow_y[c] >> 4) + 8);
        if (can_pose[k] != can_want[k] && (frame % CANNON_TURN) == 0)
            can_pose[k] += can_pose[k] < can_want[k] ? 1 : -1;
        if (c < 0 || can_cool[k] || !can_ammo[k] || can_pose[k] != can_want[k]) continue;
        for (s32 b = 0; b < BALLS; b++) {
            if (ball_t[b]) continue;
            ball_t[b] = 1;
            ball_x0[b] = x + muzzle_x[can_pose[k]]; ball_y0[b] = y + muzzle_y[can_pose[k]];
            ball_x1[b] = (crow_x[c] >> 4) + 8; ball_y1[b] = (crow_y[c] >> 4) + 8;
            can_ammo[k]--;
            can_cool[k] = CANNON_RELOAD;
            can_kick[k] = 10;
            play(SND_WHOOSH);
            break;
        }
    }
    for (s32 b = 0; b < BALLS; b++) {
        if (!ball_t[b]) continue;
        if (++ball_t[b] == BALL_FRAMES) {   // it lands: burst
            for (s32 i = 0; i < MAX_CROWS; i++)
                if (crow_targetable(i) && abs32((crow_x[i] >> 4) + 8 - ball_x1[b]) <= 16 &&
                    abs32((crow_y[i] >> 4) + 8 - ball_y1[b]) <= 16)
                    hit_crow(i, BALL_DAMAGE, 0, 0);
            play(SND_HURT);
        }
        if (ball_t[b] >= BALL_FRAMES + BURST_FRAMES) ball_t[b] = 0;
    }
}

static void draw_cannons(void) {
    static const u8 barrel_cell[5] = { SPR_BARREL_RIGHT, SPR_BARREL_DIAG, SPR_BARREL_UP, SPR_BARREL_DIAG, SPR_BARREL_RIGHT };
    for (s32 k = 0; k < cannons; k++) {
        s32 p = can_pose[k], x = cannon_x(k), y = cannon_y(k), back = can_kick[k] > 6;   // recoil: 1 px back
        if (can_kick[k] > 4)   // a puff of smoke at the muzzle
            put_sprite(x + muzzle_x[p] + muzzle_x[p] / 3, y + 1 + muzzle_y[p] + muzzle_y[p] / 3, SPR_PUFF_SMALL, 0);
        put_sprite(x - (back ? (muzzle_x[p] > 0) - (muzzle_x[p] < 0) : 0), y + (back && muzzle_y[p] ? 1 : 0),
                   barrel_cell[p], p < 2 ? HFLIP : 0);   // drawn first, so it sits on top of the carriage
        put_sprite(x, y, SPR_CANNON, 0);
    }
    for (s32 b = 0; b < BALLS; b++) {
        s32 t = ball_t[b];
        if (!t) continue;
        if (t >= BALL_FRAMES) { put_sprite(ball_x1[b] - 8, ball_y1[b] - 8, SPR_PUFF_BIG, 0); continue; }
        s32 x = ball_x0[b] + (ball_x1[b] - ball_x0[b]) * t / BALL_FRAMES;
        s32 y = ball_y0[b] + (ball_y1[b] - ball_y0[b]) * t / BALL_FRAMES - (sin64[t * 32 / BALL_FRAMES] * 24 >> 8);
        put_sprite(x - 8, y - 8, SPR_BALL, 0);
    }
}

// ---------- dusk shop ----------
// Seed and crop prices: seed_price[] and sell_price[] with the crops. v0.8 cut the prices by about a third
// (Caleb: money was hard to get).
static const s32 sickle_price[2] = { 30, 60 };
#define SCARECROW_PRICE 30
static const s32 scarecrow_price[2] = { 40, 80 };
static const s32 expand_price[3] = { 60, 120, 200 };   // the field: 7x3 -> 7x4 -> 9x4 -> 9x5
// Pages (v0.8): main, SELL, SEEDS, WEAPONS, FARM, and the dawn blessing. A opens a page or buys, B goes back.
enum { PAGE_MAIN, PAGE_SELL, PAGE_SEEDS, PAGE_WEAPONS, PAGE_FARM, PAGE_BLESS };
enum { MAIN_SELL, MAIN_SEEDS, MAIN_WEAPONS, MAIN_FARM, MAIN_NEXT, MAIN_ITEMS };
enum { SELL_ALL, SELL_ITEMS = 1 + KINDS };              // then one line per crop
enum { W_ITEMS = WEAPONS };                             // one line per weapon
enum { F_SICKLE, F_SCARECROW, F_SCARECROW_LV, F_CANNON, F_EXPAND, F_ITEMS };
enum { B_ATTACK, B_SCARE, B_HEART, B_ITEMS };
static const s32 page_items[6] = { MAIN_ITEMS, SELL_ITEMS, KINDS, W_ITEMS, F_ITEMS, B_ITEMS };
static s32 shop_open, shop_page, shop_cursor, main_cursor;
// Window: columns 2..27 (26 wide, centered; v0.9.1 widened it from 3..26 so big numbers fit), rows 2..17
// (between the HUD and the message row). A blank row above the title, two under it, items every 2nd row
// (rows 6-14), the bottom line on row 16, a blank row 17. Text never reaches the box's own edge columns
// (Caleb): at most 24 letters, columns 3..26 (test/menu_check.py checks every window). The hint goes on row 19.
#define SHOP_L 2
#define SHOP_W 26
#define SHOP_T 2
#define SHOP_ROWS 16
#define SHOP_EQ 17     // column of "=" in an item line (v0.9.1: one space after the longest name)
#define SHOP_INFO (SHOP_T + 14)   // the bottom line: YOU HAVE, or about the line under the cursor

// A number for a window line, shown as at most `max` (999 for counts, 99999 for coins), so a line keeps its width.
static s32 fmt_cap(char *buf, s32 n, s32 max) { return fmt_num(buf, n < 0 ? 0 : n > max ? max : n); }
#define COUNT_MAX 999
#define COINS_MAX 99999

static void shop_line(s32 row, const char *s) {
    put_text(SHOP_L, row, "                          ");
    put_text(SHOP_L + 1, row, s);
}
static void shop_centered(s32 row, const char *s) {   // a line centered in the window
    s32 n = 0;
    while (s[n]) n++;
    put_text(SHOP_L, row, "                          ");
    put_text(SHOP_L + (SHOP_W - n) / 2, row, s);
}
// Item line k (rows 6, 8, .. 14): "  NAME" padded to SHOP_EQ, then "= $price" (price >= 0) or "MAX" (-1);
// price -2: the name only.
static void shop_item(s32 k, const char *name, s32 price) {
    char buf[26];
    s32 n = copy_str(buf, "  ");
    n += copy_str(buf + n, name);
    if (price != -2) while (n < SHOP_EQ) buf[n++] = ' ';
    buf[n] = 0;
    if (price == -1) copy_str(buf + n, "MAX");
    else if (price >= 0) {   // "= $123"; a 5-digit price drops the space so the line keeps to 24 letters
        char num[8];
        s32 len = fmt_cap(num, price, COINS_MAX);
        n += copy_str(buf + n, n + 3 + len <= SHOP_W - 2 ? "= $" : "=$");
        copy_str(buf + n, num);
    }
    shop_line(SHOP_T + 4 + 2 * k, buf);
}
// A tool line: "SICKLE LV2" and its price, or "SICKLE LV3" and MAX.
static void shop_level(s32 k, const char *name, s32 lv, const s32 *price) {
    char buf[20];
    s32 n = copy_str(buf, name);
    fmt_num(buf + n, lv >= 3 ? 3 : lv + 1);
    shop_item(k, buf, lv >= 3 ? -1 : price[lv - 1]);
}

static s32 keep_pumpkins(void) { return cannons > 0; }   // pumpkins are ammo once you own a crow cannon

// What SELL ALL sells: every crop, but pumpkins stay as ammo once you own a cannon.
static s32 sell_all_value(void) { return crops_value() - (keep_pumpkins() ? crops[PUMPKIN] * sell_price[PUMPKIN] : 0); }
static s32 sell_all_count(void) { return crops_total() - (keep_pumpkins() ? crops[PUMPKIN] : 0); }

static void draw_shop(void) {
    char buf[26];
    s32 n;
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) shop_line(r, "");
    s32 info = 1;   // the bottom line shows YOU HAVE unless a page uses it
    if (shop_page == PAGE_MAIN) {
        if (shop_last) copy_str(buf, "LAST TIME TO BUY +150");
        else if (shop_dawn) copy_str(buf, "DAWN. BLOOD BONUS +25");
        else {
            n = copy_str(buf, "DUSK. DAY ");
            n += fmt_num(buf + n, day);
            copy_str(buf + n, " IS OVER.");
        }
        shop_centered(SHOP_T + 1, buf);
        n = copy_str(buf, "SELL  &");
        fmt_cap(buf + n, crops_total(), COUNT_MAX);
        shop_item(MAIN_SELL, buf, -2);
        shop_item(MAIN_SEEDS, "SEEDS", -2);
        shop_item(MAIN_WEAPONS, "WEAPONS", -2);
        shop_item(MAIN_FARM, "FARM", -2);
        shop_item(MAIN_NEXT, "NEXT DAY", -2);
    } else if (shop_page == PAGE_SELL) {
        shop_centered(SHOP_T + 1, "SELL");
        n = copy_str(buf, "ALL   &");
        fmt_cap(buf + n, sell_all_count(), COUNT_MAX);
        shop_item(SELL_ALL, buf, sell_all_value());
        for (s32 k = 0; k < KINDS; k++) {   // "^ CORN   &3 = $9"
            n = copy_str(buf, seed_icon[k]);
            n += copy_str(buf + n, " ");
            n += copy_str(buf + n, crop_name[k]);
            while (n < 10) buf[n++] = ' ';
            buf[n++] = '&';
            fmt_cap(buf + n, crops[k], COUNT_MAX);
            shop_item(1 + k, buf, crops[k] * sell_price[k]);
        }
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
                fmt_cap(buf + n, seeds[k], COUNT_MAX);
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
        shop_centered(SHOP_INFO, buf);
        info = 0;
    } else if (shop_page == PAGE_WEAPONS) {
        static const char *const what[WEAPONS] = { "QUICK, ALL-ROUND", "BIG HITS, SLOWER", "REACHES 2 TILES",
                                                   "HITS 3 TILES WIDE", "SHOOTS ANY DISTANCE" };
        shop_centered(SHOP_T + 1, "WEAPONS");
        for (s32 w = 0; w < WEAPONS; w++) {   // in hand: its upgrade line; else BUY (Caleb)
            if (w == weapon) {
                n = copy_str(buf, weapon_name[w]);
                copy_str(buf + n, " LV");
                shop_level(w, buf, sword_lv, weapon_up[w]);
            } else {
                n = copy_str(buf, "BUY ");
                copy_str(buf + n, weapon_name[w]);
                shop_item(w, buf, weapon_buy[w]);
            }
        }
        s32 w = shop_cursor;   // the bottom line: what the weapon under the cursor does
        if (w == weapon) {
            s32 d = sword_damage;
            n = copy_str(buf, "IN HAND  DAMAGE ");
            n += fmt_num(buf + n, d / 2);
            copy_str(buf + n, d & 1 ? ".5" : "");
        } else if (weapon_lv[w]) {
            n = copy_str(buf, what[w]);
            n += copy_str(buf + n, " LV");
            fmt_num(buf + n, weapon_lv[w]);
        } else copy_str(buf, what[w]);
        shop_centered(SHOP_INFO, buf);
        info = 0;
    } else if (shop_page == PAGE_FARM) {
        shop_centered(SHOP_T + 1, "FARM");
        shop_level(F_SICKLE, "SICKLE LV", sickle_lv, sickle_price);
        shop_item(F_SCARECROW, "SCARECROW  #1", SCARECROW_PRICE);
        shop_level(F_SCARECROW_LV, "SCARECROW LV", scarecrow_lv, scarecrow_price);
        shop_item(F_CANNON, "CROW CANNON", cannons >= MAX_CANNONS ? -1 : CANNON_PRICE);
        if (farm_step >= 3) shop_item(F_EXPAND, "FIELD 9X5", -1);
        else {
            static const char *const next_size[3] = { "FIELD 7X4", "FIELD 9X4", "FIELD 9X5" };
            shop_item(F_EXPAND, next_size[farm_step], expand_price[farm_step]);
        }
    } else {                           // the dawn blessing after a blood moon survived
        static const char *const what[B_ITEMS] = { "+0.5 WEAPON DAMAGE", "+0.5 SCARECROW DAMAGE",
                                                   "ONE MORE HEART (MAX 5)" };   // each fits the window
        shop_centered(SHOP_T + 1, "CHOOSE A BLESSING");
        shop_item(B_ATTACK, "ATTACK UP", -2);
        shop_item(B_SCARE, "SCARECROWS UP", -2);
        shop_item(B_HEART, max_hearts >= 5 ? "+1 HEART  (MAX)" : "+1 HEART", -2);
        shop_centered(SHOP_INFO, what[shop_cursor]);
        info = 0;
    }
    put_text(SHOP_L + 1, SHOP_T + 4 + 2 * shop_cursor, ">");
    if (info) {
        n = copy_str(buf, "YOU HAVE ");   // dropped when the numbers alone fill the line
        n += copy_str(buf + n, "$");
        n += fmt_cap(buf + n, coins, COINS_MAX);
        n += copy_str(buf + n, " &");
        n += fmt_cap(buf + n, crops_total(), COUNT_MAX);
        n += copy_str(buf + n, " ");
        n += copy_str(buf + n, seed_icon[sel_seed]);
        n += fmt_cap(buf + n, seeds[sel_seed], COUNT_MAX);
        n += copy_str(buf + n, " #");
        n += fmt_num(buf + n, scarecrow_bag);
        shop_centered(SHOP_INFO, n <= SHOP_W - 2 ? buf : buf + 9);
    }
    clear_row(19);
    put_text(4, 19, shop_page == PAGE_MAIN ? "UP/DOWN PICK  A CHOOSE" : shop_page == PAGE_BLESS ? "UP/DOWN PICK  A TAKE" : "A BUY  B BACK");
}

static void open_shop(void) {
    shop_open = 1;
    shop_page = bless_pending ? PAGE_BLESS : PAGE_MAIN;
    shop_cursor = bless_pending ? 0 : crops_total() ? MAIN_SELL : MAIN_SEEDS;
    bless_pending = 0;
    swing = 0;
    if (shop_dawn) set_light_dawn();   // after a blood moon it is already morning
    else set_light(0, 256);            // night
    msg_timer = 0;
    draw_shop();
}

static void close_shop(void) {
    shop_open = 0;
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) clear_row(r);
    clear_row(19);
    shop_dawn = 0;
    if (shop_last) {     // "this is your last time to make purchases": now the final blood moons
        shop_last = 0;
        finale = 1;
        start_blood_moon();
        return;
    }
    set_light(0, 0);     // a new morning
    if (blocked(px, py)) {   // a new fence went up where he stood: back to the start, below the gate
        px = PLAYER_START_X * 16 * 16;
        py = PLAYER_START_Y * 16 * 16;
    }
    day++;
    start_day();
    if (notice_pending) { notice_pending = 0; show_notice(training_notice, 4); }
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
#define BUY_HOLD 20    // hold A this long on a seed and it keeps buying (Caleb, v0.7.2)
#define BUY_EVERY 4    // then one seed every 4 frames, until A is let go or the coins run out
static s32 buy_hold;
static void update_shop(void) {
    if (shop_page == PAGE_SEEDS && HELD(KEY_A) && !HIT(KEY_A)) {
        s32 k = shop_cursor;
        if (buy_hold >= 0 && ++buy_hold >= BUY_HOLD && (buy_hold & (BUY_EVERY - 1)) == 0) {
            if (day >= unlock_day[k] && coins >= seed_price[k]) {
                coins -= seed_price[k];
                seeds[k]++;
                play(SND_PLANT);
                draw_hud();
                draw_shop();
            } else {
                play(SND_NOSEEDS);
                buy_hold = -1;   // stop until A is let go
            }
        }
        return;
    }
    if (!HELD(KEY_A)) buy_hold = 0;
    if (HIT(KEY_UP) && shop_cursor > 0) { shop_cursor--; draw_shop(); }
    if (HIT(KEY_DOWN) && shop_cursor < page_items[shop_page] - 1) { shop_cursor++; draw_shop(); }
    if (HIT(KEY_B) && shop_page != PAGE_MAIN && shop_page != PAGE_BLESS) {
        shop_page = PAGE_MAIN;
        shop_cursor = main_cursor;
        draw_shop();
        return;
    }
    if (!HIT(KEY_A)) return;
    s32 ok = 1;
    if (shop_page == PAGE_MAIN) {
        if (shop_cursor == MAIN_NEXT) {
            close_shop();
            return;
        }
        static const u8 page_of[MAIN_ITEMS] = { PAGE_SELL, PAGE_SEEDS, PAGE_WEAPONS, PAGE_FARM, PAGE_MAIN };
        main_cursor = shop_cursor;   // open a page
        shop_page = page_of[shop_cursor];
        shop_cursor = 0;
        draw_shop();
        return;
    } else if (shop_page == PAGE_BLESS) {
        if (shop_cursor == B_ATTACK) { bless_attack++; set_sword_lv(sword_lv); }
        else if (shop_cursor == B_SCARE) bless_scare++;
        else if ((ok = max_hearts < 5)) hearts = ++max_hearts;
        if (!ok) { play(SND_NOSEEDS); return; }
        play(SND_HARVEST);
        shop_page = PAGE_MAIN;
        shop_cursor = crops_total() ? MAIN_SELL : MAIN_SEEDS;
        draw_hud();
        draw_shop();
        return;
    } else if (shop_page == PAGE_SELL) {
        if (shop_cursor == SELL_ALL) {
            if ((ok = sell_all_count() > 0)) {
                coins += sell_all_value();
                for (s32 k = 0; k < KINDS; k++) if (k != PUMPKIN || !keep_pumpkins()) crops[k] = 0;
            }
        } else {
            s32 k = shop_cursor - 1;
            if ((ok = crops[k] > 0)) { coins += crops[k] * sell_price[k]; crops[k] = 0; }
        }
    } else if (shop_page == PAGE_SEEDS) {
        s32 k = shop_cursor;
        if ((ok = day >= unlock_day[k] && coins >= seed_price[k])) { coins -= seed_price[k]; seeds[k]++; }
    } else if (shop_page == PAGE_WEAPONS) {
        s32 w = shop_cursor;
        if (w == weapon) {           // upgrade the weapon in hand
            s32 lv = sword_lv;
            if ((ok = buy_level(&lv, weapon_up[w]))) set_sword_lv(lv);
        } else if ((ok = coins >= weapon_buy[w])) {   // buy another: it replaces the one in hand
            coins -= weapon_buy[w];
            take_weapon(w);
        }
    } else if (shop_cursor == F_SICKLE) {
        ok = buy_level(&sickle_lv, sickle_price);
    } else if (shop_cursor == F_SCARECROW) {
        if ((ok = coins >= SCARECROW_PRICE)) { coins -= SCARECROW_PRICE; scarecrow_bag++; }
    } else if (shop_cursor == F_SCARECROW_LV) {
        ok = buy_level(&scarecrow_lv, scarecrow_price);
    } else if (shop_cursor == F_CANNON) {
        if ((ok = cannons < MAX_CANNONS && coins >= CANNON_PRICE)) {
            coins -= CANNON_PRICE;
            can_ammo[cannons] = can_cool[cannons] = can_kick[cannons] = 0;
            can_pose[cannons] = can_want[cannons] = cannons ? 1 : 3;   // facing the field
            cannons++;
        }
    } else {
        if ((ok = farm_step < 3 && coins >= expand_price[farm_step])) { coins -= expand_price[farm_step]; expand_field(); }
    }
    if (!ok) { play(SND_NOSEEDS); return; }
    play(shop_page == PAGE_SEEDS ? SND_PLANT : SND_HARVEST);
    draw_hud();
    draw_shop();
}

// ---------- run: start and game over ----------
static s32 game_over;
static s32 run_saved, dev_run;   // this run is in the history already; this run came from dev mode (v0.9)

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
    bless_attack = bless_scare = 0;
    for (s32 w = 0; w < WEAPONS; w++) weapon_lv[w] = 0;
    weapon = WOOD;
    set_sword_lv(1);
    cannons = 0;
    for (s32 b = 0; b < BALLS; b++) ball_t[b] = 0;
    scarecrows = scarecrow_bag = sc_warned = 0;
    scarecrow_lv = 1;
    for (s32 k = 0; k < MAX_SHOTS; k++) shot_life[k] = 0;
    spawning = 1; shop_open = 0;
    blood_on = blood_kos = shop_dawn = blood_count = notice_pending = notice_timer = 0;
    dev_money = dev_god = 0;   // a normal run; the dev window sets them again after new_run
    finale = finale_pending = shop_last = stat_crops = stat_kills = bless_pending = 0;
    blood_next = BLOOD_FIRST;
    game_over = 0;
    run_saved = dev_run = 0;
    start_day();
}

// Lose: no crops in the field, no seeds, and not enough coins for a seed (DESIGN.md). Crops in the bag
// count as the coins they sell for at dusk.
static void check_game_over(void) {
    if (seeds_total() > 0 || coins + crops_value() >= seed_price[CORN] || field_has_crops()) return;
    end_screen(END_FARM);
}

// The end of a run, in the shop window: what happened, the highest day, crops grown, crows defeated.
// ---------- run history (v0.9, Caleb: best run + last 5) ----------
// Only this history is saved; every run still starts fresh. Battery-backed SRAM at 0x0E000000, one byte at a
// time; the "SRAM_V113" tag tells emulators (and the R36) to keep a save file. Layout: magic, runs played,
// the best run, the last 5 runs (newest first), a checksum. A run is two words: day | crops << 16 and
// crows | ending << 16 (each capped at 65535). Dev mode runs are not recorded.
#define SRAM ((volatile u8 *)0x0E000000)
__attribute__((used, aligned(4))) static const char save_tag[] = "SRAM_V113";
#define STATS_MAGIC 0x31534643   // "CFS1"
#define HISTORY 5
static u32 runs_played, best_a, best_b, hist_a[HISTORY], hist_b[HISTORY];

static void sram_put32(u32 at, u32 v) { for (u32 k = 0; k < 4; k++) SRAM[at + k] = (u8)(v >> (8 * k)); }
static u32 sram_get32(u32 at) {
    u32 v = 0;
    for (u32 k = 0; k < 4; k++) v |= (u32)SRAM[at + k] << (8 * k);
    return v;
}
static u32 stats_sum(void) {
    u32 sum = runs_played * 7 + best_a * 3 + best_b * 5;
    for (u32 i = 0; i < HISTORY; i++) sum += hist_a[i] * (11 + i) + hist_b[i] * (17 + i);
    return sum;
}
static void stats_load(void) {
    runs_played = 0;
    if (sram_get32(0) != STATS_MAGIC) return;
    runs_played = sram_get32(4);
    best_a = sram_get32(8); best_b = sram_get32(12);
    for (u32 i = 0; i < HISTORY; i++) { hist_a[i] = sram_get32(16 + 8 * i); hist_b[i] = sram_get32(20 + 8 * i); }
    if (sram_get32(16 + 8 * HISTORY) != stats_sum()) runs_played = 0;   // damaged: start the history over
}
static void stats_save(void) {
    sram_put32(0, 0);   // invalid while writing
    sram_put32(4, runs_played);
    sram_put32(8, best_a); sram_put32(12, best_b);
    for (u32 i = 0; i < HISTORY; i++) { sram_put32(16 + 8 * i, hist_a[i]); sram_put32(20 + 8 * i, hist_b[i]); }
    sram_put32(16 + 8 * HISTORY, stats_sum());
    sram_put32(0, STATS_MAGIC);   // written last: a cut-off save never looks valid
}
static u32 cap16(s32 v) { return v < 0 ? 0 : v > 65535 ? 65535 : (u32)v; }
// Better run: a win beats any loss, then the higher day, then more crows defeated.
static int run_better(u32 a1, u32 b1, u32 a2, u32 b2) {
    int w1 = (b1 >> 16) == END_WON, w2 = (b2 >> 16) == END_WON;
    if (w1 != w2) return w1;
    if ((a1 & 0xFFFF) != (a2 & 0xFFFF)) return (a1 & 0xFFFF) > (a2 & 0xFFFF);
    return (b1 & 0xFFFF) > (b2 & 0xFFFF);
}
static void record_run(s32 kind) {
    if (run_saved || dev_run) return;
    run_saved = 1;
    u32 a = cap16(day) | cap16(stat_crops) << 16, b = cap16(stat_kills) | (u32)kind << 16;
    for (s32 i = HISTORY - 1; i > 0; i--) { hist_a[i] = hist_a[i - 1]; hist_b[i] = hist_b[i - 1]; }
    hist_a[0] = a; hist_b[0] = b;
    if (runs_played == 0 || run_better(a, b, best_a, best_b)) { best_a = a; best_b = b; }
    runs_played++;
    stats_save();
}

#define END_START_ROW (SHOP_T + 12)    // "PRESS START" blinks here
static void end_line(s32 row, const char *label, s32 value) {
    char buf[26];
    fmt_num(buf + copy_str(buf, label), value);
    shop_centered(row, buf);
}
static s32 end_kind;   // the last ending, to redraw it after the dev window closes
static void end_screen(s32 kind) {
    end_kind = kind;
    record_run(kind);
    static const char *const title[4] = { "THE FARM IS LOST", "THE BLOOD MOON WON", "THE LAST MOON WON", "YOU WON!" };
    static const char *const sub[4] = { "YOU DID YOUR BEST", "YOU DID YOUR BEST", "YOU DID YOUR BEST",
                                        "THANK YOU FOR PLAYING!" };
    game_over = 1;
    msg_timer = notice_timer = 0;
    shop_open = 0;
    clear_row(19);   // no stale "DAWN IN n" under the window
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) shop_line(r, "");
    shop_centered(SHOP_T + 1, title[kind]);
    shop_centered(SHOP_T + 3, sub[kind]);
    end_line(SHOP_T + 6, "MAX DAY ", day);
    end_line(SHOP_T + 8, "CROPS GROWN ", stat_crops);
    end_line(SHOP_T + 10, "CROWS DEFEATED ", stat_kills);
    shop_centered(END_START_ROW, "PRESS START");
    if (kind == END_WON) set_light_dawn();
}

static void end_screen_again(void) { end_screen(end_kind); }

// ---------- dev mode (v0.7.1, Caleb: test the late game) ----------
// L + R + B together, while playing or on an end screen, opens the DEV MODE window (the game pauses).
// Up/Down pick a line, Left/Right change it (L/R: by 10 on DAY and SEEDS), A starts a fresh run on that day
// with that gear, B closes. Unlimited money keeps coins at 9999; invincible makes pecks do nothing.
enum { DEV_DAY, DEV_WEAPON, DEV_SWORD, DEV_SICKLE, DEV_SC_LV, DEV_SC_BAG, DEV_CANNONS, DEV_FIELD, DEV_SEEDS,
       DEV_BLOODS, DEV_TONIGHT, DEV_MONEY, DEV_GOD, DEV_ITEMS };
enum { TONIGHT_NORMAL, TONIGHT_BLOOD, TONIGHT_FINAL };
static const s32 dev_min[DEV_ITEMS] = { 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
static const s32 dev_max[DEV_ITEMS] = { 99, WEAPONS - 1, 3, 3, 3, MAX_SCARECROWS, MAX_CANNONS, 3, 99, FINALE_AFTER, 2, 1, 1 };
static s32 dev_open, dev_cursor, dev_val[DEV_ITEMS];
static s32 title_on;   // the title screen is showing (v0.9; below)
static s32 title_choice, stats_open;   // with a history: 0 PLAY, 1 STATS; the STATS page is showing
static void title_text(void);
static void draw_stats(void);

static void draw_dev(void) {
    static const char *const label[DEV_ITEMS] = { "DAY", "WEAPON", "WEAPON LV", "SICKLE LV", "SCARE LV",
        "SCARECROWS", "CANNONS", "FIELD STEP", "SEEDS EACH", "MOONS DONE", "TONIGHT", "MONEY", "INVINCIBLE" };
    static const char *const tonight[3] = { "NORMAL", "BLOOD", "FINAL" };
    static const char *const short_weapon[WEAPONS] = { "WOOD", "IRON", "SPEAR", "SCYTHE", "SLING" };
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) shop_line(r, "");
    shop_centered(SHOP_T + 1, "DEV MODE");   // a blank row above it, like the shop titles
    for (s32 k = 0; k < DEV_ITEMS; k++) {   // one line each, rows 4-16
        char buf[26];
        s32 n = copy_str(buf, k == dev_cursor ? "> " : "  ");
        n += copy_str(buf + n, label[k]);
        while (n < 13) buf[n++] = ' ';
        buf[n] = 0;
        if (k == dev_cursor) n += copy_str(buf + n, "<");   // the value Left/Right change: < 3 >
        if (k == DEV_TONIGHT) n += copy_str(buf + n, tonight[dev_val[k]]);
        else if (k == DEV_WEAPON) n += copy_str(buf + n, short_weapon[dev_val[k]]);
        else if (k == DEV_MONEY) n += copy_str(buf + n, dev_val[k] ? "ENDLESS" : "NORMAL");
        else if (k == DEV_GOD) n += copy_str(buf + n, dev_val[k] ? "ON" : "OFF");
        else n += fmt_num(buf + n, dev_val[k]);
        if (k == dev_cursor) copy_str(buf + n, ">");
        shop_line(SHOP_T + 2 + k, buf);
    }
    clear_row(19);
    put_text(1, 19, "L/R +-10   A START  B CLOSE");
}

static void open_dev(void) {
    dev_open = 1;
    dev_cursor = 0;
    dev_val[DEV_DAY] = day;
    dev_val[DEV_WEAPON] = weapon;
    dev_val[DEV_SWORD] = sword_lv;
    dev_val[DEV_CANNONS] = cannons;
    dev_val[DEV_SICKLE] = sickle_lv;
    dev_val[DEV_SC_LV] = blood_on ? blood_saved_lv : scarecrow_lv;
    dev_val[DEV_SC_BAG] = scarecrow_bag;
    dev_val[DEV_FIELD] = farm_step;
    dev_val[DEV_SEEDS] = seeds[CORN];
    dev_val[DEV_BLOODS] = blood_count;
    dev_val[DEV_TONIGHT] = TONIGHT_NORMAL;
    dev_val[DEV_MONEY] = dev_money;
    dev_val[DEV_GOD] = dev_god;
    msg_timer = notice_timer = 0;
    draw_dev();
}

static void close_dev(void) {
    dev_open = 0;
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) clear_row(r);
    clear_row(19);
    if (title_on && stats_open) draw_stats();
    else if (title_on) title_text();
    else if (game_over) end_screen_again();
    else if (shop_open) draw_shop();
}

// A: a fresh run on the chosen day with the chosen gear.
static void dev_start(void) {
    dev_open = 0;
    title_on = 0;
    new_run();
    dev_run = 1;   // not recorded in the run history
    day = dev_val[DEV_DAY];
    weapon = dev_val[DEV_WEAPON];
    set_sword_lv(dev_val[DEV_SWORD]);
    cannons = dev_val[DEV_CANNONS];
    for (s32 k = 0; k < cannons; k++) {   // loaded, facing the field
        can_ammo[k] = CANNON_AMMO;
        can_cool[k] = can_kick[k] = 0;
        can_pose[k] = can_want[k] = k ? 1 : 3;
    }
    sickle_lv = dev_val[DEV_SICKLE];
    scarecrow_lv = dev_val[DEV_SC_LV];
    scarecrow_bag = dev_val[DEV_SC_BAG];
    while (farm_step < dev_val[DEV_FIELD]) expand_field();
    for (s32 k = 0; k < KINDS; k++) seeds[k] = day >= unlock_day[k] ? dev_val[DEV_SEEDS] : 0;
    blood_count = dev_val[DEV_BLOODS];
    dev_money = dev_val[DEV_MONEY];
    dev_god = dev_val[DEV_GOD];
    if (dev_val[DEV_TONIGHT] == TONIGHT_BLOOD) blood_next = day;
    else if (dev_val[DEV_TONIGHT] == TONIGHT_FINAL) { finale_pending = 1; blood_next = 99999; }
    else blood_next = day <= BLOOD_FIRST ? BLOOD_FIRST : day + 4;
    if (blood_count >= FINALE_AFTER && dev_val[DEV_TONIGHT] != TONIGHT_FINAL) blood_next = 99999;
    start_day();
    show_message("DEV MODE");
}

static void update_dev(void) {
    s32 k = dev_cursor, step = (k == DEV_DAY || k == DEV_SEEDS) ? 10 : 1, d = 0;
    if (HIT(KEY_UP) && dev_cursor > 0) dev_cursor--;
    if (HIT(KEY_DOWN) && dev_cursor < DEV_ITEMS - 1) dev_cursor++;
    if (HIT(KEY_LEFT)) d = -1;
    if (HIT(KEY_RIGHT)) d = 1;
    if (HIT(KEY_L)) d = -step;
    if (HIT(KEY_R)) d = step;
    if (d) {
        dev_val[k] += d;
        if (dev_val[k] < dev_min[k]) dev_val[k] = dev_min[k];
        if (dev_val[k] > dev_max[k]) dev_val[k] = dev_max[k];
    }
    if (HIT(KEY_A)) { dev_start(); return; }
    if (HIT(KEY_B)) { close_dev(); return; }
    if (keys_now != keys_prev) draw_dev();
}

// The combo: L, R, and B all down, one of them just pressed.
static int dev_combo(void) {
    u16 all = KEY_L | KEY_R | KEY_B;
    return (keys_now & all) == all && (keys_now & ~keys_prev & all);
}

// Unlimited money: coins stay at 9999.
static void dev_tick(void) {
    if (dev_money && coins != 9999) { coins = 9999; draw_hud(); }
}

// ---------- main ----------
// ---------- title screen (v0.9) ----------
// The farm behind, crows circling, the CROWFIELD banner, the tagline, PRESS START. START begins Day 1; every
// ending comes back here. Test scripts write TITLE_SKIP_MAGIC into TITLE_SKIP before the game boots
// (`0 0 !0203FFF0=1413829460`): EWRAM is unused, so crt0 leaves it alone, and the game then starts at once
// as it did before v0.9.
#define TITLE_SKIP (*(volatile u32 *)0x0203FFF0)
#define TITLE_SKIP_MAGIC 0x54455354   // "TEST"
#define TITLE_START_ROW 14

static void title_text(void) {
    for (s32 r = 0; r < 20; r++) clear_row(r);
    for (s32 ty = 0; ty < 3; ty++)
        for (s32 tx = 0; tx < BANNER_W; tx++)
            SCREENBLOCK(TEXT_SB)[(3 + ty) * 32 + 6 + tx] = (BANNER_BASE + ty * BANNER_W + tx) | (3 << 12);
    put_text(4, 8, "GROW. DEFEND. SURVIVE.");
    if (!runs_played) { put_text(9, TITLE_START_ROW, "PRESS START"); return; }
    put_text(11, 13, title_choice == 0 ? "> PLAY " : "  PLAY ");   // a history: PLAY or STATS
    put_text(11, 15, title_choice == 1 ? "> STATS" : "  STATS");
}

// One history line: label, then day, crops, crows, and how it ended, in fixed columns.
static void stats_line(s32 row, const char *label, u32 a, u32 b) {
    static const char *const how[4] = { "FARM", "MOON", "LAST", "WON" };
    char buf[26], num[8];
    s32 n = copy_str(buf, " ");   // the 22-wide table sits in the middle of the 24 letters
    n += copy_str(buf + n, label);
    u32 v[3] = { a & 0xFFFF, a >> 16, b & 0xFFFF };
    static const u8 right[3] = { 7, 12, 17 };   // last column of each number (after the leading space)
    for (s32 k = 0; k < 3; k++) {
        s32 len = fmt_num(num, v[k]);
        while (n < right[k] + 1 - len) buf[n++] = ' ';
        n += copy_str(buf + n, num);
    }
    buf[n++] = ' ';
    n += copy_str(buf + n, how[(b >> 16) & 3]);
    buf[n] = 0;
    shop_line(row, buf);
}

// STATS: the best run and the last 5 (newest first, numbered by run). B, A, or START goes back.
static void draw_stats(void) {
    char buf[26];
    for (s32 r = 0; r < 20; r++) clear_row(r);
    for (s32 r = SHOP_T; r < SHOP_T + SHOP_ROWS; r++) shop_line(r, "");
    shop_centered(SHOP_T + 1, "PAST RUNS");
    shop_line(SHOP_T + 3, " RUN DAY CROP CROW END");
    stats_line(SHOP_T + 5, "BEST", best_a, best_b);
    for (u32 i = 0; i < HISTORY && i < runs_played; i++) {
        s32 n = fmt_cap(buf, runs_played - i, 9999);   // the run column is 4 wide
        buf[n] = 0;
        stats_line(SHOP_T + 7 + i, buf, hist_a[i], hist_b[i]);
    }
    s32 n = copy_str(buf, "RUNS PLAYED ");
    fmt_cap(buf + n, runs_played, COINS_MAX);
    shop_centered(SHOP_INFO, buf);
    put_text(1, 19, "B BACK");
}

static void title_screen(void) {
    title_on = 1;
    stats_open = title_choice = 0;
    game_over = shop_open = 0;
    for (s32 i = 0; i < MAX_CROWS; i++) crow_state[i] = CROW_NONE;
    reset_farm();
    set_light(SUNSET_MAX, 160);   // an evening farm, so the banner stands out
    title_text();
}

// START: Day 1. Returns 1 when the run began this frame.
static int update_title(void) {
    if (stats_open) {
        if (HIT(KEY_A | KEY_B | KEY_START)) { stats_open = 0; title_text(); }
        return 0;
    }
    if (runs_played) {   // the PLAY / STATS menu
        if (HIT(KEY_UP | KEY_DOWN)) { title_choice ^= 1; title_text(); }
        if (HIT(KEY_A | KEY_START)) {
            if (title_choice == 0) { title_on = 0; new_run(); return 1; }
            stats_open = 1;
            draw_stats();
        }
        return 0;
    }
    if (HIT(KEY_START)) { title_on = 0; new_run(); return 1; }
    if (((frame >> 5) & 1) != ((frame - 1) >> 5 & 1))   // blink PRESS START
        for (s32 c = 0; c < 11; c++)   // off: clear cells (no dark bar), on: the text
            SCREENBLOCK(TEXT_SB)[TITLE_START_ROW * 32 + 9 + c] = (frame >> 5) & 1 ? 0 : glyph_entry("PRESS START"[c]);
    return 0;
}

// Five crows circling the banner, wings flapping, each facing the way it flies.
static void draw_title_crows(void) {
    for (s32 k = 0; k < 5; k++) {
        s32 a = ((frame >> 2) + k * 13) & 63;
        s32 x = 120 + (sin64[(a + 16) & 63] * 108 >> 8), y = 76 + (sin64[a] * 52 >> 8);
        put_sprite(x - 8, y - 8, ((frame >> 3) + k) & 1 ? SPR_CROW_A : SPR_CROW_B, sin64[a] > 0 ? HFLIP : 0);
    }
}

// Which song fits what is on screen. The dev window keeps whatever played before it.
static void pick_song(void) {
    if (dev_open) return;
    music_play(title_on ? SONG_TITLE : game_over ? -1 : shop_open ? SONG_DUSK :
               finale ? SONG_FINAL : blood_on ? SONG_BLOOD : SONG_DAY);
}

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
    for (u32 i = 0; i < 16; i++) PAL_OBJ[96 + i] = obj_pal[i];
    PAL_OBJ[96 + 1] = (20) | (20 << 5) | (22 << 10);   // white crow body: pale gray
    PAL_OBJ[96 + 9] = (29) | (29 << 5) | (30 << 10);   // wing: white
    PAL_OBJ[96 + 10] = (31) | (31 << 5) | (31 << 10);  // sheen: bright white
    for (u32 i = 0; i < 16; i++) { PAL_OBJ[112 + i] = obj_pal[i]; PAL_OBJ[128 + i] = obj_pal[i]; }
    PAL_OBJ[112 + 3] = (27) | (28 << 5) | (29 << 10);  // iron sword: light steel blade
    PAL_OBJ[112 + 4] = (16) | (17 << 5) | (19 << 10);  // steel shade
    PAL_OBJ[128 + 3] = (19) | (19 << 5) | (19 << 10);  // slingshot stone: gray
    PAL_OBJ[128 + 11] = (13) | (13 << 5) | (14 << 10);
    PAL_OBJ[128 + 13] = (26) | (26 << 5) | (26 << 10);
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
    PAL_BG[48 + 1] = (30) | (24 << 5) | (8 << 10);   // title banner: straw gold
    PAL_BG[48 + 2] = (4) | (3 << 5) | (2 << 10);      // outline: near black
    PAL_BG[48 + 3] = (9) | (5 << 5) | (3 << 10);      // shadow: dark brown
    copy32(CHARBLOCK(1) + BANNER_BASE * 8, banner_tiles, sizeof(banner_tiles) / 4);
    sound_init();
    stats_load();
    new_run();
    if (TITLE_SKIP != TITLE_SKIP_MAGIC) title_screen();

    REG_DISPCNT = 0x0000 | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);   // mode 0, 1D OBJ, BG0, BG1, OBJ

    for (;;) {
        vsync();
        sound_vblank();
        oam_flush();
        sound_mix();
        pick_song();
        music_tick();

        poll_keys();
        frame++;
        rng_state += keys_now;
        (void)rng;

        if (dev_open) {
            update_dev();
            oam_begin();   // no sprites over the dev window
            oam_end();
            continue;
        }
        if (dev_combo()) { open_dev(); continue; }
        dev_tick();
        if (title_on) {
            if (!update_title()) {
                oam_begin();
                if (!stats_open) draw_title_crows();
                oam_end();
            }
            continue;
        }
        if (game_over) {
            if (HIT(KEY_START)) title_screen();
            else if (((frame >> 5) & 1) != ((frame - 1) >> 5 & 1))   // blink PRESS START
                put_text(9, END_START_ROW, (frame >> 5) & 1 ? "           " : "PRESS START");
            oam_begin();   // no sprites over the end window
            oam_end();
            continue;
        }
        if (shop_open) {
            update_shop();
            oam_begin();   // only the fireflies; they sit behind the window
            if (shop_open && !shop_dawn) draw_fireflies();   // fireflies only at night
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
        update_cannons();
        update_crops();
        update_message();
        update_notice();
        check_game_over();

        oam_begin();
        if (pdir != DIR_UP) draw_sword();   // in front of the farmer
        draw_farmer();
        if (pdir == DIR_UP) draw_sword();   // behind him when he faces away
        draw_crows();
        draw_shots();
        draw_cannons();
        draw_marker();
        oam_end();
    }
}
