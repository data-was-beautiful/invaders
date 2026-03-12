/*
 * ALIEN INVASION - Space Invaders Clone  v2.0
 * Built with SDL2 (no external asset files – everything drawn & synthesised in code)
 *
 * ─── COMPILE ─────────────────────────────────────────────────────────────────
 *   gcc -O2 -o alien_invasion alien_invasion.c $(sdl2-config --cflags --libs) -lm
 *
 * ─── CONTROLS ────────────────────────────────────────────────────────────────
 *   LEFT / RIGHT   Move ship
 *   SPACE          Fire
 *   ENTER          Pulse weapon (when available)
 *   P              Pause / unpause
 *   Q              Quit to title
 *
 * ─── CHANGES IN v2.0 ─────────────────────────────────────────────────────────
 *   • Alien speed increases noticeably each wave (30% per wave, shown in HUD)
 *   • Background music: procedurally synthesised Space-Invaders-style march
 *     plus arpeggio layer; tempo speeds up with the aliens.  Toggle with the
 *     Music option on the title menu.
 *   • High scores: up to 10 entries saved to alien_invasion_scores.dat.
 *     If you make the table at game-over you get to enter your name (A-Z,
 *     0-9, space, hyphen; Backspace to correct; Enter to confirm).
 *   • Title menu now has High Scores option.
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
#define SAMPLE_RATE     44100

#define HUD_H           36
#define LIVES_START     3

#define ALIEN_COLS      11
#define ALIEN_ROWS      5
#define ALIEN_W         36
#define ALIEN_H         24
#define ALIEN_PAD_X     14
#define ALIEN_PAD_Y     20
#define ALIEN_START_X   80
#define ALIEN_START_Y   80

#define SHIP_W          40
#define SHIP_H          24
#define SHIP_Y          (SCREEN_H - 60)
#define SHIP_SPEED      4

#define BULLET_W        3
#define BULLET_H        14
#define BULLET_SPEED    10
#define ALIEN_BULLET_W  4
#define ALIEN_BULLET_H  8
#define ALIEN_BULLET_SPEED  4
#define MAX_ALIEN_BULLETS   6

#define BASE_COUNT      3
#define BASE_W          64
#define BASE_H          40
#define BASE_Y          (SCREEN_H - 110)
#define BASE_BLOCK_W    8
#define BASE_BLOCK_H    8
#define BASE_COLS       (BASE_W  / BASE_BLOCK_W)
#define BASE_ROWS       (BASE_H  / BASE_BLOCK_H)

#define SAUCER_W        52
#define SAUCER_H        20
#define SAUCER_Y        (HUD_H + 14)
#define SAUCER_SPEED    2

#define POINTS_ALIEN    50
#define POINTS_SAUCER   200

#define PULSE_RADIUS_MAX    220
#define PULSE_SPEED         9

/* Speed settings */
#define ALIEN_SPEED_START   0.5f
#define ALIEN_SPEED_MULT    1.30f
#define ALIEN_SPEED_CAP     5.0f

/* High scores */
#define MAX_SCORES      10
#define NAME_MAX        12
#define SCORES_FILE     "alien_invasion_scores.dat"

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TYPES                                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER,
    STATE_WAVE_CLEAR,
    STATE_NAME_ENTRY,
    STATE_HIGH_SCORES
} GameState;

typedef struct { int type; float x,y; int alive, anim; } Alien;
typedef struct { float x,y; int active,dx; } Saucer;
typedef struct { float x,y; int active; } Bullet;
typedef struct { float x,y; int active; } AlienBullet;
typedef struct { int blocks[BASE_ROWS][BASE_COLS]; float x,y; } Base;
typedef struct { float x,y,radius; int active; } Pulse;

typedef struct {
    float x,y,vx,vy;
    int   type, atype;
    float life, maxlife;
} Particle;

#define MAX_PARTICLES 48

typedef struct { char name[NAME_MAX+1]; int score; } ScoreEntry;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  GLOBALS                                                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

static SDL_Window   *g_win = NULL;
static SDL_Renderer *g_ren = NULL;

static GameState g_state     = STATE_TITLE;
static int  g_score          = 0;
static int  g_lives          = LIVES_START;
static int  g_sound_on       = 1;
static int  g_music_on       = 1;
static int  g_title_sel      = 0;   /* 0=Play 1=Sound 2=Music 3=HighScores */
static int  g_title_anim     = 0;
static int  g_wave_num       = 1;

static Alien  g_aliens[ALIEN_ROWS][ALIEN_COLS];
static float  g_alien_dx     = 1.0f;
static float  g_alien_speed  = ALIEN_SPEED_START;
static int    g_alien_drop   = 0;
static int    g_anim_timer   = 0;
static int    g_alive_count  = 0;

static float  g_ship_x;
static int    g_expl_timer   = 0;

static Bullet      g_bullet  = {0};
static AlienBullet g_abul[MAX_ALIEN_BULLETS];
static int         g_alien_fire_timer = 0;

static Base   g_bases[BASE_COUNT];
static Saucer g_saucer = {0};
static int    g_saucer_timer      = 0;
static int    g_saucer_bomb_timer = 0;

static Pulse  g_pulse    = {0};
static int    g_has_pulse = 0;
static int    g_pulse_hud = 0;

static int    g_gameover_timer  = 0;
static int    g_waveclear_timer = 0;

static Particle g_parts[MAX_PARTICLES];

/* High scores */
static ScoreEntry g_scores[MAX_SCORES];
static int        g_score_count   = 0;
static int        g_new_score_rank = -1;

/* Name entry */
static char g_entry_name[NAME_MAX+1];
static int  g_entry_len   = 0;
static int  g_entry_blink = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  AUDIO ENGINE                                                               */
/*  Two independent voices mixed in one callback, no SDL_mixer needed.        */
/*                                                                             */
/*  Voice 1 – SFX: one-shot sine beep, triggered by beep().                  */
/*  Voice 2 – Music: procedural 4-note march bass (classic Space Invaders     */
/*             rhythm) + minor-pentatonic arpeggio layer.  Tempo accelerates  */
/*             with the alien speed.                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

SDL_AudioDeviceID g_audio_dev = 0;

/* SFX voice */
static struct { float phase, freq; int left; } g_sfx = {0,0,0};

/* Music voice */
static const float MARCH_HZ[4] = { 160.0f, 120.0f, 100.0f, 120.0f };
static const float ARP_MULT[8] = {
    1.0f, 1.189f, 1.414f, 1.587f,
    2.0f, 1.587f, 1.414f, 1.189f
};

typedef struct {
    float phase;
    float freq;
    int   samples_left;
    int   note_idx;
    int   note_len;
    float arp_phase;
    float arp_freq;
    int   arp_step;
    int   arp_timer;
    int   arp_len;
} MusicVoice;

static MusicVoice g_music = {0};

static void music_set_tempo(float alien_speed)
{
    int base = SAMPLE_RATE / 4;
    int fast = SAMPLE_RATE / 15;
    float t = (alien_speed - ALIEN_SPEED_START) / (ALIEN_SPEED_CAP - ALIEN_SPEED_START);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    g_music.note_len = base + (int)(t * (fast - base));
    g_music.arp_len  = g_music.note_len / 2;
    if (g_music.arp_len < 1) g_music.arp_len = 1;
}

static void music_init(void)
{
    memset(&g_music, 0, sizeof(g_music));
    g_music.freq         = MARCH_HZ[0];
    g_music.arp_freq     = 220.0f * ARP_MULT[0];
    music_set_tempo(ALIEN_SPEED_START);
    g_music.samples_left = g_music.note_len;
    g_music.arp_timer    = g_music.arp_len;
}

/* Produce one mixed sample from both voices (range ±1) */
static float audio_mix_sample(void)
{
    float s = 0.0f;

    /* SFX voice – simple sine */
    if (g_sfx.left > 0) {
        float env = (g_sfx.left > 200) ? 1.0f : (float)g_sfx.left / 200.0f;
        s += sinf(g_sfx.phase * 6.28318f) * 0.55f * env;
        g_sfx.phase += g_sfx.freq / (float)SAMPLE_RATE;
        if (g_sfx.phase > 1.0f) g_sfx.phase -= 1.0f;
        g_sfx.left--;
    }

    /* Music voice */
    /* Advance march note sequencer */
    g_music.samples_left--;
    if (g_music.samples_left <= 0) {
        g_music.note_idx = (g_music.note_idx + 1) & 3;
        g_music.freq     = MARCH_HZ[g_music.note_idx];
        g_music.samples_left = g_music.note_len;
    }
    /* Advance arpeggio sequencer */
    g_music.arp_timer--;
    if (g_music.arp_timer <= 0) {
        g_music.arp_step = (g_music.arp_step + 1) & 7;
        g_music.arp_freq = 220.0f * ARP_MULT[g_music.arp_step];
        g_music.arp_timer = g_music.arp_len;
    }

    if (g_music_on) {
        /* Bass – square-ish with volume envelope */
        float env_pos = 1.0f - (float)g_music.samples_left / (float)g_music.note_len;
        float env;
        if      (env_pos < 0.05f) env = env_pos / 0.05f;
        else if (env_pos < 0.80f) env = 1.0f - (env_pos - 0.05f) / 0.75f * 0.35f;
        else                      env = 0.0f;

        g_music.phase += g_music.freq / (float)SAMPLE_RATE;
        if (g_music.phase > 1.0f) g_music.phase -= 1.0f;
        float bass = (g_music.phase < 0.5f ? 1.0f : -1.0f) * env;

        /* Arpeggio – triangle wave */
        g_music.arp_phase += g_music.arp_freq / (float)SAMPLE_RATE;
        if (g_music.arp_phase > 1.0f) g_music.arp_phase -= 1.0f;
        float tp  = g_music.arp_phase;
        float tri = (tp < 0.5f) ? (4.0f*tp - 1.0f) : (3.0f - 4.0f*tp);

        s += bass * 0.22f + tri * 0.12f;
    } else {
        /* Still advance the phase even when muted so phase doesn't jump */
        g_music.phase += g_music.freq / (float)SAMPLE_RATE;
        if (g_music.phase > 1.0f) g_music.phase -= 1.0f;
        g_music.arp_phase += g_music.arp_freq / (float)SAMPLE_RATE;
        if (g_music.arp_phase > 1.0f) g_music.arp_phase -= 1.0f;
    }

    return s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
}

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    Sint16 *buf = (Sint16*)stream;
    int n = len / 2;
    for (int i = 0; i < n; i++)
        buf[i] = (Sint16)(audio_mix_sample() * 28000.0f);
}

static void beep(float freq, float secs)
{
    if (!g_sound_on || g_audio_dev == 0) return;
    SDL_LockAudioDevice(g_audio_dev);
    g_sfx.freq  = freq;
    g_sfx.left  = (int)((float)SAMPLE_RATE * secs);
    SDL_UnlockAudioDevice(g_audio_dev);
}

static void init_audio(void)
{
    music_init();
    SDL_AudioSpec want = {0};
    want.freq     = SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = audio_cb;
    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (g_audio_dev > 0) SDL_PauseAudioDevice(g_audio_dev, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  DRAWING PRIMITIVES                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void col(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{ SDL_SetRenderDrawColor(g_ren, r, g, b, a); }

static void rect(int x, int y, int w, int h)
{ SDL_Rect r={x,y,w,h}; SDL_RenderFillRect(g_ren,&r); }

static void blend_on(void)  { SDL_SetRenderDrawBlendMode(g_ren,SDL_BLENDMODE_BLEND); }
static void blend_off(void) { SDL_SetRenderDrawBlendMode(g_ren,SDL_BLENDMODE_NONE);  }

/* ─── 5×7 bitmap pixel font (ASCII 0x20–0x5A) ──────────────────────────── */
static const Uint8 FONT[][7] = {
/*' '*/{0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*'!'*/{0x04,0x04,0x04,0x04,0x00,0x04,0x00},
/*'"'*/{0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, /*'#'*/{0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
/*'$'*/{0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, /*'%'*/{0x18,0x19,0x02,0x04,0x08,0x13,0x03},
/*'&'*/{0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, /*'''*/{0x06,0x04,0x08,0x00,0x00,0x00,0x00},
/*'('*/{0x02,0x04,0x08,0x08,0x08,0x04,0x02}, /*')'*/{0x08,0x04,0x02,0x02,0x02,0x04,0x08},
/*'*'*/{0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00}, /*'+'*/{0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
/*','*/{0x00,0x00,0x00,0x00,0x06,0x04,0x08}, /*'-'*/{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
/*'.'*/{0x00,0x00,0x00,0x00,0x00,0x06,0x00}, /*'/'*/{0x00,0x01,0x02,0x04,0x08,0x10,0x00},
/*'0'*/{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /*'1'*/{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
/*'2'*/{0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, /*'3'*/{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
/*'4'*/{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /*'5'*/{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
/*'6'*/{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /*'7'*/{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
/*'8'*/{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /*'9'*/{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
/*':'*/{0x00,0x06,0x00,0x00,0x06,0x00,0x00}, /*';'*/{0x00,0x06,0x00,0x00,0x06,0x04,0x08},
/*'<'*/{0x02,0x04,0x08,0x10,0x08,0x04,0x02}, /*'='*/{0x00,0x1F,0x00,0x00,0x1F,0x00,0x00},
/*'>'*/{0x08,0x04,0x02,0x01,0x02,0x04,0x08}, /*'?'*/{0x0E,0x11,0x01,0x06,0x04,0x00,0x04},
/*'@'*/{0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, /*'A'*/{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
/*'B'*/{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /*'C'*/{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
/*'D'*/{0x1E,0x09,0x09,0x09,0x09,0x09,0x1E}, /*'E'*/{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
/*'F'*/{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /*'G'*/{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
/*'H'*/{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /*'I'*/{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
/*'J'*/{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /*'K'*/{0x11,0x12,0x14,0x18,0x14,0x12,0x11},
/*'L'*/{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /*'M'*/{0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
/*'N'*/{0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /*'O'*/{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
/*'P'*/{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /*'Q'*/{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
/*'R'*/{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /*'S'*/{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
/*'T'*/{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /*'U'*/{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
/*'V'*/{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /*'W'*/{0x11,0x11,0x15,0x15,0x15,0x15,0x0A},
/*'X'*/{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /*'Y'*/{0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
/*'Z'*/{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
};

static void draw_glyph(int px, int py, char c, int sc,
                       Uint8 r, Uint8 g, Uint8 b)
{
    if (c>='a'&&c<='z') c-=32;
    if (c<0x20||c>0x5A) c=0x20;
    const Uint8 *gl=FONT[(int)(c-0x20)];
    col(r,g,b,255);
    for (int row=0;row<7;row++)
        for (int bit=0;bit<5;bit++)
            if (gl[row]&(0x10>>bit))
                rect(px+bit*sc,py+row*sc,sc,sc);
}

static void draw_str(int px, int py, const char *s, int sc,
                     Uint8 r, Uint8 g, Uint8 b)
{ for (;*s;s++,px+=6*sc) draw_glyph(px,py,*s,sc,r,g,b); }

static void draw_cstr(int y, const char *s, int sc,
                      Uint8 r, Uint8 g, Uint8 b)
{ draw_str((SCREEN_W-(int)strlen(s)*6*sc)/2,y,s,sc,r,g,b); }

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SPRITE DRAWING                                                             */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void draw_ship(int cx, int cy, int s, Uint8 r, Uint8 g, Uint8 b)
{
    col(r,g,b,255);
    rect(cx-10*s, cy+3*s, 20*s, 4*s);
    rect(cx- 7*s, cy,     14*s, 3*s);
    rect(cx- 3*s, cy-3*s,  6*s, 3*s);
    rect(cx-  s,  cy-5*s,  2*s, 2*s);
}

static void draw_alien(int cx, int cy, int type, int frm, int s,
                       Uint8 r, Uint8 g, Uint8 b)
{
    col(r,g,b,255);
    if (type==0) {
        rect(cx-3*s,cy-4*s,6*s,s);
        rect(cx-5*s,cy-3*s,10*s,2*s);
        rect(cx-6*s,cy-1*s,12*s,3*s);
        if (frm==0){rect(cx-6*s,cy+2*s,2*s,2*s);rect(cx-2*s,cy+2*s,2*s,2*s);rect(cx+s,cy+2*s,2*s,2*s);rect(cx+4*s,cy+2*s,2*s,2*s);}
        else       {rect(cx-4*s,cy+2*s,2*s,2*s);rect(cx,    cy+2*s,2*s,2*s);rect(cx+3*s,cy+2*s,2*s,2*s);}
        col(0,0,0,255);rect(cx-4*s,cy,2*s,s);rect(cx+2*s,cy,2*s,s);
    } else if (type==1) {
        rect(cx-s,  cy-4*s,2*s,2*s);
        rect(cx-5*s,cy-2*s,10*s,2*s);
        rect(cx-6*s,cy,    12*s,2*s);
        if (frm==0){rect(cx-6*s,cy+2*s,2*s,2*s);rect(cx-3*s,cy+2*s,2*s,2*s);rect(cx+2*s,cy+2*s,2*s,2*s);rect(cx+4*s,cy+2*s,2*s,2*s);}
        else       {rect(cx-8*s,cy+2*s,2*s,2*s);rect(cx-2*s,cy+2*s,2*s,2*s);rect(cx+s,  cy+2*s,2*s,2*s);rect(cx+6*s,cy+2*s,2*s,2*s);}
        col(0,0,0,255);rect(cx-3*s,cy-s,2*s,s);rect(cx+s,cy-s,2*s,s);
    } else {
        rect(cx-3*s,cy-4*s,6*s,2*s);
        rect(cx-5*s,cy-2*s,10*s,2*s);
        rect(cx-6*s,cy,    12*s,2*s);
        if (frm==0){rect(cx-6*s,cy+2*s,4*s,2*s);rect(cx-s,cy+2*s,2*s,2*s);rect(cx+2*s,cy+2*s,4*s,2*s);}
        else       {rect(cx-4*s,cy+2*s,2*s,2*s);rect(cx-s,cy+2*s,2*s,2*s);rect(cx+2*s,cy+2*s,2*s,2*s);rect(cx+4*s,cy+2*s,2*s,2*s);}
        col(0,0,0,255);rect(cx-3*s,cy-s,2*s,s);rect(cx+s,cy-s,2*s,s);
    }
    col(r,g,b,255);
}

static void draw_saucer(int cx, int cy, int s, Uint8 r, Uint8 g, Uint8 b)
{
    col(r,g,b,255);
    rect(cx-8*s,cy,    16*s,2*s);
    rect(cx-5*s,cy-2*s,10*s,2*s);
    rect(cx-3*s,cy-4*s, 6*s,2*s);
    rect(cx-8*s,cy+2*s,16*s,2*s);
    col(0,0,0,255);
    for (int i=-5;i<=4;i+=3) rect(cx+i*s,cy+s,s,s);
}

static void draw_explosion(int cx, int cy, int age)
{
    static const int ox[]={0,-1,1,0,-2,2,-1,1};
    static const int oy[]={-2,0,0,2,-1,1,1,-1};
    col(255,200,50,255);
    int r=3+age*2;
    for (int i=0;i<8;i++) rect(cx+ox[i]*r-2,cy+oy[i]*r-2,5,5);
    col(255,80,0,200); rect(cx-3,cy-3,7,7);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  COLLISION                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int overlap(float ax,float ay,int aw,int ah,
                   float bx,float by,int bw,int bh)
{ return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by; }

static void base_damage(float hx,float hy,int hw,int hh,int dmg)
{
    for (int i=0;i<BASE_COUNT;i++) {
        Base *b=&g_bases[i];
        if (!overlap(hx,hy,hw,hh,b->x,b->y,BASE_W,BASE_H)) continue;
        for (int r=0;r<BASE_ROWS;r++)
            for (int c=0;c<BASE_COLS;c++) {
                if (b->blocks[r][c]>=2) continue;
                float blx=b->x+c*BASE_BLOCK_W, bly=b->y+r*BASE_BLOCK_H;
                if (overlap(hx,hy,hw,hh,blx,bly,BASE_BLOCK_W,BASE_BLOCK_H)) {
                    b->blocks[r][c]+=dmg;
                    if (b->blocks[r][c]>2) b->blocks[r][c]=2;
                }
            }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  HIGH SCORES                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void scores_load(void)
{
    g_score_count=0;
    FILE *f=fopen(SCORES_FILE,"rb");
    if (!f) return;
    fread(&g_score_count,sizeof(int),1,f);
    if (g_score_count<0||g_score_count>MAX_SCORES) g_score_count=0;
    else fread(g_scores,sizeof(ScoreEntry),g_score_count,f);
    fclose(f);
}

static void scores_save(void)
{
    FILE *f=fopen(SCORES_FILE,"wb");
    if (!f) return;
    fwrite(&g_score_count,sizeof(int),1,f);
    fwrite(g_scores,sizeof(ScoreEntry),g_score_count,f);
    fclose(f);
}

/* Returns insertion rank 0..MAX_SCORES-1 if qualifies, else -1 */
static int scores_qualifies(int score)
{
    if (score<=0) return -1;
    for (int i=0;i<g_score_count;i++)
        if (score>g_scores[i].score) return i;
    if (g_score_count<MAX_SCORES) return g_score_count;
    return -1;
}

static void scores_insert(int rank, int score, const char *name)
{
    int top = (g_score_count<MAX_SCORES) ? g_score_count : MAX_SCORES-1;
    for (int i=top;i>rank;i--) g_scores[i]=g_scores[i-1];
    strncpy(g_scores[rank].name,name,NAME_MAX);
    g_scores[rank].name[NAME_MAX]='\0';
    g_scores[rank].score=score;
    if (g_score_count<MAX_SCORES) g_score_count++;
    scores_save();
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  GAME INIT                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void init_aliens(void)
{
    g_alive_count=0;
    for (int r=0;r<ALIEN_ROWS;r++)
        for (int c=0;c<ALIEN_COLS;c++) {
            Alien *a=&g_aliens[r][c];
            a->x=(float)(ALIEN_START_X+c*(ALIEN_W+ALIEN_PAD_X));
            a->y=(float)(ALIEN_START_Y+r*(ALIEN_H+ALIEN_PAD_Y));
            a->alive=1; a->anim=0;
            a->type=(r<2)?0:(r<4)?1:2;
            g_alive_count++;
        }
    g_alien_dx=1.0f; g_alien_drop=0; g_anim_timer=0;
    music_set_tempo(g_alien_speed);
}

static void init_bases(void)
{
    int gap=SCREEN_W/(BASE_COUNT+1);
    for (int i=0;i<BASE_COUNT;i++) {
        Base *b=&g_bases[i];
        b->x=(float)(gap*(i+1)-BASE_W/2);
        b->y=(float)BASE_Y;
        for (int r=0;r<BASE_ROWS;r++)
            for (int c=0;c<BASE_COLS;c++)
                b->blocks[r][c]=0;
        for (int c=2;c<BASE_COLS-2;c++) {
            b->blocks[BASE_ROWS-1][c]=2;
            b->blocks[BASE_ROWS-2][c]=2;
        }
    }
}

static void grant_pulse(void)
{
    g_has_pulse=(rand()%2==0);
    g_pulse_hud=g_has_pulse?FPS*4:0;
}

static void new_game(void)
{
    g_score=0; g_lives=LIVES_START; g_wave_num=1;
    g_ship_x=SCREEN_W/2.0f;
    g_bullet.active=0;
    for (int i=0;i<MAX_ALIEN_BULLETS;i++) g_abul[i].active=0;
    g_saucer.active=0; g_saucer_timer=480+rand()%480;
    g_pulse.active=0;
    g_alien_speed=ALIEN_SPEED_START;
    g_alien_fire_timer=0;
    g_new_score_rank=-1;
    init_aliens(); init_bases(); grant_pulse();
    g_state=STATE_PLAYING;
}

static void next_wave(void)
{
    g_wave_num++;
    if (g_lives<9) g_lives++;
    g_bullet.active=0;
    for (int i=0;i<MAX_ALIEN_BULLETS;i++) g_abul[i].active=0;
    g_saucer.active=0; g_saucer_timer=480+rand()%480;
    g_pulse.active=0;
    /* Speed boost: 30% per wave, capped */
    g_alien_speed*=ALIEN_SPEED_MULT;
    if (g_alien_speed>ALIEN_SPEED_CAP) g_alien_speed=ALIEN_SPEED_CAP;
    g_alien_fire_timer=0;
    init_aliens(); init_bases(); grant_pulse();
    music_set_tempo(g_alien_speed);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TITLE PARTICLES                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void spawn_particle(Particle *p)
{
    int e=rand()%4;
    if      (e==0){p->x=(float)(rand()%SCREEN_W);p->y=-20;      p->vx=(rand()%100-50)/80.0f;p->vy= 0.4f+(rand()%60)/100.0f;}
    else if (e==1){p->x=(float)(rand()%SCREEN_W);p->y=SCREEN_H+20;p->vx=(rand()%100-50)/80.0f;p->vy=-(0.4f+(rand()%60)/100.0f);}
    else if (e==2){p->y=(float)(rand()%SCREEN_H);p->x=-20;       p->vy=(rand()%100-50)/80.0f;p->vx= 0.4f+(rand()%60)/100.0f;}
    else          {p->y=(float)(rand()%SCREEN_H);p->x=SCREEN_W+20;p->vy=(rand()%100-50)/80.0f;p->vx=-(0.4f+(rand()%60)/100.0f);}
    p->type=rand()%2; p->atype=rand()%3;
    p->maxlife=p->life=(float)(120+rand()%240);
}

static void init_particles(void)
{
    for (int i=0;i<MAX_PARTICLES;i++) {
        spawn_particle(&g_parts[i]);
        g_parts[i].x=(float)(rand()%SCREEN_W);
        g_parts[i].y=(float)(rand()%SCREEN_H);
        g_parts[i].life=g_parts[i].maxlife*((rand()%100)/100.0f);
    }
}

static void update_particles(void)
{
    for (int i=0;i<MAX_PARTICLES;i++) {
        Particle *p=&g_parts[i];
        p->x+=p->vx; p->y+=p->vy; p->life-=1.0f;
        if (p->life<=0) spawn_particle(p);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  GAME UPDATE                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void trigger_game_over(void)
{
    int rank=scores_qualifies(g_score);
    if (rank>=0) {
        g_new_score_rank=rank;
        g_entry_len=0;
        memset(g_entry_name,0,sizeof(g_entry_name));
        g_entry_blink=0;
        g_state=STATE_NAME_ENTRY;
    } else {
        g_state=STATE_GAME_OVER;
        g_gameover_timer=FPS*3;
    }
}

static void update_game(const Uint8 *keys)
{
    if (keys[SDL_SCANCODE_LEFT])  {g_ship_x-=SHIP_SPEED;if(g_ship_x<SHIP_W/2)g_ship_x=SHIP_W/2;}
    if (keys[SDL_SCANCODE_RIGHT]) {g_ship_x+=SHIP_SPEED;if(g_ship_x>SCREEN_W-SHIP_W/2)g_ship_x=SCREEN_W-SHIP_W/2.0f;}

    /* Player bullet */
    if (g_bullet.active) {
        g_bullet.y-=BULLET_SPEED;
        if (g_bullet.y<HUD_H){g_bullet.active=0;goto done_pb;}
        for (int r=0;r<ALIEN_ROWS&&g_bullet.active;r++)
            for (int c=0;c<ALIEN_COLS&&g_bullet.active;c++) {
                Alien *a=&g_aliens[r][c];
                if (!a->alive) continue;
                if (overlap(g_bullet.x-BULLET_W/2,g_bullet.y,BULLET_W,BULLET_H,
                            a->x-ALIEN_W/2,a->y-ALIEN_H/2,ALIEN_W,ALIEN_H)) {
                    a->alive=0;g_alive_count--;g_score+=POINTS_ALIEN;
                    g_bullet.active=0;beep(880.0f,0.05f);
                }
            }
        if (g_bullet.active&&g_saucer.active&&
            overlap(g_bullet.x-BULLET_W/2,g_bullet.y,BULLET_W,BULLET_H,
                    g_saucer.x-SAUCER_W/2,SAUCER_Y,SAUCER_W,SAUCER_H)) {
            g_saucer.active=0;g_score+=POINTS_SAUCER;
            g_bullet.active=0;beep(1200.0f,0.18f);
        }
        if (g_bullet.active)
            for (int i=0;i<BASE_COUNT;i++) {
                Base *b=&g_bases[i];
                if (overlap(g_bullet.x-BULLET_W/2,g_bullet.y,BULLET_W,BULLET_H,
                            b->x,b->y,BASE_W,BASE_H)) {
                    base_damage(g_bullet.x-BULLET_W/2,g_bullet.y,BULLET_W,BULLET_H,1);
                    g_bullet.active=0;break;
                }
            }
    }
    done_pb:;

    /* Alien animation */
    g_anim_timer++;
    int ai=(int)(32.0f/(g_alien_speed+0.6f));
    if (ai<5)ai=5;
    if (g_anim_timer>=ai){
        g_anim_timer=0;
        for (int r=0;r<ALIEN_ROWS;r++)
            for (int c=0;c<ALIEN_COLS;c++)
                g_aliens[r][c].anim^=1;
    }

    float left=9999,right=-9999;
    for (int r=0;r<ALIEN_ROWS;r++)
        for (int c=0;c<ALIEN_COLS;c++) {
            if (!g_aliens[r][c].alive) continue;
            float ax=g_aliens[r][c].x;
            if (ax-ALIEN_W/2<left)  left =ax-ALIEN_W/2;
            if (ax+ALIEN_W/2>right) right=ax+ALIEN_W/2;
        }

    if (g_alien_drop>0) {
        for (int r=0;r<ALIEN_ROWS;r++)
            for (int c=0;c<ALIEN_COLS;c++)
                if (g_aliens[r][c].alive) g_aliens[r][c].y+=5.0f;
        g_alien_drop--;
    } else {
        int nd=0;
        for (int r=0;r<ALIEN_ROWS;r++)
            for (int c=0;c<ALIEN_COLS;c++)
                if (g_aliens[r][c].alive) g_aliens[r][c].x+=g_alien_dx*g_alien_speed;
        if (g_alien_dx>0&&right+g_alien_dx*g_alien_speed>SCREEN_W-10) nd=1;
        if (g_alien_dx<0&&left +g_alien_dx*g_alien_speed<10)           nd=1;
        if (nd){g_alien_dx=-g_alien_dx;g_alien_drop=2;}
    }

    for (int r=0;r<ALIEN_ROWS;r++)
        for (int c=0;c<ALIEN_COLS;c++)
            if (g_aliens[r][c].alive&&g_aliens[r][c].y+ALIEN_H/2>=SHIP_Y)
                g_lives=0;

    g_alien_fire_timer++;
    int fi=(int)(70.0f/(g_alien_speed+0.4f));
    if (fi<12)fi=12;
    if (g_alien_fire_timer>=fi) {
        g_alien_fire_timer=0;
        for (int i=0;i<MAX_ALIEN_BULLETS;i++) {
            if (g_abul[i].active) continue;
            for (int t=0;t<30;t++) {
                int rr=rand()%ALIEN_ROWS,rc=rand()%ALIEN_COLS;
                if (g_aliens[rr][rc].alive) {
                    g_abul[i].x=g_aliens[rr][rc].x;
                    g_abul[i].y=g_aliens[rr][rc].y+ALIEN_H/2;
                    g_abul[i].active=1;break;
                }
            }
            break;
        }
    }

    for (int i=0;i<MAX_ALIEN_BULLETS;i++) {
        AlienBullet *ab=&g_abul[i];
        if (!ab->active) continue;
        ab->y+=ALIEN_BULLET_SPEED;
        if (ab->y>SCREEN_H){ab->active=0;continue;}
        int hit=0;
        for (int bi=0;bi<BASE_COUNT&&!hit;bi++) {
            Base *b=&g_bases[bi];
            if (overlap(ab->x-ALIEN_BULLET_W/2,ab->y,ALIEN_BULLET_W,ALIEN_BULLET_H,
                        b->x,b->y,BASE_W,BASE_H)) {
                base_damage(ab->x-ALIEN_BULLET_W/2,ab->y,ALIEN_BULLET_W,ALIEN_BULLET_H,1);
                ab->active=0;hit=1;
            }
        }
        if (hit) continue;
        if (g_expl_timer==0&&
            overlap(ab->x-ALIEN_BULLET_W/2,ab->y,ALIEN_BULLET_W,ALIEN_BULLET_H,
                    g_ship_x-SHIP_W/2,SHIP_Y,SHIP_W,SHIP_H)) {
            ab->active=0;g_lives--;g_expl_timer=45;beep(160.0f,0.4f);
            if (g_lives<=0) trigger_game_over();
        }
    }

    if (!g_saucer.active) {
        g_saucer_timer--;
        if (g_saucer_timer<=0) {
            g_saucer.active=1;
            g_saucer.dx=(rand()%2)?1:-1;
            g_saucer.x=(g_saucer.dx>0)?-(float)(SAUCER_W/2):(float)(SCREEN_W+SAUCER_W/2);
            g_saucer_bomb_timer=60+rand()%60;
            beep(440.0f,0.08f);
        }
    }
    if (g_saucer.active) {
        g_saucer.x+=g_saucer.dx*SAUCER_SPEED;
        if (g_saucer.x<-(float)SAUCER_W||g_saucer.x>SCREEN_W+(float)SAUCER_W) {
            g_saucer.active=0;g_saucer_timer=480+rand()%480;
        }
        g_saucer_bomb_timer--;
        if (g_saucer_bomb_timer<=0) {
            g_saucer_bomb_timer=50+rand()%50;
            for (int i=0;i<MAX_ALIEN_BULLETS;i++)
                if (!g_abul[i].active) {
                    g_abul[i].x=g_saucer.x;
                    g_abul[i].y=(float)(SAUCER_Y+SAUCER_H);
                    g_abul[i].active=1;break;
                }
        }
    }

    if (g_pulse.active) {
        g_pulse.radius+=PULSE_SPEED;
        for (int r=0;r<ALIEN_ROWS;r++)
            for (int c=0;c<ALIEN_COLS;c++) {
                Alien *a=&g_aliens[r][c];
                if (!a->alive) continue;
                float dx=a->x-g_pulse.x,dy=a->y-g_pulse.y;
                if (sqrtf(dx*dx+dy*dy)<g_pulse.radius) {a->alive=0;g_alive_count--;g_score+=POINTS_ALIEN;}
            }
        for (int i=0;i<BASE_COUNT;i++) {
            Base *b=&g_bases[i];
            float bdx=b->x+BASE_W/2-g_pulse.x,bdy=b->y+BASE_H/2-g_pulse.y;
            float dist=sqrtf(bdx*bdx+bdy*bdy);
            if (dist<g_pulse.radius&&dist>g_pulse.radius-PULSE_SPEED-20)
                for (int r=0;r<BASE_ROWS;r++)
                    for (int c=0;c<BASE_COLS;c++)
                        if (b->blocks[r][c]<2&&rand()%10<7) b->blocks[r][c]=2;
        }
        if (g_pulse.radius>=PULSE_RADIUS_MAX) g_pulse.active=0;
    }
    if (g_pulse_hud>0) g_pulse_hud--;

    if (g_alive_count<=0&&g_state==STATE_PLAYING) {
        g_state=STATE_WAVE_CLEAR;
        g_waveclear_timer=FPS*2;
        beep(660.0f,0.5f);
    }
    if (g_expl_timer>0) g_expl_timer--;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  RENDERING                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void render_stars(unsigned seed)
{
    srand(seed);
    for (int i=0;i<100;i++) {
        int sx=rand()%SCREEN_W,sy=HUD_H+rand()%(SCREEN_H-HUD_H);
        Uint8 b=(Uint8)(50+rand()%160);
        col(b,b,b+20,255);
        SDL_RenderDrawPoint(g_ren,sx,sy);
    }
    srand((unsigned)time(NULL));
}

static void render_hud(void)
{
    col(0,0,18,255); rect(0,0,SCREEN_W,HUD_H);
    col(0,140,0,255); SDL_RenderDrawLine(g_ren,0,HUD_H,SCREEN_W,HUD_H);
    char buf[48];
    snprintf(buf,sizeof(buf),"SCORE:%06d  WAVE:%d",g_score,g_wave_num);
    draw_str(8,7,buf,2,255,220,0);
    int lx=SCREEN_W-12;
    for (int i=0;i<g_lives-1&&i<8;i++){lx-=26;draw_ship(lx,18,1,80,220,80);}
    if (g_has_pulse) {
        int vis=(g_pulse_hud>0)?((SDL_GetTicks()/200)%2):1;
        if (vis) draw_cstr(7,"* PULSE READY *",1,80,200,255);
    }
}

static void render_bases(void)
{
    for (int i=0;i<BASE_COUNT;i++) {
        Base *b=&g_bases[i];
        for (int r=0;r<BASE_ROWS;r++)
            for (int c=0;c<BASE_COLS;c++) {
                int st=b->blocks[r][c];
                if (st>=2) continue;
                int bx=(int)b->x+c*BASE_BLOCK_W,by=(int)b->y+r*BASE_BLOCK_H;
                if (st==0) col(0,210,60,255); else col(0,120,35,255);
                rect(bx,by,BASE_BLOCK_W-1,BASE_BLOCK_H-1);
            }
    }
}

static void render_pulse(void)
{
    if (!g_pulse.active) return;
    blend_on();
    float frac=g_pulse.radius/PULSE_RADIUS_MAX;
    Uint8 alpha=(Uint8)(220*(1.0f-frac));
    for (int ring=0;ring<5;ring++) {
        float r=g_pulse.radius-ring*5;
        if (r<0) continue;
        col(80,160,255,(Uint8)(alpha/(ring+1)));
        for (int i=0;i<64;i++) {
            float a1=(float)i/64*6.28318f,a2=(float)(i+1)/64*6.28318f;
            SDL_RenderDrawLine(g_ren,
                (int)(g_pulse.x+cosf(a1)*r),(int)(g_pulse.y+sinf(a1)*r),
                (int)(g_pulse.x+cosf(a2)*r),(int)(g_pulse.y+sinf(a2)*r));
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
    for (int r=0;r<ALIEN_ROWS;r++)
        for (int c=0;c<ALIEN_COLS;c++) {
            Alien *a=&g_aliens[r][c];
            if (!a->alive) continue;
            Uint8 ar=100,ag=255,ab=100;
            if (a->type==1){ar=100;ag=100;ab=255;}
            if (a->type==2){ar=255;ag=100;ab=100;}
            draw_alien((int)a->x,(int)a->y,a->type,a->anim,1,ar,ag,ab);
        }
    if (g_saucer.active) draw_saucer((int)g_saucer.x,SAUCER_Y+SAUCER_H/2,1,255,60,210);
    if (g_bullet.active) {
        col(255,255,80,255);
        rect((int)(g_bullet.x-BULLET_W/2),(int)g_bullet.y,BULLET_W,BULLET_H);
    }
    for (int i=0;i<MAX_ALIEN_BULLETS;i++) {
        AlienBullet *ab=&g_abul[i];
        if (!ab->active) continue;
        col(255,60,60,255);
        rect((int)(ab->x-ALIEN_BULLET_W/2),(int)ab->y,ALIEN_BULLET_W,ALIEN_BULLET_H);
    }
    render_pulse();
    if (g_expl_timer>0)  draw_explosion((int)g_ship_x,SHIP_Y+SHIP_H/2,(45-g_expl_timer)/6);
    else if (g_lives>0)  draw_ship((int)g_ship_x,SHIP_Y+SHIP_H/2,2,100,255,100);
    col(0,160,0,255); SDL_RenderDrawLine(g_ren,0,SCREEN_H-20,SCREEN_W,SCREEN_H-20);
}

static void render_paused(void)
{
    render_game_scene();
    blend_on();col(0,0,0,150);rect(0,0,SCREEN_W,SCREEN_H);blend_off();
    draw_cstr(SCREEN_H/2-24,"PAUSED",4,255,255,0);
    draw_cstr(SCREEN_H/2+20,"PRESS P TO CONTINUE  Q TO QUIT",2,180,180,180);
}

static void render_wave_clear(void)
{
    render_game_scene();
    blend_on();col(0,0,0,130);rect(0,0,SCREEN_W,SCREEN_H);blend_off();
    char buf[40];
    snprintf(buf,sizeof(buf),"WAVE %d CLEARED!",g_wave_num-1);
    draw_cstr(SCREEN_H/2-50,buf,3,100,255,100);
    /* Show next-wave speed */
    float next_speed=g_alien_speed*ALIEN_SPEED_MULT;
    if (next_speed>ALIEN_SPEED_CAP) next_speed=ALIEN_SPEED_CAP;
    char spd[60];
    snprintf(spd,sizeof(spd),"NEXT WAVE SPEED: %.1fx    BONUS LIFE!",
             next_speed/ALIEN_SPEED_START);
    draw_cstr(SCREEN_H/2+6,spd,2,255,220,60);
}

static void render_game_over(void)
{
    render_game_scene();
    blend_on();col(0,0,0,160);rect(0,0,SCREEN_W,SCREEN_H);blend_off();
    draw_cstr(SCREEN_H/2-50,"GAME OVER",5,255,40,40);
    char buf[32]; snprintf(buf,sizeof(buf),"FINAL SCORE: %d",g_score);
    draw_cstr(SCREEN_H/2+20,buf,2,255,220,60);
    draw_cstr(SCREEN_H/2+58,"RETURNING TO TITLE...",2,160,160,160);
}

/* ── Name entry ──────────────────────────────────────────────────────────── */
static void render_name_entry(void)
{
    render_game_scene();
    blend_on();col(0,0,0,175);rect(0,0,SCREEN_W,SCREEN_H);blend_off();

    draw_cstr(90,"NEW HIGH SCORE!",4,255,220,0);

    char sbuf[48];
    snprintf(sbuf,sizeof(sbuf),"SCORE: %d    RANK #%d",g_score,g_new_score_rank+1);
    draw_cstr(142,sbuf,2,255,180,60);

    draw_cstr(204,"ENTER YOUR NAME (THEN PRESS ENTER):",2,200,200,200);

    /* Input box */
    int bw=300,bh=30,bx=SCREEN_W/2-bw/2,by=244;
    col(0,50,0,255);rect(bx,by,bw,bh);
    col(0,200,0,255);SDL_RenderDrawRect(g_ren,&(SDL_Rect){bx,by,bw,bh});

    draw_str(bx+8,by+8,g_entry_name,2,80,255,80);
    /* Blinking cursor */
    if ((g_entry_blink/18)%2==0) {
        col(80,255,80,255);
        rect(bx+8+g_entry_len*12,by+8,2,16);
    }

    draw_cstr(294,"A-Z  0-9  SPACE  -   BACKSPACE=DELETE",1,140,140,140);
    if (g_entry_len==0)
        draw_cstr(312,"(ENTER AT LEAST ONE CHARACTER)",1,180,80,80);
}

/* ── High scores table ───────────────────────────────────────────────────── */
static void render_high_scores(void)
{
    /* Semi-transparent panel over whatever scene is behind */
    int pw=520,ph=420,px2=(SCREEN_W-pw)/2,py2=72;
    blend_on();col(0,0,28,220);rect(px2,py2,pw,ph);blend_off();
    col(0,160,0,255);SDL_RenderDrawRect(g_ren,&(SDL_Rect){px2,py2,pw,ph});
    /* Inner highlight line */
    col(0,80,0,255);SDL_RenderDrawRect(g_ren,&(SDL_Rect){px2+2,py2+2,pw-4,ph-4});

    draw_cstr(py2+12,"HIGH SCORES",3,255,220,0);
    col(0,120,0,255);SDL_RenderDrawLine(g_ren,px2+20,py2+46,px2+pw-20,py2+46);

    draw_str(px2+22,  py2+54,"RANK",1,160,160,60);
    draw_str(px2+90,  py2+54,"NAME",1,160,160,60);
    draw_str(px2+pw-72,py2+54,"SCORE",1,160,160,60);

    if (g_score_count==0) {
        draw_cstr(py2+200,"NO SCORES YET - BE THE FIRST!",1,120,120,120);
    } else {
        for (int i=0;i<g_score_count;i++) {
            int ry=py2+70+i*30;
            /* Highlight new entry */
            if (i==g_new_score_rank) {
                blend_on();col(0,100,0,100);rect(px2+16,ry-2,pw-32,22);blend_off();
            }
            Uint8 rc=200,gc=200,bc=200;
            if      (i==0){rc=255;gc=215;bc=0;}
            else if (i==1){rc=192;gc=192;bc=192;}
            else if (i==2){rc=205;gc=127;bc=50;}

            char rank[8]; snprintf(rank,sizeof(rank),"#%d",i+1);
            draw_str(px2+22,  ry,rank,              1,rc,gc,bc);
            draw_str(px2+90,  ry,g_scores[i].name,  1,rc,gc,bc);
            char sc2[16]; snprintf(sc2,sizeof(sc2),"%d",g_scores[i].score);
            draw_str(px2+pw-22-(int)strlen(sc2)*6, ry, sc2, 1, rc,gc,bc);
        }
    }

    col(0,100,0,255);
    SDL_RenderDrawLine(g_ren,px2+20,py2+ph-24,px2+pw-20,py2+ph-24);
    draw_cstr(py2+ph-16,"PRESS ANY KEY TO CONTINUE",1,120,120,120);
}

/* ── Title screen ────────────────────────────────────────────────────────── */
static void render_title(void)
{
    col(0,0,14,255);SDL_RenderClear(g_ren);
    srand(77);
    for (int i=0;i<130;i++) {
        int sx=rand()%SCREEN_W,sy=rand()%SCREEN_H;
        Uint8 b=(Uint8)(50+rand()%180);
        col(b,b,(Uint8)SDL_min(255,b+40),255);
        SDL_RenderDrawPoint(g_ren,sx,sy);
        if (b>170) SDL_RenderDrawPoint(g_ren,sx+1,sy);
    }
    srand((unsigned)time(NULL));

    for (int i=0;i<MAX_PARTICLES;i++) {
        Particle *p=&g_parts[i];
        float fade=p->life/p->maxlife;
        if (p->type==0){col(255,180,60,(Uint8)(180*fade));rect((int)p->x,(int)p->y,3,9);}
        else {
            Uint8 r=50,g=50,b=50;
            if (p->atype==0){r=30;g=100;b=30;}else if(p->atype==1){r=30;g=30;b=100;}else{r=100;g=30;b=30;}
            draw_alien((int)p->x,(int)p->y,p->atype,(g_title_anim/22)%2,1,
                       (Uint8)(r*fade),(Uint8)(g*fade),(Uint8)(b*fade));
        }
    }
    for (int c=0;c<9;c++) {
        int at=c%3,ax=55+c*80,ay=162;
        Uint8 ar=50,ag=160,ab=50;
        if (at==1){ar=50;ag=50;ab=160;} if (at==2){ar=160;ag=50;ab=50;}
        draw_alien(ax,ay,at,(g_title_anim/24)%2,1,ar,ag,ab);
    }
    draw_ship(SCREEN_W/2,200,3,60,180,60);
    draw_saucer(110,228,2,180,50,160);
    draw_saucer(SCREEN_W-110,228,2,180,50,160);

    Uint8 pulse=(Uint8)(150+90*sinf(g_title_anim*0.045f));
    draw_cstr(34,"ALIEN INVASION",5,15,60,15);
    draw_cstr(32,"ALIEN INVASION",5,60,pulse,60);

    col(0,110,0,255);SDL_RenderDrawLine(g_ren,50,264,SCREEN_W-50,264);

    /* 4-item menu */
    char snd_str[20],mus_str[20];
    snprintf(snd_str,sizeof(snd_str),"SOUND: %s",g_sound_on?"ON ":"OFF");
    snprintf(mus_str,sizeof(mus_str),"MUSIC: %s",g_music_on?"ON ":"OFF");
    const char *items[4]={"PLAY GAME",snd_str,mus_str,"HIGH SCORES"};

    int my=278;
    for (int i=0;i<4;i++) {
        int iw=(int)strlen(items[i])*12;
        if (g_title_sel==i) {
            col(0,70,0,180);rect(SCREEN_W/2-iw/2-8,my-3,iw+16,18);
            char sel[40]; snprintf(sel,sizeof(sel),"> %s <",items[i]);
            draw_cstr(my,sel,2,80,255,80);
        } else {
            draw_cstr(my,items[i],2,140,140,140);
        }
        my+=32;
    }

    col(0,70,0,255);SDL_RenderDrawLine(g_ren,50,my+4,SCREEN_W-50,my+4);
    draw_cstr(my+12,"ARROWS:MOVE  SPACE:FIRE  ENTER:PULSE  P:PAUSE  Q:QUIT",1,100,100,100);

    col(0,70,0,255);SDL_RenderDrawLine(g_ren,50,my+28,SCREEN_W-50,my+28);
    {
        int px2=SCREEN_W/2-180;
        draw_alien(px2,     my+52,0,0,1,60,180,60);draw_str(px2+14, my+46,"=50", 1,160,160,160);
        draw_alien(px2+80,  my+52,1,0,1,60,60,180);draw_str(px2+94, my+46,"=50", 1,160,160,160);
        draw_alien(px2+160, my+52,2,0,1,180,60,60);draw_str(px2+174,my+46,"=50", 1,160,160,160);
        draw_saucer(px2+268,my+52,1,180,50,160);    draw_str(px2+284,my+46,"=200",1,160,160,160);
    }
    draw_str(SCREEN_W-54,SCREEN_H-14,"V2.0",1,50,50,50);
    g_title_anim++;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  MAIN                                                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    srand((unsigned)time(NULL));
    scores_load();

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0) {
        fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;
    }
    g_win=SDL_CreateWindow("Alien Invasion",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);
    if (!g_win){fprintf(stderr,"Window: %s\n",SDL_GetError());return 1;}
    g_ren=SDL_CreateRenderer(g_win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren=SDL_CreateRenderer(g_win,-1,0);
    if (!g_ren){fprintf(stderr,"Renderer: %s\n",SDL_GetError());return 1;}

    init_audio();
    init_particles();

    int k_up=0,k_dn=0,k_ret=0,k_p=0,k_q=0,k_sp=0;
    int pk_up,pk_dn,pk_ret,pk_p,pk_q,pk_sp;
    int any_key=0;

#define PRESSED(k,pk) ((k)&&!(pk))

    SDL_bool running=SDL_TRUE;
    while (running) {
        Uint32 t0=SDL_GetTicks();
        any_key=0;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type==SDL_QUIT){running=SDL_FALSE;break;}

            if (g_state==STATE_NAME_ENTRY && ev.type==SDL_KEYDOWN) {
                SDL_Keycode k=ev.key.keysym.sym;
                if (k==SDLK_RETURN||k==SDLK_KP_ENTER) {
                    if (g_entry_len>0) {
                        scores_insert(g_new_score_rank,g_score,g_entry_name);
                        g_state=STATE_HIGH_SCORES;
                    }
                } else if (k==SDLK_BACKSPACE) {
                    if (g_entry_len>0){g_entry_len--;g_entry_name[g_entry_len]='\0';}
                } else if (g_entry_len<NAME_MAX) {
                    char c=0;
                    if (k>=SDLK_a&&k<=SDLK_z)              c=(char)('A'+(k-SDLK_a));
                    else if (k>='0'&&k<='9')                c=(char)k;
                    else if (k==SDLK_SPACE)                 c=' ';
                    else if (k==SDLK_MINUS||k==SDLK_KP_MINUS) c='-';
                    if (c){g_entry_name[g_entry_len++]=c;g_entry_name[g_entry_len]='\0';}
                }
            } else if (ev.type==SDL_KEYDOWN) {
                any_key=1;
            }
        }

        const Uint8 *K=SDL_GetKeyboardState(NULL);
        pk_up=k_up;pk_dn=k_dn;pk_ret=k_ret;
        pk_p=k_p;pk_q=k_q;pk_sp=k_sp;
        k_up =K[SDL_SCANCODE_UP];
        k_dn =K[SDL_SCANCODE_DOWN];
        k_ret=K[SDL_SCANCODE_RETURN];
        k_p  =K[SDL_SCANCODE_P];
        k_q  =K[SDL_SCANCODE_Q];
        k_sp =K[SDL_SCANCODE_SPACE];

        switch (g_state) {

        case STATE_TITLE:
            update_particles();
            if (PRESSED(k_up,pk_up))   {g_title_sel=(g_title_sel+3)%4;beep(550.0f,0.03f);}
            if (PRESSED(k_dn,pk_dn))   {g_title_sel=(g_title_sel+1)%4;beep(550.0f,0.03f);}
            if (PRESSED(k_ret,pk_ret)) {
                if      (g_title_sel==0){beep(750.0f,0.1f);new_game();}
                else if (g_title_sel==1){g_sound_on^=1;beep(500.0f,0.04f);}
                else if (g_title_sel==2){g_music_on^=1;beep(500.0f,0.04f);}
                else if (g_title_sel==3){g_new_score_rank=-1;g_state=STATE_HIGH_SCORES;}
            }
            render_title();
            break;

        case STATE_PLAYING:
            if (PRESSED(k_q,pk_q)){g_state=STATE_TITLE;break;}
            if (PRESSED(k_p,pk_p)){g_state=STATE_PAUSED;break;}
            if (PRESSED(k_sp,pk_sp)&&!g_bullet.active) {
                g_bullet.x=g_ship_x;g_bullet.y=(float)(SHIP_Y-BULLET_H);
                g_bullet.active=1;beep(1050.0f,0.04f);
            }
            if (PRESSED(k_ret,pk_ret)&&g_has_pulse&&!g_pulse.active) {
                g_pulse.x=g_ship_x;g_pulse.y=(float)(SHIP_Y+SHIP_H/2);
                g_pulse.radius=1.0f;g_pulse.active=1;
                g_has_pulse=0;beep(200.0f,0.7f);
            }
            update_game(K);
            render_game_scene();
            break;

        case STATE_PAUSED:
            if (PRESSED(k_p,pk_p)) g_state=STATE_PLAYING;
            if (PRESSED(k_q,pk_q)) g_state=STATE_TITLE;
            render_paused();
            break;

        case STATE_WAVE_CLEAR:
            g_waveclear_timer--;
            if (g_waveclear_timer<=0){next_wave();g_state=STATE_PLAYING;}
            render_wave_clear();
            break;

        case STATE_GAME_OVER:
            g_gameover_timer--;
            if (g_gameover_timer<=0||PRESSED(k_ret,pk_ret)) g_state=STATE_TITLE;
            render_game_over();
            break;

        case STATE_NAME_ENTRY:
            g_entry_blink++;
            render_name_entry();
            break;

        case STATE_HIGH_SCORES:
            /* Render title as background, then the scores panel on top */
            render_title();
            render_high_scores();
            if (any_key){g_state=STATE_TITLE;g_new_score_rank=-1;}
            break;
        }

        SDL_RenderPresent(g_ren);
        Uint32 elapsed=SDL_GetTicks()-t0;
        if (elapsed<FRAME_MS) SDL_Delay(FRAME_MS-elapsed);
    }

    if (g_audio_dev) SDL_CloseAudioDevice(g_audio_dev);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    return 0;
}
