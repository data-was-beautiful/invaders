/*
 * ALIEN INVASION - Space Invaders Clone
 * Built with SDL2 (no external asset files required - everything is drawn in code)
 *
 * ─── COMPILE ─────────────────────────────────────────────────────────────────
 *   gcc -O2 -o alien_invasion alien_invasion.c $(sdl2-config --cflags --libs) -lm
 *
 * Or without sdl2-config:
 *   gcc -O2 -o alien_invasion alien_invasion.c -I/usr/include/SDL2 -lSDL2 -lm
 *
 * ─── CONTROLS ────────────────────────────────────────────────────────────────
 *   LEFT / RIGHT   Move your ship
 *   SPACE          Fire bullet
 *   ENTER          Activate pulse weapon (when available)
 *   P              Pause / Unpause
 *   Q              Quit to title screen
 *
 * ─── HOW TO PLAY ─────────────────────────────────────────────────────────────
 *   Destroy all aliens before they reach you. Hide behind the three bases.
 *   You have 3 lives. Alien bombs and the flying saucer can kill you.
 *   Kill all aliens to advance; you earn a bonus life each wave.
 *   A pulse weapon is randomly granted each wave – it blasts a large shockwave
 *   destroying many aliens at once but also damages your bases.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  CONSTANTS                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

#define SCREEN_W        800
#define SCREEN_H        600
#define FPS             60
#define FRAME_MS        (1000 / FPS)

#define HUD_H           36
#define LIVES_START     3

/* Alien grid */
#define ALIEN_COLS      11
#define ALIEN_ROWS      5
#define ALIEN_W         36
#define ALIEN_H         24
#define ALIEN_PAD_X     14
#define ALIEN_PAD_Y     20
#define ALIEN_START_X   80
#define ALIEN_START_Y   80

/* Ship */
#define SHIP_W          40
#define SHIP_H          24
#define SHIP_Y          (SCREEN_H - 60)
#define SHIP_SPEED      4

/* Bullets */
#define BULLET_W        3
#define BULLET_H        14
#define BULLET_SPEED    10
#define ALIEN_BULLET_W  4
#define ALIEN_BULLET_H  8
#define ALIEN_BULLET_SPEED  4
#define MAX_ALIEN_BULLETS   6

/* Bases */
#define BASE_COUNT      3
#define BASE_W          64
#define BASE_H          40
#define BASE_Y          (SCREEN_H - 110)
#define BASE_BLOCK_W    8
#define BASE_BLOCK_H    8
#define BASE_COLS       (BASE_W  / BASE_BLOCK_W)
#define BASE_ROWS       (BASE_H  / BASE_BLOCK_H)

/* Saucer */
#define SAUCER_W        52
#define SAUCER_H        20
#define SAUCER_Y        (HUD_H + 14)
#define SAUCER_SPEED    2

/* Scoring */
#define POINTS_ALIEN    50
#define POINTS_SAUCER   200

/* Pulse weapon */
#define PULSE_RADIUS_MAX    220
#define PULSE_SPEED         9

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TYPES                                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER,
    STATE_WAVE_CLEAR
} GameState;

typedef struct {
    int   type;   /* 0=squid  1=crab  2=octopus */
    float x, y;
    int   alive;
    int   anim;   /* 0 or 1 */
} Alien;

typedef struct {
    float x, y;
    int   active;
    int   dx;     /* +1 left→right, -1 right→left */
} Saucer;

typedef struct {
    float x, y;
    int   active;
} Bullet;

typedef struct {
    float x, y;
    int   active;
} AlienBullet;

typedef struct {
    int   blocks[BASE_ROWS][BASE_COLS]; /* 0=intact 1=cracked 2=gone */
    float x, y;
} Base;

typedef struct {
    float x, y;
    float radius;
    int   active;
} Pulse;

/* Title-screen atmosphere particles */
typedef struct {
    float x, y, vx, vy;
    int   type;       /* 0=bullet 1=alien */
    int   atype;      /* alien type when type==1 */
    float life, maxlife;
} Particle;

#define MAX_PARTICLES 48

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  GLOBALS                                                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

static SDL_Window   *g_win = NULL;
static SDL_Renderer *g_ren = NULL;

static GameState g_state  = STATE_TITLE;
static int  g_score       = 0;
static int  g_lives       = LIVES_START;
static int  g_sound_on    = 1;
static int  g_title_sel   = 0;  /* 0=Play 1=Sound */
static int  g_title_anim  = 0;

/* Aliens */
static Alien  g_aliens[ALIEN_ROWS][ALIEN_COLS];
static float  g_alien_dx    = 1.0f;
static float  g_alien_speed = 0.5f;
static int    g_alien_drop  = 0;
static int    g_anim_timer  = 0;
static int    g_alive_count = 0;

/* Ship */
static float  g_ship_x;
static int    g_expl_timer = 0;   /* death flash frames */

/* Bullets */
static Bullet      g_bullet = {0};
static AlienBullet g_abul[MAX_ALIEN_BULLETS];
static int         g_alien_fire_timer = 0;

/* Bases */
static Base   g_bases[BASE_COUNT];

/* Saucer */
static Saucer g_saucer = {0};
static int    g_saucer_timer      = 0;
static int    g_saucer_bomb_timer = 0;

/* Pulse */
static Pulse  g_pulse    = {0};
static int    g_has_pulse = 0;
static int    g_pulse_hud = 0;  /* blink timer for HUD notification */

/* Timers */
static int    g_gameover_timer = 0;
static int    g_waveclear_timer = 0;

/* Title particles */
static Particle g_parts[MAX_PARTICLES];

/* Audio */
static SDL_AudioDeviceID g_audio = 0;
static struct { float phase, freq; int left; } g_tone = {0};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  AUDIO (software sine beeps – no external library needed)                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    Sint16 *buf = (Sint16*)stream;
    int n = len / 2;
    for (int i = 0; i < n; i++) {
        if (g_tone.left > 0) {
            float env = (g_tone.left > 200) ? 1.0f : g_tone.left / 200.0f;
            buf[i] = (Sint16)(sinf(g_tone.phase * 6.28318f) * 7000.0f * env);
            g_tone.phase += g_tone.freq / 44100.0f;
            if (g_tone.phase > 1.0f) g_tone.phase -= 1.0f;
            g_tone.left--;
        } else {
            buf[i] = 0;
        }
    }
}

static void beep(float freq, float secs)
{
    if (!g_sound_on || g_audio == 0) return;
    SDL_LockAudioDevice(g_audio);
    g_tone.freq  = freq;
    g_tone.left  = (int)(44100.0f * secs);
    SDL_UnlockAudioDevice(g_audio);
}

static void init_audio(void)
{
    SDL_AudioSpec want = {0};
    want.freq     = 44100;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = audio_cb;
    g_audio = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (g_audio > 0) SDL_PauseAudioDevice(g_audio, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  DRAWING PRIMITIVES                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void col(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{ SDL_SetRenderDrawColor(g_ren, r, g, b, a); }

static void rect(int x, int y, int w, int h)
{ SDL_Rect r = {x,y,w,h}; SDL_RenderFillRect(g_ren, &r); }

static void blend_on(void)
{ SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND); }

static void blend_off(void)
{ SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE); }

/* ─── 5×7 pixel font ────────────────────────────────────────────────────── */
/* One byte per row (5 bits used), 7 rows per glyph, ASCII 0x20–0x5A        */

static const Uint8 FONT[][7] = {
/* ' ' */  {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '!' */  {0x04,0x04,0x04,0x04,0x00,0x04,0x00},
/* '"' */  {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
/* '#' */  {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
/* '$' */  {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
/* '%' */  {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
/* '&' */  {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
/* ''' */  {0x06,0x04,0x08,0x00,0x00,0x00,0x00},
/* '(' */  {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
/* ')' */  {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
/* '*' */  {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00},
/* '+' */  {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
/* ',' */  {0x00,0x00,0x00,0x00,0x06,0x04,0x08},
/* '-' */  {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
/* '.' */  {0x00,0x00,0x00,0x00,0x00,0x06,0x00},
/* '/' */  {0x00,0x01,0x02,0x04,0x08,0x10,0x00},
/* '0' */  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
/* '1' */  {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
/* '2' */  {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
/* '3' */  {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
/* '4' */  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
/* '5' */  {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
/* '6' */  {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
/* '7' */  {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
/* '8' */  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
/* '9' */  {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
/* ':' */  {0x00,0x06,0x00,0x00,0x06,0x00,0x00},
/* ';' */  {0x00,0x06,0x00,0x00,0x06,0x04,0x08},
/* '<' */  {0x02,0x04,0x08,0x10,0x08,0x04,0x02},
/* '=' */  {0x00,0x1F,0x00,0x00,0x1F,0x00,0x00},
/* '>' */  {0x08,0x04,0x02,0x01,0x02,0x04,0x08},
/* '?' */  {0x0E,0x11,0x01,0x06,0x04,0x00,0x04},
/* '@' */  {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E},
/* 'A' */  {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
/* 'B' */  {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
/* 'C' */  {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
/* 'D' */  {0x1E,0x09,0x09,0x09,0x09,0x09,0x1E},
/* 'E' */  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
/* 'F' */  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
/* 'G' */  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
/* 'H' */  {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
/* 'I' */  {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
/* 'J' */  {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
/* 'K' */  {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
/* 'L' */  {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
/* 'M' */  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
/* 'N' */  {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
/* 'O' */  {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
/* 'P' */  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
/* 'Q' */  {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
/* 'R' */  {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
/* 'S' */  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
/* 'T' */  {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
/* 'U' */  {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
/* 'V' */  {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
/* 'W' */  {0x11,0x11,0x15,0x15,0x15,0x15,0x0A},
/* 'X' */  {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
/* 'Y' */  {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
/* 'Z' */  {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
};

static void draw_glyph(int px, int py, char c, int sc,
                       Uint8 r, Uint8 g, Uint8 b)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 0x20 || c > 0x5A) c = 0x20;
    const Uint8 *gl = FONT[(int)(c - 0x20)];
    col(r, g, b, 255);
    for (int row = 0; row < 7; row++)
        for (int bit = 0; bit < 5; bit++)
            if (gl[row] & (0x10 >> bit))
                rect(px + bit*sc, py + row*sc, sc, sc);
}

static void draw_str(int px, int py, const char *s, int sc,
                     Uint8 r, Uint8 g, Uint8 b)
{
    for (; *s; s++, px += 6*sc)
        draw_glyph(px, py, *s, sc, r, g, b);
}

static void draw_cstr(int y, const char *s, int sc,
                      Uint8 r, Uint8 g, Uint8 b)
{
    int x = (SCREEN_W - (int)strlen(s) * 6 * sc) / 2;
    draw_str(x, y, s, sc, r, g, b);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SPRITE DRAWING                                                             */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* Player ship: centred at (cx, cy), scale s */
static void draw_ship(int cx, int cy, int s, Uint8 r, Uint8 g, Uint8 b)
{
    col(r, g, b, 255);
    rect(cx - 10*s, cy + 3*s,  20*s, 4*s);   /* base hull */
    rect(cx -  7*s, cy,        14*s, 3*s);   /* mid body  */
    rect(cx -  3*s, cy - 3*s,   6*s, 3*s);   /* cockpit   */
    rect(cx -  1*s, cy - 5*s,   2*s, 2*s);   /* cannon    */
}

/* Aliens: centred at (cx,cy), scale s, frame 0 or 1 */
static void draw_alien(int cx, int cy, int type, int frm, int s,
                       Uint8 r, Uint8 g, Uint8 b)
{
    col(r, g, b, 255);
    if (type == 0) { /* SQUID */
        rect(cx - 3*s, cy - 4*s,  6*s,  s);
        rect(cx - 5*s, cy - 3*s, 10*s, 2*s);
        rect(cx - 6*s, cy - 1*s, 12*s, 3*s);
        if (frm == 0) {
            rect(cx - 6*s, cy + 2*s, 2*s, 2*s);
            rect(cx - 2*s, cy + 2*s, 2*s, 2*s);
            rect(cx +   s, cy + 2*s, 2*s, 2*s);
            rect(cx + 4*s, cy + 2*s, 2*s, 2*s);
        } else {
            rect(cx - 4*s, cy + 2*s, 2*s, 2*s);
            rect(cx,       cy + 2*s, 2*s, 2*s);
            rect(cx + 3*s, cy + 2*s, 2*s, 2*s);
        }
        col(0,0,0,255);
        rect(cx - 4*s, cy, 2*s, s);
        rect(cx + 2*s, cy, 2*s, s);
    } else if (type == 1) { /* CRAB */
        rect(cx -   s, cy - 4*s,  2*s, 2*s);
        rect(cx - 5*s, cy - 2*s, 10*s, 2*s);
        rect(cx - 6*s, cy,       12*s, 2*s);
        if (frm == 0) {
            rect(cx - 6*s, cy + 2*s, 2*s, 2*s);
            rect(cx - 3*s, cy + 2*s, 2*s, 2*s);
            rect(cx + 2*s, cy + 2*s, 2*s, 2*s);
            rect(cx + 4*s, cy + 2*s, 2*s, 2*s);
        } else {
            rect(cx - 8*s, cy + 2*s, 2*s, 2*s);
            rect(cx - 2*s, cy + 2*s, 2*s, 2*s);
            rect(cx +   s, cy + 2*s, 2*s, 2*s);
            rect(cx + 6*s, cy + 2*s, 2*s, 2*s);
        }
        col(0,0,0,255);
        rect(cx - 3*s, cy - s, 2*s, s);
        rect(cx +   s, cy - s, 2*s, s);
    } else { /* OCTOPUS */
        rect(cx - 3*s, cy - 4*s,  6*s, 2*s);
        rect(cx - 5*s, cy - 2*s, 10*s, 2*s);
        rect(cx - 6*s, cy,       12*s, 2*s);
        if (frm == 0) {
            rect(cx - 6*s, cy + 2*s, 4*s, 2*s);
            rect(cx -   s, cy + 2*s, 2*s, 2*s);
            rect(cx + 2*s, cy + 2*s, 4*s, 2*s);
        } else {
            rect(cx - 4*s, cy + 2*s, 2*s, 2*s);
            rect(cx -   s, cy + 2*s, 2*s, 2*s);
            rect(cx + 2*s, cy + 2*s, 2*s, 2*s);
            rect(cx + 4*s, cy + 2*s, 2*s, 2*s);
        }
        col(0,0,0,255);
        rect(cx - 3*s, cy - s, 2*s, s);
        rect(cx +   s, cy - s, 2*s, s);
    }
    col(r, g, b, 255);
}

static void draw_saucer(int cx, int cy, int s, Uint8 r, Uint8 g, Uint8 b)
{
    col(r, g, b, 255);
    rect(cx - 8*s, cy,       16*s, 2*s);
    rect(cx - 5*s, cy - 2*s, 10*s, 2*s);
    rect(cx - 3*s, cy - 4*s,  6*s, 2*s);
    rect(cx - 8*s, cy + 2*s, 16*s, 2*s);
    col(0,0,0,255);
    for (int i = -5; i <= 4; i += 3)
        rect(cx + i*s, cy + s, s, s);
}

static void draw_explosion(int cx, int cy, int age)
{
    static const int ox[] = {0,-1,1,0,-2,2,-1,1};
    static const int oy[] = {-2,0,0,2,-1,1,1,-1};
    col(255, 200, 50, 255);
    int r = 3 + age * 2;
    for (int i = 0; i < 8; i++)
        rect(cx + ox[i]*r - 2, cy + oy[i]*r - 2, 5, 5);
    col(255, 80, 0, 200);
    rect(cx-3, cy-3, 7, 7);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  COLLISION                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int overlap(float ax, float ay, int aw, int ah,
                   float bx, float by, int bw, int bh)
{
    return ax < bx+bw && ax+aw > bx && ay < by+bh && ay+ah > by;
}

/* Damage base blocks in the region of a hit */
static void base_damage(float hx, float hy, int hw, int hh, int dmg)
{
    for (int i = 0; i < BASE_COUNT; i++) {
        Base *b = &g_bases[i];
        if (!overlap(hx,hy,hw,hh, b->x,b->y,BASE_W,BASE_H)) continue;
        for (int r = 0; r < BASE_ROWS; r++) {
            for (int c = 0; c < BASE_COLS; c++) {
                if (b->blocks[r][c] >= 2) continue;
                float blx = b->x + c * BASE_BLOCK_W;
                float bly = b->y + r * BASE_BLOCK_H;
                if (overlap(hx,hy,hw,hh, blx,bly,BASE_BLOCK_W,BASE_BLOCK_H)) {
                    b->blocks[r][c] += dmg;
                    if (b->blocks[r][c] > 2) b->blocks[r][c] = 2;
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  GAME INITIALIZATION                                                        */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void init_aliens(void)
{
    g_alive_count = 0;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            Alien *a = &g_aliens[r][c];
            a->x = (float)(ALIEN_START_X + c * (ALIEN_W + ALIEN_PAD_X));
            a->y = (float)(ALIEN_START_Y + r * (ALIEN_H + ALIEN_PAD_Y));
            a->alive = 1; a->anim = 0;
            a->type = (r < 2) ? 0 : (r < 4) ? 1 : 2;
            g_alive_count++;
        }
    }
    g_alien_dx   = 1.0f;
    g_alien_drop = 0;
    g_anim_timer = 0;
}

static void init_bases(void)
{
    int gap = SCREEN_W / (BASE_COUNT + 1);
    for (int i = 0; i < BASE_COUNT; i++) {
        Base *b = &g_bases[i];
        b->x = (float)(gap * (i+1) - BASE_W/2);
        b->y = (float)BASE_Y;
        for (int r = 0; r < BASE_ROWS; r++)
            for (int c = 0; c < BASE_COLS; c++)
                b->blocks[r][c] = 0;
        /* Arch cutout at bottom centre */
        for (int c = 2; c < BASE_COLS-2; c++) {
            b->blocks[BASE_ROWS-1][c] = 2;
            b->blocks[BASE_ROWS-2][c] = 2;
        }
    }
}

static void grant_pulse(void)
{
    g_has_pulse = (rand() % 2 == 0);
    g_pulse_hud = g_has_pulse ? FPS * 4 : 0;
}

static void new_game(void)
{
    g_score = 0; g_lives = LIVES_START;
    g_ship_x = SCREEN_W / 2.0f;
    g_bullet.active = 0;
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) g_abul[i].active = 0;
    g_saucer.active = 0;
    g_saucer_timer  = 480 + rand() % 480;
    g_pulse.active  = 0;
    g_alien_speed   = 0.5f;
    g_alien_fire_timer = 0;
    init_aliens();
    init_bases();
    grant_pulse();
    g_state = STATE_PLAYING;
}

static void next_wave(void)
{
    if (g_lives < 9) g_lives++;
    g_bullet.active = 0;
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) g_abul[i].active = 0;
    g_saucer.active = 0;
    g_saucer_timer  = 480 + rand() % 480;
    g_pulse.active  = 0;
    g_alien_speed  *= 1.18f;
    if (g_alien_speed > 3.5f) g_alien_speed = 3.5f;
    g_alien_fire_timer = 0;
    init_aliens();
    init_bases();
    grant_pulse();
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TITLE PARTICLES                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void spawn_particle(Particle *p)
{
    int edge = rand() % 4;
    if      (edge == 0) { p->x = (float)(rand()%SCREEN_W); p->y = -20; p->vx = (rand()%100-50)/80.0f; p->vy = 0.4f+(rand()%60)/100.0f; }
    else if (edge == 1) { p->x = (float)(rand()%SCREEN_W); p->y = SCREEN_H+20; p->vx = (rand()%100-50)/80.0f; p->vy = -(0.4f+(rand()%60)/100.0f); }
    else if (edge == 2) { p->y = (float)(rand()%SCREEN_H); p->x = -20; p->vy = (rand()%100-50)/80.0f; p->vx = 0.4f+(rand()%60)/100.0f; }
    else                { p->y = (float)(rand()%SCREEN_H); p->x = SCREEN_W+20; p->vy = (rand()%100-50)/80.0f; p->vx = -(0.4f+(rand()%60)/100.0f); }
    p->type = rand() % 2;
    p->atype = rand() % 3;
    p->maxlife = p->life = (float)(120 + rand() % 240);
}

static void init_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        spawn_particle(&g_parts[i]);
        /* scatter initial positions so screen isn't empty */
        g_parts[i].x = (float)(rand() % SCREEN_W);
        g_parts[i].y = (float)(rand() % SCREEN_H);
        g_parts[i].life = g_parts[i].maxlife * ((rand()%100)/100.0f);
    }
}

static void update_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_parts[i];
        p->x += p->vx; p->y += p->vy;
        p->life -= 1.0f;
        if (p->life <= 0) spawn_particle(p);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  GAME UPDATE                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void update_game(const Uint8 *keys)
{
    /* Ship */
    if (keys[SDL_SCANCODE_LEFT])  { g_ship_x -= SHIP_SPEED; if (g_ship_x < SHIP_W/2) g_ship_x = SHIP_W/2; }
    if (keys[SDL_SCANCODE_RIGHT]) { g_ship_x += SHIP_SPEED; if (g_ship_x > SCREEN_W-SHIP_W/2) g_ship_x = SCREEN_W-SHIP_W/2.0f; }

    /* Player bullet movement */
    if (g_bullet.active) {
        g_bullet.y -= BULLET_SPEED;
        if (g_bullet.y < HUD_H) { g_bullet.active = 0; goto done_pbullet; }

        /* vs aliens */
        for (int r = 0; r < ALIEN_ROWS && g_bullet.active; r++) {
            for (int c = 0; c < ALIEN_COLS && g_bullet.active; c++) {
                Alien *a = &g_aliens[r][c];
                if (!a->alive) continue;
                if (overlap(g_bullet.x-BULLET_W/2, g_bullet.y, BULLET_W, BULLET_H,
                            a->x-ALIEN_W/2, a->y-ALIEN_H/2, ALIEN_W, ALIEN_H)) {
                    a->alive = 0; g_alive_count--;
                    g_score += POINTS_ALIEN;
                    g_bullet.active = 0;
                    beep(880.0f, 0.05f);
                }
            }
        }
        /* vs saucer */
        if (g_bullet.active && g_saucer.active) {
            if (overlap(g_bullet.x-BULLET_W/2, g_bullet.y, BULLET_W, BULLET_H,
                        g_saucer.x-SAUCER_W/2, SAUCER_Y, SAUCER_W, SAUCER_H)) {
                g_saucer.active = 0; g_score += POINTS_SAUCER;
                g_bullet.active = 0;
                beep(1200.0f, 0.18f);
            }
        }
        /* vs bases */
        if (g_bullet.active) {
            for (int i = 0; i < BASE_COUNT; i++) {
                Base *b = &g_bases[i];
                if (overlap(g_bullet.x-BULLET_W/2, g_bullet.y, BULLET_W, BULLET_H,
                            b->x, b->y, BASE_W, BASE_H)) {
                    base_damage(g_bullet.x-BULLET_W/2, g_bullet.y, BULLET_W, BULLET_H, 1);
                    g_bullet.active = 0; break;
                }
            }
        }
    }
    done_pbullet:;

    /* Alien animation */
    g_anim_timer++;
    int anim_int = (int)(32.0f / (g_alien_speed + 0.6f));
    if (anim_int < 5) anim_int = 5;
    if (g_anim_timer >= anim_int) {
        g_anim_timer = 0;
        for (int r = 0; r < ALIEN_ROWS; r++)
            for (int c = 0; c < ALIEN_COLS; c++)
                g_aliens[r][c].anim ^= 1;
    }

    /* Alien bounds */
    float left = 9999, right = -9999;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (!g_aliens[r][c].alive) continue;
            float ax = g_aliens[r][c].x;
            if (ax - ALIEN_W/2 < left)  left  = ax - ALIEN_W/2;
            if (ax + ALIEN_W/2 > right) right = ax + ALIEN_W/2;
        }
    }

    /* Alien movement */
    if (g_alien_drop > 0) {
        for (int r = 0; r < ALIEN_ROWS; r++)
            for (int c = 0; c < ALIEN_COLS; c++)
                if (g_aliens[r][c].alive) g_aliens[r][c].y += 5.0f;
        g_alien_drop--;
    } else {
        int need_drop = 0;
        for (int r = 0; r < ALIEN_ROWS; r++)
            for (int c = 0; c < ALIEN_COLS; c++)
                if (g_aliens[r][c].alive) g_aliens[r][c].x += g_alien_dx * g_alien_speed;
        if (g_alien_dx > 0 && right + g_alien_dx*g_alien_speed > SCREEN_W-10) need_drop=1;
        if (g_alien_dx < 0 && left  + g_alien_dx*g_alien_speed < 10) need_drop=1;
        if (need_drop) { g_alien_dx = -g_alien_dx; g_alien_drop = 2; }
    }

    /* Aliens reaching ship level = instant death */
    for (int r = 0; r < ALIEN_ROWS; r++)
        for (int c = 0; c < ALIEN_COLS; c++)
            if (g_aliens[r][c].alive && g_aliens[r][c].y + ALIEN_H/2 >= SHIP_Y)
                g_lives = 0;

    /* Alien fire */
    g_alien_fire_timer++;
    int fire_int = (int)(70.0f / (g_alien_speed + 0.4f));
    if (fire_int < 12) fire_int = 12;
    if (g_alien_fire_timer >= fire_int) {
        g_alien_fire_timer = 0;
        for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
            if (g_abul[i].active) continue;
            for (int t = 0; t < 30; t++) {
                int rr = rand() % ALIEN_ROWS, rc = rand() % ALIEN_COLS;
                if (g_aliens[rr][rc].alive) {
                    g_abul[i].x = g_aliens[rr][rc].x;
                    g_abul[i].y = g_aliens[rr][rc].y + ALIEN_H/2;
                    g_abul[i].active = 1;
                    break;
                }
            }
            break;
        }
    }

    /* Alien bullet movement */
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        AlienBullet *ab = &g_abul[i];
        if (!ab->active) continue;
        ab->y += ALIEN_BULLET_SPEED;
        if (ab->y > SCREEN_H) { ab->active = 0; continue; }

        /* vs bases */
        int hit = 0;
        for (int bi = 0; bi < BASE_COUNT && !hit; bi++) {
            Base *b = &g_bases[bi];
            if (overlap(ab->x-ALIEN_BULLET_W/2, ab->y, ALIEN_BULLET_W, ALIEN_BULLET_H,
                        b->x, b->y, BASE_W, BASE_H)) {
                base_damage(ab->x-ALIEN_BULLET_W/2, ab->y, ALIEN_BULLET_W, ALIEN_BULLET_H, 1);
                ab->active = 0; hit = 1;
            }
        }
        if (hit) continue;

        /* vs player */
        if (g_expl_timer == 0 &&
            overlap(ab->x-ALIEN_BULLET_W/2, ab->y, ALIEN_BULLET_W, ALIEN_BULLET_H,
                    g_ship_x-SHIP_W/2, SHIP_Y, SHIP_W, SHIP_H)) {
            ab->active = 0;
            g_lives--;
            g_expl_timer = 45;
            beep(160.0f, 0.4f);
            if (g_lives <= 0) { g_state = STATE_GAME_OVER; g_gameover_timer = FPS*3; }
        }
    }

    /* Saucer spawn */
    if (!g_saucer.active) {
        g_saucer_timer--;
        if (g_saucer_timer <= 0) {
            g_saucer.active = 1;
            g_saucer.dx = (rand()%2) ? 1 : -1;
            g_saucer.x  = (g_saucer.dx > 0) ? -(float)(SAUCER_W/2)
                                              : (float)(SCREEN_W + SAUCER_W/2);
            g_saucer_bomb_timer = 60 + rand()%60;
            beep(440.0f, 0.08f);
        }
    }
    if (g_saucer.active) {
        g_saucer.x += g_saucer.dx * SAUCER_SPEED;
        if (g_saucer.x < -(float)SAUCER_W || g_saucer.x > SCREEN_W+(float)SAUCER_W) {
            g_saucer.active = 0;
            g_saucer_timer  = 480 + rand()%480;
        }
        g_saucer_bomb_timer--;
        if (g_saucer_bomb_timer <= 0) {
            g_saucer_bomb_timer = 50 + rand()%50;
            for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
                if (!g_abul[i].active) {
                    g_abul[i].x = g_saucer.x;
                    g_abul[i].y = (float)(SAUCER_Y + SAUCER_H);
                    g_abul[i].active = 1;
                    break;
                }
            }
        }
    }

    /* Pulse weapon */
    if (g_pulse.active) {
        g_pulse.radius += PULSE_SPEED;
        /* Destroy aliens inside radius */
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                Alien *a = &g_aliens[r][c];
                if (!a->alive) continue;
                float dx = a->x - g_pulse.x, dy = a->y - g_pulse.y;
                if (sqrtf(dx*dx+dy*dy) < g_pulse.radius) {
                    a->alive = 0; g_alive_count--;
                    g_score += POINTS_ALIEN;
                }
            }
        }
        /* Damage bases (expanding ring passes over them) */
        for (int i = 0; i < BASE_COUNT; i++) {
            Base *b = &g_bases[i];
            float bdx = b->x + BASE_W/2 - g_pulse.x;
            float bdy = b->y + BASE_H/2 - g_pulse.y;
            float dist = sqrtf(bdx*bdx + bdy*bdy);
            if (dist < g_pulse.radius && dist > g_pulse.radius - PULSE_SPEED - 20) {
                for (int r = 0; r < BASE_ROWS; r++)
                    for (int c = 0; c < BASE_COLS; c++)
                        if (b->blocks[r][c] < 2 && rand()%10 < 7)
                            b->blocks[r][c] = 2;
            }
        }
        if (g_pulse.radius >= PULSE_RADIUS_MAX) g_pulse.active = 0;
    }
    if (g_pulse_hud > 0) g_pulse_hud--;

    /* Wave clear */
    if (g_alive_count <= 0 && g_state == STATE_PLAYING) {
        g_state = STATE_WAVE_CLEAR;
        g_waveclear_timer = FPS * 2;
        beep(660.0f, 0.5f);
    }

    if (g_expl_timer > 0) g_expl_timer--;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  RENDERING                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void render_stars(unsigned int seed)
{
    srand(seed);
    for (int i = 0; i < 100; i++) {
        int sx = rand() % SCREEN_W, sy = HUD_H + rand() % (SCREEN_H - HUD_H);
        Uint8 b = (Uint8)(50 + rand()%160);
        col(b, b, b+20, 255);
        SDL_RenderDrawPoint(g_ren, sx, sy);
    }
    srand((unsigned)time(NULL));
}

static void render_hud(void)
{
    /* Bar */
    col(0,0,18,255); rect(0,0,SCREEN_W,HUD_H);
    col(0,140,0,255); SDL_RenderDrawLine(g_ren, 0,HUD_H, SCREEN_W,HUD_H);

    /* Score */
    char buf[32]; snprintf(buf,sizeof(buf),"SCORE:%06d", g_score);
    draw_str(8, 7, buf, 2, 255,220,0);

    /* Life icons */
    int lx = SCREEN_W - 12;
    for (int i = 0; i < g_lives - 1 && i < 8; i++) {
        lx -= 26;
        draw_ship(lx, 18, 1, 80,220,80);
    }

    /* Pulse indicator */
    if (g_has_pulse) {
        int vis = (g_pulse_hud > 0) ? ((SDL_GetTicks()/200)%2) : 1;
        if (vis) draw_cstr(7, "* PULSE READY *", 1, 80,200,255); 
    }
}

static void render_bases(void)
{
    for (int i = 0; i < BASE_COUNT; i++) {
        Base *b = &g_bases[i];
        for (int r = 0; r < BASE_ROWS; r++) {
            for (int c = 0; c < BASE_COLS; c++) {
                int st = b->blocks[r][c];
                if (st >= 2) continue;
                int bx = (int)b->x + c*BASE_BLOCK_W;
                int by = (int)b->y + r*BASE_BLOCK_H;
                if (st == 0) col(0,210,60,255); else col(0,120,35,255);
                rect(bx, by, BASE_BLOCK_W-1, BASE_BLOCK_H-1);
            }
        }
    }
}

static void render_pulse(void)
{
    if (!g_pulse.active) return;
    blend_on();
    float frac = g_pulse.radius / PULSE_RADIUS_MAX;
    Uint8 alpha = (Uint8)(220 * (1.0f - frac));
    /* Draw 4 concentric rings for glow */
    for (int ring = 0; ring < 5; ring++) {
        float r = g_pulse.radius - ring*5;
        if (r < 0) continue;
        col(80, 160, 255, (Uint8)(alpha / (ring+1)));
        int steps = 64;
        for (int i = 0; i < steps; i++) {
            float a1 = (float)i     / steps * 6.28318f;
            float a2 = (float)(i+1) / steps * 6.28318f;
            SDL_RenderDrawLine(g_ren,
                (int)(g_pulse.x + cosf(a1)*r), (int)(g_pulse.y + sinf(a1)*r),
                (int)(g_pulse.x + cosf(a2)*r), (int)(g_pulse.y + sinf(a2)*r));
        }
    }
    blend_off();
}

static void render_game_scene(void)
{
    col(0,0,10,255); SDL_RenderClear(g_ren);
    render_stars(42);
    render_hud();
    render_bases();

    /* Aliens */
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            Alien *a = &g_aliens[r][c];
            if (!a->alive) continue;
            Uint8 ar=100,ag=255,ab=100;
            if (a->type==1) { ar=100;ag=100;ab=255; }
            if (a->type==2) { ar=255;ag=100;ab=100; }
            draw_alien((int)a->x,(int)a->y,a->type,a->anim,1,ar,ag,ab);
        }
    }

    /* Saucer */
    if (g_saucer.active)
        draw_saucer((int)g_saucer.x, SAUCER_Y+SAUCER_H/2, 1, 255,60,210);

    /* Player bullet */
    if (g_bullet.active) {
        col(255,255,80,255);
        rect((int)(g_bullet.x-BULLET_W/2),(int)g_bullet.y,BULLET_W,BULLET_H);
    }

    /* Alien bullets */
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        AlienBullet *ab = &g_abul[i];
        if (!ab->active) continue;
        col(255,60,60,255);
        rect((int)(ab->x-ALIEN_BULLET_W/2),(int)ab->y,ALIEN_BULLET_W,ALIEN_BULLET_H);
    }

    render_pulse();

    /* Ship or explosion */
    if (g_expl_timer > 0)
        draw_explosion((int)g_ship_x, SHIP_Y+SHIP_H/2, (45-g_expl_timer)/6);
    else if (g_lives > 0)
        draw_ship((int)g_ship_x, SHIP_Y+SHIP_H/2, 2, 100,255,100);

    /* Ground line */
    col(0,160,0,255);
    SDL_RenderDrawLine(g_ren, 0,SCREEN_H-20, SCREEN_W,SCREEN_H-20);
}

static void render_playing(void)   { render_game_scene(); }

static void render_paused(void)
{
    render_game_scene();
    blend_on(); col(0,0,0,150); rect(0,0,SCREEN_W,SCREEN_H); blend_off();
    draw_cstr(SCREEN_H/2 - 24, "PAUSED", 4, 255,255,0);
    draw_cstr(SCREEN_H/2 + 20, "PRESS P TO CONTINUE  Q TO QUIT", 2, 180,180,180);
}

static void render_wave_clear(void)
{
    render_game_scene();
    blend_on(); col(0,0,0,130); rect(0,0,SCREEN_W,SCREEN_H); blend_off();
    draw_cstr(SCREEN_H/2 - 30, "WAVE CLEARED!", 3, 100,255,100);
    draw_cstr(SCREEN_H/2 + 14, "BONUS LIFE!", 2, 255,220,60);
}

static void render_game_over(void)
{
    render_game_scene();
    blend_on(); col(0,0,0,160); rect(0,0,SCREEN_W,SCREEN_H); blend_off();
    draw_cstr(SCREEN_H/2 - 50, "GAME OVER", 5, 255,40,40);
    char buf[32]; snprintf(buf,sizeof(buf),"FINAL SCORE: %d", g_score);
    draw_cstr(SCREEN_H/2 + 20, buf, 2, 255,220,60);
    draw_cstr(SCREEN_H/2 + 58, "RETURNING TO TITLE...", 2, 160,160,160);
}

static void render_title(void)
{
    /* Deep-space background */
    col(0,0,14,255); SDL_RenderClear(g_ren);

    /* Stars */
    srand(77);
    for (int i = 0; i < 130; i++) {
        int sx=rand()%SCREEN_W, sy=rand()%SCREEN_H;
        Uint8 b=(Uint8)(50+rand()%180);
        col(b,b,(Uint8)SDL_min(255,b+40),255);
        SDL_RenderDrawPoint(g_ren,sx,sy);
        if (b>170) SDL_RenderDrawPoint(g_ren,sx+1,sy);
    }
    srand((unsigned)time(NULL));

    /* Background particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_parts[i];
        float fade = p->life / p->maxlife;
        if (p->type == 0) { /* bullet tracer */
            col(255,180,60,(Uint8)(180*fade));
            rect((int)p->x,(int)p->y,3,9);
        } else { /* alien silhouette */
            Uint8 r=50,g=50,b=50;
            if (p->atype==0) {r=30;g=100;b=30;}
            else if (p->atype==1) {r=30;g=30;b=100;}
            else {r=100;g=30;b=30;}
            draw_alien((int)p->x,(int)p->y,p->atype,(g_title_anim/22)%2,1,
                       (Uint8)(r*fade),(Uint8)(g*fade),(Uint8)(b*fade));
        }
    }

    /* Alien banner row */
    for (int c = 0; c < 9; c++) {
        int at = c % 3;
        int ax = 55 + c * 80, ay = 170;
        Uint8 ar=50,ag=160,ab=50;
        if (at==1){ar=50;ag=50;ab=160;} if (at==2){ar=160;ag=50;ab=50;}
        draw_alien(ax, ay, at, (g_title_anim/24)%2, 1, ar,ag,ab);
    }

    /* Decorative ship */
    draw_ship(SCREEN_W/2, 208, 3, 60,180,60);

    /* Saucer accents */
    draw_saucer(110, 236, 2, 180,50,160);
    draw_saucer(SCREEN_W-110, 236, 2, 180,50,160);

    /* Animated title */
    Uint8 pulse = (Uint8)(150 + 90*sinf(g_title_anim * 0.045f));
    /* Shadow */
    draw_cstr(42, "ALIEN INVASION", 5, 15,60,15);
    /* Main */
    draw_cstr(40, "ALIEN INVASION", 5, 60,pulse,60);

    /* Divider */
    col(0,110,0,255);
    SDL_RenderDrawLine(g_ren, 50,278, SCREEN_W-50,278);

    /* Menu items */
    int my = 298;
    if (g_title_sel==0) {
        col(0,70,0,180); rect(SCREEN_W/2-85,my-4,170,20);
        draw_cstr(my, "> PLAY GAME <", 2, 80,255,80);
    } else {
        draw_cstr(my, "PLAY GAME", 2, 140,140,140);
    }
    my += 42;
    char snd[28]; snprintf(snd,sizeof(snd),"SOUND: %s", g_sound_on?"ON":"OFF");
    if (g_title_sel==1) {
        col(0,70,0,180); rect(SCREEN_W/2-85,my-4,170,20);
        draw_cstr(my, snd, 2, 80,255,80);
    } else {
        draw_cstr(my, snd, 2, 140,140,140);
    }

    /* Controls strip */
    col(0,70,0,255); SDL_RenderDrawLine(g_ren,50,398,SCREEN_W-50,398);
    draw_cstr(410, "CONTROLS", 2, 160,160,50);
    draw_cstr(434, "ARROWS:MOVE   SPACE:FIRE   ENTER:PULSE", 1, 120,120,120);
    draw_cstr(448, "P:PAUSE   Q:QUIT TO TITLE", 1, 120,120,120);

    /* Score guide */
    col(0,70,0,255); SDL_RenderDrawLine(g_ren,50,468,SCREEN_W-50,468);
    draw_cstr(476, "POINT VALUES", 2, 140,140,50);
    {
        int px = SCREEN_W/2 - 180;
        draw_alien(px,      502, 0,0,1, 60,180,60);  draw_str(px+14, 496,"=50",  1, 180,180,180);
        draw_alien(px+80,   502, 1,0,1, 60,60,180);  draw_str(px+94, 496,"=50",  1, 180,180,180);
        draw_alien(px+160,  502, 2,0,1, 180,60,60);  draw_str(px+174,496,"=50",  1, 180,180,180);
        draw_saucer(px+268, 502, 1, 180,50,160);      draw_str(px+284,496,"=200", 1, 180,180,180);
    }

    draw_str(SCREEN_W-54, SCREEN_H-14, "V1.0", 1, 50,50,50);
    g_title_anim++;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  MAIN                                                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    g_win = SDL_CreateWindow("Alien Invasion",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!g_win) { fprintf(stderr, "Window: %s\n", SDL_GetError()); return 1; }

    g_ren = SDL_CreateRenderer(g_win, -1,
                               SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) { fprintf(stderr, "Renderer: %s\n", SDL_GetError()); return 1; }

    init_audio();
    init_particles();

    /* Edge-triggered key state */
    int k_up=0,k_dn=0,k_ret=0,k_p=0,k_q=0,k_sp=0;
    int pk_up,pk_dn,pk_ret,pk_p,pk_q,pk_sp;

    SDL_bool running = SDL_TRUE;
    while (running) {
        Uint32 t0 = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT) running = SDL_FALSE;

        const Uint8 *K = SDL_GetKeyboardState(NULL);
        /* Save prev, read current */
        pk_up=k_up; pk_dn=k_dn; pk_ret=k_ret;
        pk_p=k_p;   pk_q=k_q;   pk_sp=k_sp;
        k_up  = K[SDL_SCANCODE_UP];
        k_dn  = K[SDL_SCANCODE_DOWN];
        k_ret = K[SDL_SCANCODE_RETURN];
        k_p   = K[SDL_SCANCODE_P];
        k_q   = K[SDL_SCANCODE_Q];
        k_sp  = K[SDL_SCANCODE_SPACE];
        /* Rising edge */
#define PRESSED(k,pk) ((k) && !(pk))

        switch (g_state) {

        case STATE_TITLE:
            update_particles();
            if (PRESSED(k_up,pk_up) || PRESSED(k_dn,pk_dn)) {
                g_title_sel ^= 1; beep(550.0f,0.03f);
            }
            if (PRESSED(k_ret,pk_ret)) {
                if (g_title_sel==0) { beep(750.0f,0.1f); new_game(); }
                else                { g_sound_on ^= 1; beep(500.0f,0.04f); }
            }
            render_title();
            break;

        case STATE_PLAYING:
            if (PRESSED(k_q,pk_q))  { g_state=STATE_TITLE; break; }
            if (PRESSED(k_p,pk_p))  { g_state=STATE_PAUSED; break; }
            if (PRESSED(k_sp,pk_sp) && !g_bullet.active) {
                g_bullet.x=g_ship_x; g_bullet.y=(float)(SHIP_Y-BULLET_H);
                g_bullet.active=1; beep(1050.0f,0.04f);
            }
            if (PRESSED(k_ret,pk_ret) && g_has_pulse && !g_pulse.active) {
                g_pulse.x=g_ship_x; g_pulse.y=(float)(SHIP_Y+SHIP_H/2);
                g_pulse.radius=1.0f; g_pulse.active=1;
                g_has_pulse=0; beep(200.0f,0.7f);
            }
            update_game(K);
            render_playing();
            break;

        case STATE_PAUSED:
            if (PRESSED(k_p,pk_p)) g_state=STATE_PLAYING;
            if (PRESSED(k_q,pk_q)) g_state=STATE_TITLE;
            render_paused();
            break;

        case STATE_WAVE_CLEAR:
            g_waveclear_timer--;
            if (g_waveclear_timer <= 0) { next_wave(); g_state=STATE_PLAYING; }
            render_wave_clear();
            break;

        case STATE_GAME_OVER:
            g_gameover_timer--;
            if (g_gameover_timer<=0 || PRESSED(k_ret,pk_ret)) g_state=STATE_TITLE;
            render_game_over();
            break;
        }

        SDL_RenderPresent(g_ren);

        Uint32 elapsed = SDL_GetTicks() - t0;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }

    if (g_audio) SDL_CloseAudioDevice(g_audio);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    return 0;
}
