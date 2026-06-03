/*
 * beatchess_sdl.cpp - BeatChess SDL2 Edition
 * Linux and Windows. No Allegro dependency.
 *
 * Audio: SDL_mixer MIDI stubs — drop .mid files in later.
 * Graphics: SDL2 renderer, 640x480 logical resolution scaled to window.
 *
 * Keyboard shortcuts:
 *   N        - New game
 *   U        - Undo
 *   A        - Toggle AI vs AI / Player vs AI
 *   B        - Swap player color
 *   F        - Flip board
 *   M        - Toggle music
 *   ?        - Help
 *   Q / Esc  - Quit
 */

#include "beatchess.h"
#include "visualization.h"
#include "chess_ai_move.h"
#include "pgn.h"
#include "chess_pieces_loader_sdl.h"
#include "DejaVuMono.h"
#include "DejaVuMonoBold.h"


#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/types.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define WINDOW_W        900
#define WINDOW_H        640
#define LOGICAL_W       900
#define LOGICAL_H       640

#define BOARD_MARGIN_X  60
#define BOARD_MARGIN_Y  40
#define SQUARE_SIZE     68    /* 8 * 68 = 544, fits in 640 */

#define PANEL_X         (BOARD_MARGIN_X + 8 * SQUARE_SIZE + 12)
#define PANEL_W         (LOGICAL_W - PANEL_X - 8)

#define MENU_H          22

/* Colours (R,G,B,A) */
#define COL_BG          {  20,  20,  25, 255 }
#define COL_LIGHT_SQ    { 240, 217, 181, 255 }
#define COL_DARK_SQ     { 181, 136,  99, 255 }
#define COL_HIGHLIGHT   { 106, 176,  76, 200 }
#define COL_SELECTED    { 255, 255,  80, 200 }
#define COL_LASTMOVE    {  80, 200, 255, 120 }
#define COL_CHECK       { 220,  50,  50, 180 }
#define COL_PANEL       {  35,  35,  42, 255 }
#define COL_MENUBAR     {  28,  28,  36, 255 }
#define COL_MENUHOVER   {  55,  55,  70, 255 }
#define COL_BTN         {  50,  50,  65, 255 }
#define COL_BTNHOVER    {  80,  80, 105, 255 }
#define COL_WHITE_TEXT  { 230, 230, 230, 255 }
#define COL_YELLOW      { 255, 220,  60, 255 }
#define COL_GREEN       {  80, 200,  80, 255 }
#define COL_RED         { 220,  70,  70, 255 }
#define COL_CYAN        {  60, 200, 220, 255 }

/* ============================================================================
 * Piece rendering — drawn with SDL primitives, no sprites needed
 * ============================================================================ */

static void sdl_fill_rect(SDL_Renderer *r, int x, int y, int w, int h,
                           Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_SetRenderDrawBlendMode(r, A < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

static void sdl_draw_rect(SDL_Renderer *r, int x, int y, int w, int h,
                           Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderDrawRect(r, &rc);
}

/* Draw a filled circle */
static void sdl_fill_circle(SDL_Renderer *r, int cx, int cy, int radius,
                              Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_SetRenderDrawBlendMode(r, A < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

/* ============================================================================
 * Text rendering via SDL_ttf
 * ============================================================================ */

static TTF_Font *g_font_sm  = NULL;   /* 13px */
static TTF_Font *g_font_med = NULL;   /* 17px */
static TTF_Font *g_font_lg  = NULL;   /* 22px */

/* Base64 decode — RFC 4648, no line-wrap handling needed for our data */
static unsigned char *b64_decode(const char *src, size_t src_len, size_t *out_len) {
    static const signed char T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    /* Count valid chars */
    size_t valid = 0;
    for (size_t i = 0; i < src_len; i++)
        if (T[(unsigned char)src[i]] >= 0 || src[i] == '=') valid++;
    size_t decoded_len = (valid / 4) * 3;
    unsigned char *dst = (unsigned char *)malloc(decoded_len + 4);
    if (!dst) return NULL;
    size_t di = 0;
    unsigned char buf[4];
    int bi = 0;
    for (size_t i = 0; i < src_len && di < decoded_len; i++) {
        char c = src[i];
        if (T[(unsigned char)c] < 0 && c != '=') continue;
        buf[bi++] = (unsigned char)c;
        if (bi == 4) {
            signed char v0 = T[(unsigned char)buf[0]];
            signed char v1 = T[(unsigned char)buf[1]];
            signed char v2 = (buf[2] == '=') ? 0 : T[(unsigned char)buf[2]];
            signed char v3 = (buf[3] == '=') ? 0 : T[(unsigned char)buf[3]];
            if (di < decoded_len) dst[di++] = (v0 << 2) | (v1 >> 4);
            if (buf[2] != '=' && di < decoded_len) dst[di++] = (v1 << 4) | (v2 >> 2);
            if (buf[3] != '=' && di < decoded_len) dst[di++] = (v2 << 6) | v3;
            bi = 0;
        }
    }
    *out_len = di;
    return dst;
}

static unsigned char *g_font_buf_regular = NULL;
static unsigned char *g_font_buf_bold    = NULL;

static bool fonts_init(void) {
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        return false;
    }
    /* Decode the font once, then open three TTF_Font handles from the same buffer */
    size_t font_len = 0;
    g_font_buf_regular = b64_decode(DEJAVU_REGULAR_FONT_B64,
                                     DEJAVU_REGULAR_FONT_B64_SIZE, &font_len);
    if (!g_font_buf_regular) {
        fprintf(stderr, "Warning: embedded font decode failed\n");
        return false;
    }
    /* SDL_RWFromMem does NOT take ownership — safe to reuse buffer */
    g_font_sm  = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf_regular, (int)font_len), 1, 13);
    g_font_med = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf_regular, (int)font_len), 1, 17);
    g_font_lg  = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf_regular, (int)font_len), 1, 22);
    if (!g_font_sm || !g_font_med || !g_font_lg) {
        fprintf(stderr, "Warning: embedded font open failed: %s\n", TTF_GetError());
        return false;
    }
    printf("Embedded DejaVu font loaded OK\n");
    return true;
}

static void fonts_shutdown(void) {
    if (g_font_sm)  TTF_CloseFont(g_font_sm);
    if (g_font_med) TTF_CloseFont(g_font_med);
    if (g_font_lg)  TTF_CloseFont(g_font_lg);
    TTF_Quit();
    if (g_font_buf_regular) { free(g_font_buf_regular); g_font_buf_regular = NULL; }
    if (g_font_buf_bold)    { free(g_font_buf_bold);    g_font_buf_bold    = NULL; }
}

static void render_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                         int x, int y, Uint8 R, Uint8 G, Uint8 B) {
    if (!font || !text || !text[0]) return;
    SDL_Color col = {R, G, B, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void render_text_centered(SDL_Renderer *r, TTF_Font *font, const char *text,
                                   int cx, int y, Uint8 R, Uint8 G, Uint8 B) {
    if (!font || !text || !text[0]) return;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text, &w, &h);
    render_text(r, font, text, cx - w/2, y, R, G, B);
}

/* ============================================================================
 * Audio stubs — replace with SDL_mixer calls when you have MIDI files
 * ============================================================================ */

typedef enum {
    SND_MOVE = 0, SND_CAPTURE, SND_CHECK, SND_CHECKMATE, SND_STALEMATE,
    SND_NEWGAME, SND_COUNT
} SoundEvent;

static Mix_Music *g_bg_music                = NULL;
static Mix_Chunk *g_stings[SND_COUNT]       = {};
static bool       g_audio_ok                = false;
static bool       g_music_on                = true;

static void audio_init(void) {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "SDL_mixer: %s\n", Mix_GetError());
        g_audio_ok = false;
        return;
    }
    Mix_AllocateChannels(8);
    g_audio_ok = true;
    printf("Audio initialized. Drop MIDI/OGG files and load them here.\n");
    /*
     * To add music, uncomment and adjust:
     *   g_bg_music = Mix_LoadMUS("music/theme.mid");
     *   g_stings[SND_MOVE]      = Mix_LoadWAV("sounds/move.wav");
     *   g_stings[SND_CAPTURE]   = Mix_LoadWAV("sounds/capture.wav");
     *   g_stings[SND_CHECK]     = Mix_LoadWAV("sounds/check.wav");
     *   g_stings[SND_CHECKMATE] = Mix_LoadWAV("sounds/checkmate.wav");
     *   g_stings[SND_STALEMATE] = Mix_LoadWAV("sounds/stalemate.wav");
     *   g_stings[SND_NEWGAME]   = Mix_LoadWAV("sounds/newgame.wav");
     */
}

static void audio_play_event(SoundEvent ev) {
    if (!g_audio_ok || !g_music_on) return;
    if (g_stings[ev]) Mix_PlayChannel(-1, g_stings[ev], 0);
}

static void audio_start_bg(void) {
    if (!g_audio_ok || !g_music_on || !g_bg_music) return;
    if (!Mix_PlayingMusic()) Mix_PlayMusic(g_bg_music, -1);
}

static void audio_toggle(void) {
    g_music_on = !g_music_on;
    if (!g_audio_ok) return;
    if (g_music_on) audio_start_bg();
    else Mix_HaltMusic();
}

static void audio_shutdown(void) {
    Mix_HaltMusic();
    Mix_HaltChannel(-1);
    if (g_bg_music) Mix_FreeMusic(g_bg_music);
    for (int i = 0; i < SND_COUNT; i++)
        if (g_stings[i]) Mix_FreeChunk(g_stings[i]);
    Mix_CloseAudio();
}

/* ============================================================================
 * File browser
 * ============================================================================ */

#define MAX_FILES 200

typedef struct {
    char name[256];
} FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    int       count;
    int       selected;
    int       scroll;
} FileBrowser;

static void fb_scan(FileBrowser *fb, const char *ext) {
    fb->count    = 0;
    fb->selected = 0;
    fb->scroll   = 0;
    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && fb->count < MAX_FILES) {
        char *dot = strrchr(e->d_name, '.');
        if (dot && strcasecmp(dot, ext) == 0) {
            strncpy(fb->entries[fb->count++].name, e->d_name, 255);
        }
    }
    closedir(d);
}

/* ============================================================================
 * Button helper
 * ============================================================================ */


static bool btn_hovered(const Button *b, int mx, int my) {
    return mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h;
}

static void btn_draw(SDL_Renderer *r, const Button *b, bool hover, bool active) {
    Uint8 br = hover ? 80  : (active ? 60 : 45);
    Uint8 bg_ = hover ? 80  : (active ? 60 : 45);
    Uint8 bb = hover ? 110 : (active ? 90 : 65);
    sdl_fill_rect(r, b->x, b->y, b->w, b->h, br, bg_, bb, 255);
    sdl_draw_rect(r, b->x, b->y, b->w, b->h, 100, 100, 140, 255);
    if (g_font_sm) {
        int tw, th;
        TTF_SizeUTF8(g_font_sm, b->label, &tw, &th);
        render_text(r, g_font_sm, b->label,
                    b->x + (b->w - tw)/2, b->y + (b->h - th)/2,
                    220, 220, 220);
    }
}

/* ============================================================================
 * Application state
 * ============================================================================ */

typedef enum {
    SCREEN_GAME,
    SCREEN_HELP,
    SCREEN_ABOUT,
    SCREEN_SAVE,
    SCREEN_LOAD
} Screen;

typedef struct {
    /* Game state (owned directly — no ChessGUI dependency) */
    ChessGameState    game;
    MoveHistory       move_history[MAX_MOVE_HISTORY * 2];
    int               move_history_count;
    ChessThinkingState thinking;

    /* Display */
    SDL_Window   *window;
    SDL_Renderer *renderer;
    Screen        screen;

    /* Board layout */
    bool  board_flipped;
    int   selected_row, selected_col;
    bool  piece_selected;
    int   last_from_row, last_from_col;
    int   last_to_row,   last_to_col;
    bool  has_last_move;

    /* Game mode */
    bool  player_vs_ai;
    bool  player_is_white;

    /* Status */
    char  status[256];
    bool  is_in_check;
    bool  is_checkmate;
    bool  is_stalemate;
    float check_timer;     /* countdown in seconds */

    /* Move counters / timers */
    int   move_count;
    Uint32 move_start_ms;
    double white_ms, black_ms;

    /* AI state */
    bool  ai_thinking;
    int   ai_delay_frames;
    int   ai_frame_counter;

    /* Input */
    int  mouse_x, mouse_y;
    bool mouse_down;
    bool mouse_was_down;

    /* File browser state */
    FileBrowser fb;
    char        fb_input[256];
    int         fb_input_len;

    /* Menu state */
    bool file_menu_open;
    bool help_menu_open;

    /* Running flag */
    bool running;
} App;

/* ============================================================================
 * Coordinate helpers
 * ============================================================================ */

static void board_square_rect(App *app, int row, int col, SDL_Rect *out) {
    int vis_row = app->board_flipped ? (7 - row) : row;
    int vis_col = app->board_flipped ? (7 - col) : col;
    out->x = BOARD_MARGIN_X + vis_col * SQUARE_SIZE;
    out->y = BOARD_MARGIN_Y + MENU_H + vis_row * SQUARE_SIZE;
    out->w = SQUARE_SIZE;
    out->h = SQUARE_SIZE;
}

static bool pixel_to_board(App *app, int px, int py, int *row, int *col) {
    int bx = px - BOARD_MARGIN_X;
    int by = py - (BOARD_MARGIN_Y + MENU_H);
    if (bx < 0 || bx >= 8*SQUARE_SIZE || by < 0 || by >= 8*SQUARE_SIZE) return false;
    int vc = bx / SQUARE_SIZE;
    int vr = by / SQUARE_SIZE;
    *col = app->board_flipped ? (7 - vc) : vc;
    *row = app->board_flipped ? (7 - vr) : vr;
    return true;
}

/* ============================================================================
 * Game helpers
 * ============================================================================ */

static void game_new(App *app) {
    chess_init_board(&app->game);
    app->selected_row    = -1;
    app->selected_col    = -1;
    app->piece_selected  = false;
    app->has_last_move   = false;
    app->is_in_check     = false;
    app->is_checkmate    = false;
    app->is_stalemate    = false;
    app->check_timer     = 0;
    app->move_count      = 0;
    app->white_ms        = 0;
    app->black_ms        = 0;
    app->move_start_ms   = SDL_GetTicks();
    app->ai_thinking     = false;
    app->ai_frame_counter = 0;
    app->game.turn   = WHITE;
    snprintf(app->status, sizeof(app->status), "New game — White to move");

    chess_start_thinking(&app->thinking, &app->game);
    audio_play_event(SND_NEWGAME);
}

static void check_game_over(App *app) {
    ChessGameStatus st = chess_check_game_status(&app->game);
    if (st == CHESS_PLAYING) return;

    if (st == CHESS_CHECKMATE_WHITE || st == CHESS_CHECKMATE_BLACK) {
        app->is_checkmate = true;
        const char *winner = (st == CHESS_CHECKMATE_BLACK) ? "White" : "Black";
        snprintf(app->status, sizeof(app->status), "Checkmate! %s wins!", winner);
        audio_play_event(SND_CHECKMATE);
    } else {
        app->is_stalemate = true;
        snprintf(app->status, sizeof(app->status), "Stalemate!");
        audio_play_event(SND_STALEMATE);
    }
}

static void commit_move(App *app, int fr, int fc, int tr, int tc) {
    bool was_capture = (app->game.board[tr][tc].type != EMPTY);

    ChessMove mv = {fr, fc, tr, tc, 0};
    chess_make_move(&app->game, mv);

    app->last_from_row = fr; app->last_from_col = fc;
    app->last_to_row   = tr; app->last_to_col   = tc;
    app->has_last_move = true;
    app->move_count++;

    /* Update timers */
    Uint32 now = SDL_GetTicks();
    double elapsed = (now - app->move_start_ms);
    ChessColor just_moved = (app->game.turn == WHITE) ? BLACK : WHITE;
    if (just_moved == WHITE) app->white_ms += elapsed;
    else                     app->black_ms += elapsed;
    app->move_start_ms = now;

    /* Save history */
    MoveHistory mh;
    mh.game_state  = app->game;
    mh.move        = mv;
    mh.time_elapsed = elapsed / 1000.0;
    if (app->move_history_count < MAX_MOVE_HISTORY * 2)
        app->move_history[app->move_history_count++] = mh;

    /* Check state */
    bool in_check = chess_is_in_check(&app->game, app->game.turn);
    if (in_check && !app->is_in_check) {
        app->is_in_check  = true;
        app->check_timer  = 1.5f;
        if (!app->is_checkmate) audio_play_event(SND_CHECK);
    } else if (!in_check) {
        app->is_in_check = false;
        app->check_timer = 0;
    }

    audio_play_event(was_capture ? SND_CAPTURE : SND_MOVE);

    check_game_over(app);
    if (!app->is_checkmate && !app->is_stalemate) {
        const char *turn = app->game.turn == WHITE ? "White" : "Black";
        snprintf(app->status, sizeof(app->status), "%s to move", turn);
        chess_start_thinking(&app->thinking, &app->game);
    }
}

static void undo_move(App *app) {
    if (app->is_checkmate || app->is_stalemate) return;

    int undo_count = app->player_vs_ai ? 2 : 1;
    if (app->move_history_count < undo_count) return;

    app->move_history_count -= undo_count;
    int restore = app->move_history_count;

    if (restore > 0) {
        app->game = app->move_history[restore - 1].game_state;
    } else {
        chess_init_board(&app->game);
    }

    app->has_last_move  = false;
    app->piece_selected = false;
    app->is_in_check    = false;
    app->is_checkmate   = false;
    app->is_stalemate   = false;
    app->check_timer    = 0;
    app->move_count     = app->move_history_count;
    snprintf(app->status, sizeof(app->status), "Move undone");

    chess_start_thinking(&app->thinking, &app->game);
}

/* ============================================================================
 * Drawing
 * ============================================================================ */

static void draw_board(App *app) {
    SDL_Renderer *r = app->renderer;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            SDL_Rect rc;
            board_square_rect(app, row, col, &rc);

            /* Base square colour */
            bool light = ((row + col) % 2 == 0);
            if (light) sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 240, 217, 181, 255);
            else        sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 181, 136,  99, 255);

            /* Last move highlight */
            if (app->has_last_move &&
                ((row == app->last_from_row && col == app->last_from_col) ||
                 (row == app->last_to_row   && col == app->last_to_col))) {
                sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 80, 200, 255, 120);
            }

            /* Selected piece highlight */
            if (app->piece_selected &&
                row == app->selected_row && col == app->selected_col) {
                sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 255, 255, 80, 180);
            }

            /* Check highlight on king */
            if (app->is_in_check || app->is_checkmate) {
                ChessPiece p = app->game.board[row][col];
                if (p.type == KING && p.color == app->game.turn) {
                    sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 220, 50, 50, 180);
                }
            }

            /* Piece */
            ChessPiece piece = app->game.board[row][col];
            if (piece.type != EMPTY) {
                SDL_Texture *tex = sdl_get_piece_texture(piece.type, piece.color);
                sdl_draw_piece(r, tex, rc.x + rc.w/2, rc.y + rc.h/2, rc.w - 8);
            }
        }
    }

    /* Board border */
    sdl_draw_rect(r, BOARD_MARGIN_X - 1, BOARD_MARGIN_Y + MENU_H - 1,
                  8*SQUARE_SIZE + 2, 8*SQUARE_SIZE + 2, 100, 80, 60, 255);

    /* File / rank labels */
    const char *files = "abcdefgh";
    const char *ranks = "87654321";
    for (int i = 0; i < 8; i++) {
        char buf[2] = {files[app->board_flipped ? 7-i : i], 0};
        render_text(r, g_font_sm, buf,
                    BOARD_MARGIN_X + i*SQUARE_SIZE + SQUARE_SIZE/2 - 4,
                    BOARD_MARGIN_Y + MENU_H + 8*SQUARE_SIZE + 2, 180, 160, 130);
        char buf2[2] = {ranks[app->board_flipped ? 7-i : i], 0};
        render_text(r, g_font_sm, buf2,
                    BOARD_MARGIN_X - 14,
                    BOARD_MARGIN_Y + MENU_H + i*SQUARE_SIZE + SQUARE_SIZE/2 - 7,
                    180, 160, 130);
    }
}

static void draw_panel(App *app) {
    SDL_Renderer *r = app->renderer;
    int x = PANEL_X, y = BOARD_MARGIN_Y + MENU_H;

    sdl_fill_rect(r, x - 4, y, PANEL_W + 4, 8*SQUARE_SIZE, 35, 35, 42, 255);

    /* Title */
    render_text_centered(r, g_font_lg, "BeatChess", x + PANEL_W/2, y + 6, 255, 220, 60);
    y += 36;

    /* Mode */
    const char *mode = app->player_vs_ai
        ? (app->player_is_white ? "Player(W) vs AI(B)" : "Player(B) vs AI(W)")
        : "AI vs AI";
    render_text(r, g_font_sm, mode, x, y, 160, 200, 230);
    y += 22;

    /* Turn */
    char buf[128];
    const char *turn = app->game.turn == WHITE ? "White" : "Black";
    snprintf(buf, sizeof(buf), "Turn: %s  Move: %d", turn, app->move_count);
    render_text(r, g_font_sm, buf, x, y, 200, 200, 200);
    y += 22;

    /* Status */
    render_text(r, g_font_sm, app->status, x, y,
                app->is_checkmate  ? 255 :
                app->is_stalemate  ? 180 :
                app->is_in_check   ? 220 : 160,
                app->is_in_check || app->is_checkmate  ? 80 : 180,
                app->is_checkmate  ?  80 : 200);
    y += 28;

    /* Timers */
    long wm = (long)app->white_ms, bm = (long)app->black_ms;
    snprintf(buf, sizeof(buf), "White: %ld:%02ld.%03ld",
             wm/60000, (wm%60000)/1000, wm%1000);
    render_text(r, g_font_sm, buf, x, y, 220, 220, 220);
    y += 18;
    snprintf(buf, sizeof(buf), "Black: %ld:%02ld.%03ld",
             bm/60000, (bm%60000)/1000, bm%1000);
    render_text(r, g_font_sm, buf, x, y, 180, 180, 180);
    y += 26;

    /* AI thinking indicator */
    if (app->ai_thinking) {
        render_text(r, g_font_sm, "AI thinking...", x, y, 100, 220, 255);
        y += 20;
    }

    /* Divider */
    sdl_fill_rect(r, x, y, PANEL_W, 1, 70, 70, 90, 255);
    y += 10;

    /* Buttons */
    Button buttons[] = {
        {x, y,      PANEL_W, 26, "N - New Game"},
        {x, y+30,   PANEL_W, 26, "U - Undo"},
        {x, y+60,   PANEL_W, 26, "A - Toggle AI Mode"},
        {x, y+90,   PANEL_W, 26, "B - Swap Color"},
        {x, y+120,  PANEL_W, 26, "F - Flip Board"},
        {x, y+150,  PANEL_W, 26, app->player_vs_ai ? "Player vs AI" : "AI vs AI"},
        {x, y+186,  PANEL_W, 26, g_music_on ? "M - Music: ON" : "M - Music: OFF"},
        {x, y+216,  PANEL_W, 26, "? - Help"},
        {x, y+252,  PANEL_W, 26, "Q - Quit"},
    };
    for (int i = 0; i < 9; i++) {
        btn_draw(r, &buttons[i],
                 btn_hovered(&buttons[i], app->mouse_x, app->mouse_y),
                 false);
    }
}

static void draw_menubar(App *app) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r, 0, 0, LOGICAL_W, MENU_H, 28, 28, 36, 255);

    /* File */
    bool fhov = (app->mouse_x < 60 && app->mouse_y < MENU_H);
    if (fhov || app->file_menu_open)
        sdl_fill_rect(r, 0, 0, 60, MENU_H, 55, 55, 70, 255);
    render_text(r, g_font_sm, "File", 6, 4, 210, 210, 210);

    /* Help */
    bool hhov = (app->mouse_x >= 60 && app->mouse_x < 120 && app->mouse_y < MENU_H);
    if (hhov || app->help_menu_open)
        sdl_fill_rect(r, 60, 0, 60, MENU_H, 55, 55, 70, 255);
    render_text(r, g_font_sm, "Help", 66, 4, 210, 210, 210);

    /* Title */
    render_text_centered(r, g_font_sm, "BeatChess SDL2 Edition", LOGICAL_W/2, 4, 255, 220, 60);

    /* File dropdown */
    if (app->file_menu_open) {
        const char *items[] = {"New Game (N)", "Undo (U)", "---",
                                "Save Game", "Load Game", "---", "Quit (Q)"};
        int iy = MENU_H;
        for (int i = 0; i < 7; i++) {
            if (items[i][0] == '-') {
                sdl_fill_rect(r, 0, iy, 180, 1, 70, 70, 90, 255);
                iy += 2; continue;
            }
            bool h = (app->mouse_x < 180 && app->mouse_y >= iy && app->mouse_y < iy+22);
            sdl_fill_rect(r, 0, iy, 180, 22, h ? 55 : 35, h ? 55 : 35, h ? 70 : 45, 255);
            render_text(r, g_font_sm, items[i], 8, iy+4, 210, 210, 210);
            iy += 22;
        }
    }

    /* Help dropdown */
    if (app->help_menu_open) {
        const char *items[] = {"Help (?)", "About"};
        int iy = MENU_H;
        for (int i = 0; i < 2; i++) {
            bool h = (app->mouse_x >= 60 && app->mouse_x < 240
                      && app->mouse_y >= iy && app->mouse_y < iy+22);
            sdl_fill_rect(r, 60, iy, 180, 22, h ? 55 : 35, h ? 55 : 35, h ? 70 : 45, 255);
            render_text(r, g_font_sm, items[i], 68, iy+4, 210, 210, 210);
            iy += 22;
        }
    }
}

static void draw_overlay_text(App *app) {
    if (!app->is_checkmate && !app->is_stalemate &&
        app->check_timer <= 0) return;

    SDL_Renderer *r = app->renderer;
    const char *msg = app->is_checkmate ? "CHECKMATE"
                    : app->is_stalemate ? "STALEMATE"
                    : "CHECK";
    Uint8 mr = app->is_checkmate ? 255 : app->is_stalemate ? 180 : 220;
    Uint8 mg = app->is_checkmate ?  60 : app->is_stalemate ? 180 :  80;
    Uint8 mb = app->is_checkmate ?  60 : app->is_stalemate ? 180 :  80;

    int bx = BOARD_MARGIN_X, by = BOARD_MARGIN_Y + MENU_H;
    int bw = 8*SQUARE_SIZE,  bh = 8*SQUARE_SIZE;
    int tw, th;
    TTF_SizeUTF8(g_font_lg, msg, &tw, &th);
    int ox = bx + (bw - tw)/2 - 16;
    int oy = by + (bh - th)/2 - 12;
    sdl_fill_rect(r, ox, oy, tw+32, th+24, 10, 10, 10, 210);
    sdl_draw_rect(r, ox, oy, tw+32, th+24, mr, mg, mb, 255);
    render_text_centered(r, g_font_lg, msg, bx + bw/2, oy+12, mr, mg, mb);
}

static void draw_help_screen(App *app) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r, 0, 0, LOGICAL_W, LOGICAL_H, 15, 15, 20, 240);

    int y = 50;
    render_text_centered(r, g_font_lg, "BeatChess — Help", LOGICAL_W/2, y, 255, 220, 60);
    y += 45;

    const char *lines[] = {
        "N          New Game",
        "U          Undo move (Player vs AI: undoes AI move too)",
        "A          Toggle AI vs AI / Player vs AI",
        "B          Swap player colour",
        "F          Flip board",
        "M          Toggle music on/off",
        "?          Show this help",
        "Q / Esc    Quit",
        "",
        "Mouse:  Click a piece to select, click destination to move.",
        "        File menu → Save/Load game (.sav format).",
        "",
        "Press any key or click to return.",
        NULL
    };
    for (int i = 0; lines[i]; i++) {
        if (lines[i][0])
            render_text(r, g_font_sm, lines[i], 120, y, 200, 200, 200);
        y += 20;
    }
}

static void draw_about_screen(App *app) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r, 0, 0, LOGICAL_W, LOGICAL_H, 15, 15, 20, 240);

    int y = 60;
    render_text_centered(r, g_font_lg, "BeatChess", LOGICAL_W/2, y, 255, 220, 60);
    y += 30;
    render_text_centered(r, g_font_med, "SDL2 Edition", LOGICAL_W/2, y, 180, 180, 200);
    y += 40;
    render_text_centered(r, g_font_sm, "Copyright (c) 2025 Jason Brian Hall", LOGICAL_W/2, y, 160, 200, 230);
    y += 25;
    render_text_centered(r, g_font_sm, "MIT License", LOGICAL_W/2, y, 100, 200, 100);
    y += 40;
    render_text_centered(r, g_font_sm, "Press any key or click to return.", LOGICAL_W/2, y, 140, 140, 160);
}

static void draw_file_dialog(App *app, bool is_save) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r, 0, 0, LOGICAL_W, LOGICAL_H, 15, 15, 20, 230);

    const char *title = is_save ? "Save Game" : "Load Game";
    render_text_centered(r, g_font_lg, title, LOGICAL_W/2, 20, 255, 220, 60);

    /* File list */
    int lx = 60, ly = 60, lw = LOGICAL_W - 120, lh = 360;
    sdl_draw_rect(r, lx, ly, lw, lh, 80, 80, 110, 255);

    int visible = lh / 24;
    for (int i = 0; i < visible && (i + app->fb.scroll) < app->fb.count; i++) {
        int fi = i + app->fb.scroll;
        bool sel = (fi == app->fb.selected);
        int ry = ly + i * 24;
        if (sel) sdl_fill_rect(r, lx+1, ry, lw-2, 24, 60, 80, 120, 255);
        render_text(r, g_font_sm, app->fb.entries[fi].name, lx+10, ry+5,
                    sel ? 255 : 200, sel ? 255 : 200, sel ? 255 : 200);
    }
    if (app->fb.count == 0) {
        render_text_centered(r, g_font_sm, "(no .sav files found)", LOGICAL_W/2, ly + lh/2 - 10,
                             140, 140, 160);
    }

    /* Filename input (save mode) */
    if (is_save) {
        render_text(r, g_font_sm, "Filename:", lx, ly+lh+16, 180, 180, 180);
        sdl_draw_rect(r, lx, ly+lh+32, 400, 26, 100, 100, 140, 255);
        char disp[260];
        snprintf(disp, sizeof(disp), "%s_", app->fb_input);
        render_text(r, g_font_sm, disp, lx+6, ly+lh+37, 220, 220, 100);
    }

    render_text(r, g_font_sm,
                is_save ? "Enter / click: Save    Esc: Cancel    Up/Down: Browse"
                        : "Enter / double-click: Load    Esc: Cancel    Up/Down: Browse",
                lx, ly+lh+62, 120, 180, 120);
}

/* ============================================================================
 * Main draw dispatch
 * ============================================================================ */

static void draw_frame(App *app) {
    SDL_Renderer *r = app->renderer;

    /* Background */
    sdl_fill_rect(r, 0, 0, LOGICAL_W, LOGICAL_H, 20, 20, 25, 255);

    switch (app->screen) {
        case SCREEN_HELP:  draw_help_screen(app);  break;
        case SCREEN_ABOUT: draw_about_screen(app); break;
        case SCREEN_SAVE:  draw_file_dialog(app, true);  break;
        case SCREEN_LOAD:  draw_file_dialog(app, false); break;
        case SCREEN_GAME:
            draw_board(app);
            draw_panel(app);
            draw_overlay_text(app);
            draw_menubar(app);
            break;
    }

    SDL_RenderPresent(r);
}

/* ============================================================================
 * Input handling
 * ============================================================================ */

static void handle_game_key(App *app, SDL_Keycode key) {
    switch (key) {
        case SDLK_n: game_new(app); break;
        case SDLK_u: undo_move(app); break;
        case SDLK_a:
            app->player_vs_ai = !app->player_vs_ai;
            game_new(app);
            break;
        case SDLK_b:
            app->player_is_white = !app->player_is_white;
            game_new(app);
            break;
        case SDLK_f:
            app->board_flipped = !app->board_flipped;
            break;
        case SDLK_m:
            audio_toggle();
            break;
        case SDLK_SLASH:
        case SDLK_QUESTION:
            app->screen = SCREEN_HELP;
            break;
        case SDLK_q:
        case SDLK_ESCAPE:
            app->running = false;
            break;
        default: break;
    }
}

static void handle_board_click(App *app, int px, int py) {
    if (app->is_checkmate || app->is_stalemate) return;

    /* Determine whose turn it is and whether it's the player's */
    ChessColor player_color = app->player_is_white ? WHITE : BLACK;
    if (app->player_vs_ai && app->game.turn != player_color) return;

    int row, col;
    if (!pixel_to_board(app, px, py, &row, &col)) return;

    ChessPiece piece = app->game.board[row][col];

    if (!app->piece_selected) {
        /* Select a piece */
        ChessColor turn_color = app->player_vs_ai ? player_color : app->game.turn;
        if (piece.type != EMPTY && piece.color == turn_color) {
            app->selected_row   = row;
            app->selected_col   = col;
            app->piece_selected = true;
        }
    } else {
        /* Attempt move */
        int fr = app->selected_row, fc = app->selected_col;
        if (fr == row && fc == col) {
            app->piece_selected = false;
            return;
        }

        /* Re-select if same colour */
        ChessColor turn_color = app->player_vs_ai ? player_color : app->game.turn;
        if (piece.type != EMPTY && piece.color == turn_color) {
            app->selected_row = row;
            app->selected_col = col;
            return;
        }

        if (chess_is_valid_move(&app->game, fr, fc, row, col)) {
            ChessGameState tmp = app->game;
            ChessMove mv = {fr, fc, row, col, 0};
            chess_make_move(&tmp, mv);
            if (!chess_is_in_check(&tmp, app->game.turn)) {
                commit_move(app, fr, fc, row, col);
            } else {
                snprintf(app->status, sizeof(app->status), "Illegal — king in check");
            }
        } else {
            snprintf(app->status, sizeof(app->status), "Illegal move");
        }
        app->piece_selected = false;
    }
}

static void handle_menu_click(App *app, int px, int py) {
    /* Clicking File menu button */
    if (py < MENU_H) {
        if (px < 60) {
            app->file_menu_open = !app->file_menu_open;
            app->help_menu_open = false;
            return;
        }
        if (px >= 60 && px < 120) {
            app->help_menu_open = !app->help_menu_open;
            app->file_menu_open = false;
            return;
        }
        app->file_menu_open = app->help_menu_open = false;
        return;
    }

    /* File dropdown items */
    if (app->file_menu_open && px < 180) {
        int item_heights[] = {22, 22, 2, 22, 22, 2, 22};
        int iy = MENU_H;
        for (int i = 0; i < 7; i++) {
            if (py >= iy && py < iy + item_heights[i]) {
                app->file_menu_open = false;
                switch (i) {
                    case 0: game_new(app); break;
                    case 1: undo_move(app); break;
                    case 3:
                        fb_scan(&app->fb, ".sav");
                        app->fb_input[0] = 0;
                        app->fb_input_len = 0;
                        app->screen = SCREEN_SAVE;
                        break;
                    case 4:
                        fb_scan(&app->fb, ".sav");
                        app->screen = SCREEN_LOAD;
                        break;
                    case 6: app->running = false; break;
                }
                return;
            }
            iy += item_heights[i];
        }
        app->file_menu_open = false;
        return;
    }

    /* Help dropdown items */
    if (app->help_menu_open && px >= 60 && px < 240) {
        int iy = MENU_H;
        for (int i = 0; i < 2; i++) {
            if (py >= iy && py < iy+22) {
                app->help_menu_open = false;
                if (i == 0) app->screen = SCREEN_HELP;
                else        app->screen = SCREEN_ABOUT;
                return;
            }
            iy += 22;
        }
        app->help_menu_open = false;
        return;
    }

    /* Close menus if click outside */
    if (app->file_menu_open || app->help_menu_open) {
        app->file_menu_open = app->help_menu_open = false;
        return;
    }

    /* Panel buttons — check if click in panel y range */
    int panel_y_start = BOARD_MARGIN_Y + MENU_H + 36 + 22 + 22 + 28 + 18 + 18 + 26 + 10;
    if (px >= PANEL_X && px < LOGICAL_W) {
        int oy = panel_y_start;
        int btn_ys[] = {0, 30, 60, 90, 120, 150, 186, 216, 252};
        const char btn_keys[] = {'n','u','a','b','f','t','m','?','q'};
        for (int i = 0; i < 9; i++) {
            if (py >= oy + btn_ys[i] && py < oy + btn_ys[i] + 26) {
                SDL_Keycode k;
                switch (btn_keys[i]) {
                    case 'n': k = SDLK_n; break;
                    case 'u': k = SDLK_u; break;
                    case 'a': k = SDLK_a; break;
                    case 'b': k = SDLK_b; break;
                    case 'f': k = SDLK_f; break;
                    case 't': k = SDLK_a; break; /* toggle same as A */
                    case 'm': k = SDLK_m; break;
                    case '?': k = SDLK_QUESTION; break;
                    case 'q': k = SDLK_q; break;
                    default:  k = SDLK_UNKNOWN; break;
                }
                if (k != SDLK_UNKNOWN) handle_game_key(app, k);
                return;
            }
        }
        return;
    }

    /* Board click */
    handle_board_click(app, px, py);
}

static void handle_dialog_key(App *app, SDL_Keycode key, bool is_save) {
    switch (key) {
        case SDLK_ESCAPE:
            app->screen = SCREEN_GAME;
            break;
        case SDLK_UP:
            if (app->fb.selected > 0) {
                app->fb.selected--;
                if (app->fb.selected < app->fb.scroll) app->fb.scroll = app->fb.selected;
            }
            break;
        case SDLK_DOWN:
            if (app->fb.selected < app->fb.count - 1) {
                app->fb.selected++;
                if (app->fb.selected >= app->fb.scroll + 15) app->fb.scroll++;
            }
            break;
        case SDLK_RETURN: {
            if (is_save) {
                /* Build filename */
                char fn[300];
                if (app->fb_input_len > 0) {
                    snprintf(fn, sizeof(fn), "%s", app->fb_input);
                } else if (app->fb.count > 0) {
                    snprintf(fn, sizeof(fn), "%s", app->fb.entries[app->fb.selected].name);
                } else {
                    break;
                }
                /* Ensure .sav extension */
                if (!strstr(fn, ".sav")) strncat(fn, ".sav", sizeof(fn)-strlen(fn)-1);

                BeatChessVisualization vis;
                vis.game = app->game;
                vis.move_history_count = app->move_history_count;
                vis.move_count = app->move_count;
                for (int i = 0; i < app->move_history_count; i++)
                    vis.move_history[i] = app->move_history[i];
                pgn_export_game(&vis, fn, "Player", "BeatChess AI");
                snprintf(app->status, sizeof(app->status), "Saved: %s", fn);
            } else {
                if (app->fb.count == 0) break;
                char *fn = app->fb.entries[app->fb.selected].name;
                BeatChessVisualization vis;
                memset(&vis, 0, sizeof(vis));
                if (pgn_import_game(&vis, fn)) {
                    app->game = vis.game;
                    app->move_history_count = vis.move_history_count;
                    app->move_count = vis.move_count;
                    for (int i = 0; i < vis.move_history_count; i++)
                        app->move_history[i] = vis.move_history[i];
                    snprintf(app->status, sizeof(app->status), "Loaded: %s", fn);
                    chess_start_thinking(&app->thinking, &app->game);
                }
            }
            app->screen = SCREEN_GAME;
            break;
        }
        case SDLK_BACKSPACE:
            if (is_save && app->fb_input_len > 0)
                app->fb_input[--app->fb_input_len] = 0;
            break;
        default: break;
    }
}

static void handle_dialog_text(App *app, const char *text, bool is_save) {
    if (!is_save) return;
    int len = strlen(text);
    for (int i = 0; i < len && app->fb_input_len < 250; i++) {
        char c = text[i];
        if (c >= 32 && c < 127) {
            app->fb_input[app->fb_input_len++] = c;
            app->fb_input[app->fb_input_len]   = 0;
        }
    }
}

/* ============================================================================
 * AI update (called every frame)
 * ============================================================================ */

static void update_ai(App *app, float dt) {
    if (app->is_checkmate || app->is_stalemate) return;

    ChessColor player_color = app->player_is_white ? WHITE : BLACK;
    bool ai_turn = !app->player_vs_ai ||
                   (app->player_vs_ai && app->game.turn != player_color);
    if (!ai_turn) return;

    /* Poll thinking state */
#if BEATCHESS_HAS_PTHREAD
    pthread_mutex_lock(&app->thinking.lock);
#endif
    bool has_move    = app->thinking.has_move;
    int  depth       = app->thinking.current_depth;
    bool is_thinking = app->thinking.thinking;
#if BEATCHESS_HAS_PTHREAD
    pthread_mutex_unlock(&app->thinking.lock);
#endif

    app->ai_thinking = is_thinking || has_move;

    /* Wait at least a brief moment and minimum depth before playing */
    if (!has_move) return;
    app->ai_frame_counter++;
    if (app->ai_frame_counter < app->ai_delay_frames) return;
    if (depth < 2 && app->ai_frame_counter < app->ai_delay_frames * 3) return;

    /* Get and execute move */
    ChessMove mv = chess_get_best_move_now(&app->thinking);

    if (!chess_is_valid_move(&app->game,
                             mv.from_row, mv.from_col,
                             mv.to_row,   mv.to_col)) {
        chess_start_thinking(&app->thinking, &app->game);
        app->ai_frame_counter = 0;
        return;
    }

    ChessGameState tmp = app->game;
    chess_make_move(&tmp, mv);
    if (chess_is_in_check(&tmp, app->game.turn)) {
        chess_start_thinking(&app->thinking, &app->game);
        app->ai_frame_counter = 0;
        return;
    }

    commit_move(app, mv.from_row, mv.from_col, mv.to_row, mv.to_col);
    app->ai_thinking     = false;
    app->ai_frame_counter = 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    /* App allocation */
    App *app = (App *)calloc(1, sizeof(App));
    if (!app) { fprintf(stderr, "OOM\n"); return 1; }

    /* Window */
    app->window = SDL_CreateWindow(
        "BeatChess SDL2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_RESIZABLE);
    if (!app->window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }

    app->renderer = SDL_CreateRenderer(app->window, -1,
                                        SDL_RENDERER_ACCELERATED |
                                        SDL_RENDERER_PRESENTVSYNC);
    if (!app->renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }

    /* Logical resolution so layout is always 900x640 regardless of window size */
    SDL_RenderSetLogicalSize(app->renderer, LOGICAL_W, LOGICAL_H);

    /* Load piece sprites from embedded BMP data */
    if (sdl_load_chess_pieces(app->renderer) != 0) {
        fprintf(stderr, "Warning: piece sprites failed to load, using fallback\n");
    }

    /* Splash screen */
    sdl_show_splashscreen(app->renderer, LOGICAL_W, LOGICAL_H);

    /* Fonts */
    fonts_init();

    /* Audio */
    audio_init();

    /* Chess engine */
    chess_init_zobrist();
    chess_clear_transposition_table();
    chess_init_thinking_state(&app->thinking);

    /* Default mode: Player vs AI, player is White */
    app->player_vs_ai    = true;
    app->player_is_white = true;
    app->ai_delay_frames  = 20;   /* ~333ms at 60fps before AI plays */
    app->screen           = SCREEN_GAME;
    app->running          = true;

    game_new(app);
    audio_start_bg();

    Uint32 last_ticks = SDL_GetTicks();

    SDL_StartTextInput();

    while (app->running) {
        Uint32 now = SDL_GetTicks();
        float dt   = (now - last_ticks) / 1000.0f;
        last_ticks = now;

        /* ---- Events ---- */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    app->running = false;
                    break;

                case SDL_KEYDOWN: {
                    SDL_Keycode k = ev.key.keysym.sym;
                    if (app->screen == SCREEN_HELP ||
                        app->screen == SCREEN_ABOUT) {
                        app->screen = SCREEN_GAME;
                        break;
                    }
                    if (app->screen == SCREEN_SAVE) {
                        handle_dialog_key(app, k, true);  break;
                    }
                    if (app->screen == SCREEN_LOAD) {
                        handle_dialog_key(app, k, false); break;
                    }
                    if (k == SDLK_ESCAPE) {
                        /* Close menus first */
                        if (app->file_menu_open || app->help_menu_open) {
                            app->file_menu_open = app->help_menu_open = false;
                        } else {
                            app->running = false;
                        }
                        break;
                    }
                    handle_game_key(app, k);
                    break;
                }

                case SDL_TEXTINPUT:
                    if (app->screen == SCREEN_SAVE)
                        handle_dialog_text(app, ev.text.text, true);
                    break;

                case SDL_MOUSEMOTION:
                    app->mouse_x = ev.motion.x;
                    app->mouse_y = ev.motion.y;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    app->mouse_x    = ev.button.x;
                    app->mouse_y    = ev.button.y;
                    app->mouse_down = true;
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        int px = ev.button.x, py = ev.button.y;
                        app->mouse_x = px;
                        app->mouse_y = py;

                        if (app->screen == SCREEN_HELP ||
                            app->screen == SCREEN_ABOUT) {
                            app->screen = SCREEN_GAME;
                            break;
                        }
                        if (app->screen == SCREEN_SAVE ||
                            app->screen == SCREEN_LOAD) {
                            /* Clicking file list */
                            int ly = 60, lh = 360;
                            if (px >= 60 && px < LOGICAL_W - 60 &&
                                py >= ly && py < ly + lh) {
                                int idx = (py - ly) / 24 + app->fb.scroll;
                                if (idx >= 0 && idx < app->fb.count) {
                                    if (idx == app->fb.selected &&
                                        app->screen == SCREEN_LOAD) {
                                        /* Double-click: load immediately */
                                        handle_dialog_key(app, SDLK_RETURN, false);
                                    } else {
                                        app->fb.selected = idx;
                                    }
                                }
                            }
                            break;
                        }

                        handle_menu_click(app, px, py);
                    }
                    app->mouse_down = false;
                    break;

                case SDL_MOUSEWHEEL:
                    if (app->screen == SCREEN_SAVE ||
                        app->screen == SCREEN_LOAD) {
                        app->fb.scroll -= ev.wheel.y;
                        if (app->fb.scroll < 0) app->fb.scroll = 0;
                        if (app->fb.scroll > app->fb.count - 1)
                            app->fb.scroll = app->fb.count > 0 ? app->fb.count - 1 : 0;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_CLOSE)
                        app->running = false;
                    break;
            }
        }

        /* ---- Update ---- */
        if (app->screen == SCREEN_GAME) {
            /* Check timer */
            if (app->check_timer > 0) {
                app->check_timer -= dt;
                if (app->check_timer < 0) app->check_timer = 0;
            }

            /* Update live check state */
            if (!app->is_checkmate && !app->is_stalemate) {
                bool ic = chess_is_in_check(&app->game, app->game.turn);
                if (ic && !app->is_in_check) {
                    app->is_in_check  = true;
                    app->check_timer  = 1.5f;
                } else if (!ic) {
                    app->is_in_check = false;
                }
            }

            update_ai(app, dt);
        }

        /* ---- Draw ---- */
        draw_frame(app);
    }

    /* Cleanup */
    SDL_StopTextInput();
    chess_cleanup_thinking_state(&app->thinking);
    audio_shutdown();
    sdl_destroy_chess_pieces();
    fonts_shutdown();
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    free(app);
    SDL_Quit();
    return 0;
}
