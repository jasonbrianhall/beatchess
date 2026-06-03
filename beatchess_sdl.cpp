/*
 * beatchess_sdl.cpp - BeatChess SDL2 Edition
 * Linux and Windows. No Allegro dependency.
 *
 * Features: piece animation, legal move hints, captured pieces display,
 *           pawn promotion UI, resign button, custom cursors, procedural SFX.
 *
 * Keyboard shortcuts:
 *   N        - New game
 *   U        - Undo
 *   A        - Toggle AI vs AI / Player vs AI
 *   B        - Swap player color
 *   F        - Flip board
 *   M        - Toggle music
 *   R        - Resign
 *   ?        - Help
 *   Q / Esc  - Quit
 */

#include "beatchess.h"
#include "visualization.h"
#include "chess_ai_move.h"
#include "pgn.h"
#include "chess_pieces_loader_sdl.h"
#include "chess_sound.h"
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

/* ---- Dynamic layout scaled from the window size ----
 * Base design is 900x640. All layout values scale from g_scale.
 * g_scale is set at startup and on every window resize. */
static float g_scale = 1.0f;

/* Logical resolution tracks the actual window size */
static int LOGICAL_W = 900;
static int LOGICAL_H = 640;

/* Layout constants — recalculated by layout_update() on resize */
static int BOARD_MARGIN_X = 60;
static int BOARD_MARGIN_Y = 40;
static int SQUARE_SIZE    = 68;
static int PANEL_X        = 60 + 8*68 + 12;
static int PANEL_W        = 900 - (60 + 8*68 + 12) - 8;
static int MENU_H         = 22;

/* Font point sizes at current scale */
static int FONT_SM  = 13;
static int FONT_MED = 17;
static int FONT_LG  = 22;

#define ANIM_SPEED      3.0f

static void layout_update(int win_w, int win_h) {
    LOGICAL_W = win_w;
    LOGICAL_H = win_h;
    g_scale   = win_w / 900.0f;

    BOARD_MARGIN_X = (int)(60  * g_scale);
    BOARD_MARGIN_Y = (int)(40  * g_scale);
    MENU_H         = (int)(22  * g_scale);

    /* Square size: fit 8 squares in the shorter of (height-margins, width*0.6) */
    float max_sq_h = (win_h - BOARD_MARGIN_Y - MENU_H - (int)(20*g_scale)) / 8.0f;
    float max_sq_w = (win_w * 0.62f) / 8.0f;
    SQUARE_SIZE    = (int)fminf(max_sq_h, max_sq_w);

    PANEL_X = BOARD_MARGIN_X + 8 * SQUARE_SIZE + (int)(12 * g_scale);
    PANEL_W = LOGICAL_W - PANEL_X - (int)(8 * g_scale);

    FONT_SM  = (int)(13 * g_scale);
    FONT_MED = (int)(17 * g_scale);
    FONT_LG  = (int)(22 * g_scale);
    if (FONT_SM  < 9)  FONT_SM  = 9;
    if (FONT_MED < 12) FONT_MED = 12;
    if (FONT_LG  < 16) FONT_LG  = 16;
}

/* ============================================================================
 * Primitive drawing helpers
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

static void sdl_fill_circle(SDL_Renderer *r, int cx, int cy, int radius,
                              Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_SetRenderDrawBlendMode(r, A < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius*radius - dy*dy));
        SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
    }
}

/* ============================================================================
 * Text rendering
 * ============================================================================ */

static TTF_Font *g_font_sm  = NULL;
static TTF_Font *g_font_med = NULL;
static TTF_Font *g_font_lg  = NULL;

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
            signed char v2 = (buf[2]=='=') ? 0 : T[(unsigned char)buf[2]];
            signed char v3 = (buf[3]=='=') ? 0 : T[(unsigned char)buf[3]];
            if (di < decoded_len) dst[di++] = (v0<<2)|(v1>>4);
            if (buf[2]!='=' && di < decoded_len) dst[di++] = (v1<<4)|(v2>>2);
            if (buf[3]!='=' && di < decoded_len) dst[di++] = (v2<<6)|v3;
            bi = 0;
        }
    }
    *out_len = di;
    return dst;
}

static unsigned char *g_font_buf = NULL;

static bool fonts_init(void) {
    if (TTF_Init() != 0) return false;
    size_t font_len = 0;
    g_font_buf = b64_decode(DEJAVU_REGULAR_FONT_B64, DEJAVU_REGULAR_FONT_B64_SIZE, &font_len);
    if (!g_font_buf) return false;
    g_font_sm  = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf, (int)font_len), 0, FONT_SM);
    g_font_med = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf, (int)font_len), 0, FONT_MED);
    g_font_lg  = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf, (int)font_len), 0, FONT_LG);
    return g_font_sm && g_font_med && g_font_lg;
}

static void fonts_rebuild(void) {
    if (!g_font_buf) return;
    if (g_font_sm)  { TTF_CloseFont(g_font_sm);  g_font_sm  = NULL; }
    if (g_font_med) { TTF_CloseFont(g_font_med); g_font_med = NULL; }
    if (g_font_lg)  { TTF_CloseFont(g_font_lg);  g_font_lg  = NULL; }
    size_t font_len = (size_t)(DEJAVU_REGULAR_FONT_B64_SIZE * 3 / 4);
    g_font_sm  = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf, (int)font_len), 0, FONT_SM);
    g_font_med = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf, (int)font_len), 0, FONT_MED);
    g_font_lg  = TTF_OpenFontRW(SDL_RWFromMem(g_font_buf, (int)font_len), 0, FONT_LG);
}

static void fonts_shutdown(void) {
    if (g_font_sm)  TTF_CloseFont(g_font_sm);
    if (g_font_med) TTF_CloseFont(g_font_med);
    if (g_font_lg)  TTF_CloseFont(g_font_lg);
    TTF_Quit();
    if (g_font_buf) { free(g_font_buf); g_font_buf = NULL; }
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
 * Audio — uses chess_sound.cpp procedural SFX
 * ============================================================================ */

static bool  g_music_on = true;
static Mix_Music *g_bg_music = NULL;

static void audio_init(void) {
    chess_sound_init();
}

static void audio_play_move(void)      { chess_sound_play(SFX_MOVE); }
static void audio_play_capture(void)   { chess_sound_play(SFX_CAPTURE); }
static void audio_play_check(void)     { chess_sound_play(SFX_CHECK); }
static void audio_play_checkmate(void) { chess_sound_play(SFX_CHECKMATE); }
static void audio_play_resign(void)    { chess_sound_play(SFX_RESIGN); }

static void audio_toggle(void) {
    g_music_on = !g_music_on;
    if (g_bg_music) {
        if (g_music_on) Mix_PlayMusic(g_bg_music, -1);
        else Mix_HaltMusic();
    }
}

static void audio_shutdown(void) {
    if (g_bg_music) { Mix_FreeMusic(g_bg_music); g_bg_music = NULL; }
    chess_sound_shutdown();
}

/* ============================================================================
 * File browser
 * ============================================================================ */

#define MAX_FILES 200

typedef struct { char name[256]; } FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    int count, selected, scroll;
} FileBrowser;

static void fb_scan(FileBrowser *fb, const char *ext) {
    fb->count = fb->selected = fb->scroll = 0;
    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && fb->count < MAX_FILES) {
        char *dot = strrchr(e->d_name, '.');
        if (dot && strcasecmp(dot, ext) == 0)
            strncpy(fb->entries[fb->count++].name, e->d_name, 255);
    }
    closedir(d);
}

/* ============================================================================
 * Button helper
 * ============================================================================ */

static bool btn_hovered(const Button *b, int mx, int my) {
    return mx >= b->x && mx < b->x+b->w && my >= b->y && my < b->y+b->h;
}

static void btn_draw(SDL_Renderer *r, const Button *b, bool hover, bool active,
                     Uint8 ar=45, Uint8 ag=45, Uint8 ab=65) {
    Uint8 br = hover ? (Uint8)(ar+35) : active ? (Uint8)(ar+15) : ar;
    Uint8 bg_ = hover ? (Uint8)(ag+35) : active ? (Uint8)(ag+15) : ag;
    Uint8 bb = hover ? (Uint8)(ab+45) : active ? (Uint8)(ab+25) : ab;
    sdl_fill_rect(r, b->x, b->y, b->w, b->h, br, bg_, bb, 255);
    sdl_draw_rect(r, b->x, b->y, b->w, b->h, 100, 100, 140, 255);
    if (g_font_sm && b->label) {
        int tw, th;
        TTF_SizeUTF8(g_font_sm, b->label, &tw, &th);
        render_text(r, g_font_sm, b->label,
                    b->x+(b->w-tw)/2, b->y+(b->h-th)/2, 220, 220, 220);
    }
}

/* ============================================================================
 * Application state
 * ============================================================================ */

typedef enum {
    SCREEN_GAME, SCREEN_HELP, SCREEN_ABOUT, SCREEN_SAVE, SCREEN_LOAD
} Screen;

typedef struct {
    /* Core game state */
    ChessGameState     game;
    MoveHistory        move_history[MAX_MOVE_HISTORY * 2];
    int                move_history_count;
    ChessThinkingState thinking;

    /* SDL */
    SDL_Window   *window;
    SDL_Renderer *renderer;
    Screen        screen;

    /* Board */
    bool  board_flipped;
    int   selected_row, selected_col;
    bool  piece_selected;
    int   last_from_row, last_from_col;
    int   last_to_row, last_to_col;
    bool  has_last_move;
    int   hover_row, hover_col;  /* square under mouse, -1 if none */

    /* Legal move hints */
    bool  legal_dests[8][8];     /* squares reachable from selected piece */
    bool  danger_squares[8][8];  /* legal_dest squares attacked by opponent */
    bool  check_squares[8][8];   /* legal_dest squares that put opponent king in check */

    /* Game mode */
    bool  player_vs_ai;
    bool  player_is_white;

    /* Game status */
    char  status[256];
    bool  is_in_check;
    bool  is_checkmate;
    bool  is_stalemate;
    bool  resigned;
    float check_timer;

    /* Captured pieces */
    int   white_captured[7];  /* indexed by PieceType */
    int   black_captured[7];

    /* Timers */
    int    move_count;
    Uint32 move_start_ms;
    double white_ms, black_ms;

    /* Piece animation */
    bool  is_animating;
    float anim_progress;      /* 0..1 */
    int   anim_fr, anim_fc;   /* from square */
    int   anim_tr, anim_tc;   /* to square */
    SDL_Texture *anim_tex;    /* piece texture being animated */
    ChessGameState post_anim_game; /* game state to apply after anim finishes */
    bool  post_anim_pending;

    /* AI */
    bool  ai_thinking;
    float time_thinking;

    /* Pawn promotion */
    bool  awaiting_promotion;
    int   promo_row, promo_col;
    /* hover state for promotion buttons */
    bool  promo_queen_hov, promo_rook_hov, promo_bishop_hov, promo_knight_hov;

    /* Input */
    int  mouse_x, mouse_y;

    /* File browser */
    FileBrowser fb;
    char        fb_input[256];
    int         fb_input_len;

    /* Menus */
    bool file_menu_open;
    bool help_menu_open;
    bool confirm_new_game;  /* showing "are you sure?" dialog */

    bool running;
} App;

/* ============================================================================
 * Coordinate helpers
 * ============================================================================ */

static void board_square_rect(App *app, int row, int col, SDL_Rect *out) {
    int vr = app->board_flipped ? (7-row) : row;
    int vc = app->board_flipped ? (7-col) : col;
    out->x = BOARD_MARGIN_X + vc * SQUARE_SIZE;
    out->y = BOARD_MARGIN_Y + MENU_H + vr * SQUARE_SIZE;
    out->w = SQUARE_SIZE;
    out->h = SQUARE_SIZE;
}

/* pixel centre of a square */
static void square_centre(App *app, int row, int col, float *ox, float *oy) {
    SDL_Rect rc;
    board_square_rect(app, row, col, &rc);
    *ox = rc.x + rc.w / 2.0f;
    *oy = rc.y + rc.h / 2.0f;
}

static bool pixel_to_board(App *app, int px, int py, int *row, int *col) {
    int bx = px - BOARD_MARGIN_X;
    int by = py - (BOARD_MARGIN_Y + MENU_H);
    if (bx < 0 || bx >= 8*SQUARE_SIZE || by < 0 || by >= 8*SQUARE_SIZE) return false;
    *col = app->board_flipped ? (7 - bx/SQUARE_SIZE) : bx/SQUARE_SIZE;
    *row = app->board_flipped ? (7 - by/SQUARE_SIZE) : by/SQUARE_SIZE;
    return true;
}

/* ============================================================================
 * Cursor management
 * ============================================================================ */

static SDL_Cursor *g_cursor_arrow = NULL;
static SDL_Cursor *g_cursor_hand  = NULL;

static void cursors_init(void) {
    g_cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    g_cursor_hand  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
}

static void cursors_shutdown(void) {
    if (g_cursor_arrow) SDL_FreeCursor(g_cursor_arrow);
    if (g_cursor_hand)  SDL_FreeCursor(g_cursor_hand);
}

static void set_cursor(SDL_Cursor *c) {
    SDL_SetCursor(c);
}

/* ============================================================================
 * Legal move hint calculation
 * ============================================================================ */

/* Returns true if (row,col) can be captured by `attacker` in the given state */
static bool square_is_attacked(ChessGameState *game, int row, int col, ChessColor attacker) {
    ChessMove moves[256];
    int n = chess_get_all_moves(game, attacker, moves);
    for (int i = 0; i < n; i++) {
        if (moves[i].to_row == row && moves[i].to_col == col)
            return true;
    }
    return false;
}

static void calc_legal_dests(App *app) {
    memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
    if (!app->piece_selected) return;

    int fr = app->selected_row, fc = app->selected_col;
    ChessColor mover    = app->game.turn;
    ChessColor opponent = (mover == WHITE) ? BLACK : WHITE;

    ChessMove moves[256];
    int n = chess_get_all_moves(&app->game, mover, moves);
    for (int i = 0; i < n; i++) {
        if (moves[i].from_row != fr || moves[i].from_col != fc) continue;
        ChessGameState tmp = app->game;
        chess_make_move(&tmp, moves[i]);
        if (chess_is_in_check(&tmp, mover)) continue;  /* illegal */
        int tr = moves[i].to_row, tc = moves[i].to_col;
        app->legal_dests[tr][tc] = true;
        /* After moving there, can the opponent immediately recapture? */
        if (square_is_attacked(&tmp, tr, tc, opponent))
            app->danger_squares[tr][tc] = true;
        /* Does this move put the opponent king in check? */
        if (chess_is_in_check(&tmp, opponent))
            app->check_squares[tr][tc] = true;
    }
}

/* ============================================================================
 * Game helpers
 * ============================================================================ */

static void start_animation(App *app, int fr, int fc, int tr, int tc,
                             ChessGameState post_game) {
    app->is_animating     = true;
    app->anim_progress    = 0.0f;
    app->anim_fr = fr; app->anim_fc = fc;
    app->anim_tr = tr; app->anim_tc = tc;
    app->anim_tex         = sdl_get_piece_texture(app->game.board[fr][fc].type,
                                                   app->game.board[fr][fc].color);
    app->post_anim_game   = post_game;
    app->post_anim_pending = true;
}

static void game_new(App *app) {
    chess_init_board(&app->game);
    app->selected_row    = -1;
    app->selected_col    = -1;
    app->piece_selected  = false;
    app->has_last_move   = false;
    app->is_in_check     = false;
    app->is_checkmate    = false;
    app->is_stalemate    = false;
    app->resigned        = false;
    app->check_timer     = 0;
    app->move_count      = 0;
    app->white_ms        = 0;
    app->black_ms        = 0;
    app->move_start_ms   = SDL_GetTicks();
    app->ai_thinking     = false;
    app->time_thinking   = 0.0f;
    app->is_animating    = false;
    app->awaiting_promotion = false;
    app->hover_row       = -1;
    app->hover_col       = -1;
    memset(app->white_captured, 0, sizeof(app->white_captured));
    memset(app->black_captured, 0, sizeof(app->black_captured));
    memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
    app->move_history_count = 0;
    app->game.turn       = WHITE;
    snprintf(app->status, sizeof(app->status), "New game — White to move");
    chess_start_thinking(&app->thinking, &app->game);
}

static void check_game_over(App *app) {
    ChessGameStatus st = chess_check_game_status(&app->game);
    if (st == CHESS_PLAYING) return;
    if (st == CHESS_CHECKMATE_WHITE || st == CHESS_CHECKMATE_BLACK) {
        app->is_checkmate = true;
        const char *winner = (st == CHESS_CHECKMATE_BLACK) ? "White" : "Black";
        snprintf(app->status, sizeof(app->status), "Checkmate! %s wins!", winner);
        audio_play_checkmate();
    } else {
        app->is_stalemate = true;
        snprintf(app->status, sizeof(app->status), "Stalemate!");
    }
}

static void commit_move(App *app, int fr, int fc, int tr, int tc) {
    /* Check pawn promotion before animation */
    ChessPiece moving = app->game.board[fr][fc];
    bool is_promo = (moving.type == PAWN &&
                     ((moving.color == WHITE && tr == 7) ||
                      (moving.color == BLACK && tr == 0)));

    /* Check capture before the board is modified */
    bool was_capture = (app->game.board[tr][tc].type != EMPTY);

    /* Build post-animation game state */
    ChessGameState post = app->game;
    ChessMove mv = {fr, fc, tr, tc, 0};
    chess_make_move(&post, mv);

    /* Play sound immediately (don't wait for animation to finish) */
    if (was_capture) audio_play_capture();
    else             audio_play_move();

    /* Start animation */
    start_animation(app, fr, fc, tr, tc, post);

    if (is_promo) {
        /* We'll show the promotion dialog after animation completes */
        app->awaiting_promotion = true;
        app->promo_row = tr;
        app->promo_col = tc;
        /* Default to queen until player chooses */
        app->post_anim_game.board[tr][tc].type = QUEEN;
    }
}

static void finish_animation(App *app) {
    app->is_animating    = false;
    app->post_anim_pending = false;
    app->game = app->post_anim_game;

    /* If promotion, let player choose — game state already has queen default */
    if (app->awaiting_promotion) {
        /* Status updated in draw_promotion */
        snprintf(app->status, sizeof(app->status), "Choose promotion piece");
        return;
    }

    /* Update rest of game state */
    int fr = app->anim_fr, fc = app->anim_fc;
    int tr = app->anim_tr, tc = app->anim_tc;

    bool was_capture = false; /* already tracked before animation started */
    (void)was_capture;

    app->last_from_row = fr; app->last_from_col = fc;
    app->last_to_row   = tr; app->last_to_col   = tc;
    app->has_last_move = true;
    app->move_count++;
    app->piece_selected = false;
    memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));

    Uint32 now = SDL_GetTicks();
    double elapsed = now - app->move_start_ms;
    ChessColor just_moved = (app->game.turn == WHITE) ? BLACK : WHITE;
    if (just_moved == WHITE) app->white_ms += elapsed;
    else                     app->black_ms += elapsed;
    app->move_start_ms = now;

    MoveHistory mh;
    ChessMove mv = {fr, fc, tr, tc, 0};
    mh.game_state   = app->game;
    mh.move         = mv;
    mh.time_elapsed = elapsed / 1000.0;
    if (app->move_history_count < MAX_MOVE_HISTORY * 2)
        app->move_history[app->move_history_count++] = mh;

    bool in_check = chess_is_in_check(&app->game, app->game.turn);
    if (in_check) { app->is_in_check = true; app->check_timer = 1.5f; }
    else          { app->is_in_check = false; app->check_timer = 0; }

    check_game_over(app);
    if (!app->is_checkmate && !app->is_stalemate) {
        if (in_check) audio_play_check();
        const char *turn = app->game.turn == WHITE ? "White" : "Black";
        snprintf(app->status, sizeof(app->status), "%s to move", turn);
        chess_start_thinking(&app->thinking, &app->game);
    }
}

static void undo_move(App *app) {
    if (app->is_animating || app->awaiting_promotion) return;
    if (app->is_checkmate || app->is_stalemate || app->resigned) return;

    int undo_count = app->player_vs_ai ? 2 : 1;
    if (app->move_history_count < undo_count) return;

    /* Undo captured piece tracking */
    for (int u = 0; u < undo_count; u++) {
        if (app->move_history_count <= 0) break;
        MoveHistory *mh = &app->move_history[app->move_history_count - 1];
        /* piece on to_square in the pre-move state was captured */
        ChessPiece was_there = app->move_history[app->move_history_count-1].game_state.board
            [mh->move.to_row][mh->move.to_col];
        /* actually easier — just recalculate from history */
        (void)was_there;
        app->move_history_count--;
    }

    int restore = app->move_history_count;
    if (restore > 0)
        app->game = app->move_history[restore-1].game_state;
    else
        chess_init_board(&app->game);

    /* Recalculate captured from scratch */
    memset(app->white_captured, 0, sizeof(app->white_captured));
    memset(app->black_captured, 0, sizeof(app->black_captured));
    for (int i = 0; i < app->move_history_count; i++) {
        MoveHistory *mh = &app->move_history[i];
        /* Pre-move state is i==0 ? initial : move_history[i-1].game_state */
        ChessGameState *pre = (i == 0) ? NULL : &app->move_history[i-1].game_state;
        if (pre) {
            ChessPiece cap = pre->board[mh->move.to_row][mh->move.to_col];
            if (cap.type != EMPTY) {
                if (mh->game_state.turn == WHITE) /* black just moved, captured white */
                    app->black_captured[cap.type]++;
                else
                    app->white_captured[cap.type]++;
            }
        }
    }

    app->has_last_move   = false;
    app->piece_selected  = false;
    app->is_in_check     = false;
    app->is_checkmate    = false;
    app->is_stalemate    = false;
    app->resigned        = false;
    app->check_timer     = 0;
    app->move_count      = app->move_history_count;
    memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
    snprintf(app->status, sizeof(app->status), "Move undone");
    chess_start_thinking(&app->thinking, &app->game);
}

static void player_resign(App *app) {
    if (app->is_checkmate || app->is_stalemate || app->resigned) return;
    app->resigned = true;
    const char *loser = app->player_is_white ? "White" : "Black";
    snprintf(app->status, sizeof(app->status), "%s resigns.", loser);
    audio_play_resign();
}

/* ============================================================================
 * Drawing — board
 * ============================================================================ */

static void draw_board(App *app) {
    SDL_Renderer *r = app->renderer;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            SDL_Rect rc;
            board_square_rect(app, row, col, &rc);

            /* Base colour */
            bool light = ((row + col) % 2 == 0);
            if (light) sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 240, 217, 181, 255);
            else        sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 181, 136,  99, 255);

            /* Last move highlight */
            if (app->has_last_move &&
                ((row==app->last_from_row && col==app->last_from_col) ||
                 (row==app->last_to_row   && col==app->last_to_col)))
                sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 80, 200, 255, 100);

            /* Selected piece highlight */
            if (app->piece_selected && row==app->selected_row && col==app->selected_col)
                sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 255, 255, 80, 180);

            /* Check highlight */
            if (app->is_in_check || app->is_checkmate) {
                ChessPiece p = app->game.board[row][col];
                if (p.type == KING && p.color == app->game.turn)
                    sdl_fill_rect(r, rc.x, rc.y, rc.w, rc.h, 220, 50, 50, 180);
            }

            /* Legal move hints:
             *   dot/ring colour priority: check (yellow) > danger (red) > safe (dark) */
            if (app->piece_selected && app->legal_dests[row][col]) {
                ChessPiece p = app->game.board[row][col];
                bool danger = app->danger_squares[row][col];
                bool gives_check = app->check_squares[row][col];
                /* Colour: yellow=check, red=danger, dark=safe */
                Uint8 hr = gives_check ? 220 : danger ? 180 : 0;
                Uint8 hg = gives_check ? 200 : danger ?  40 : 0;
                Uint8 hb = gives_check ?  30 : danger ?  40 : 0;
                if (p.type == EMPTY) {
                    if (gives_check || danger)
                        sdl_fill_circle(r, rc.x+rc.w/2, rc.y+rc.h/2, rc.w/6, hr, hg, hb, 160);
                    else
                        sdl_fill_circle(r, rc.x+rc.w/2, rc.y+rc.h/2, rc.w/6, 0, 0, 0, 60);
                } else {
                    int s = rc.w/4;
                    Uint8 cr2 = gives_check ? hr : danger ? 200 : 80;
                    Uint8 cg2 = gives_check ? hg : danger ?  40 : 200;
                    Uint8 cb2 = gives_check ? hb : danger ?  40 : 80;
                    sdl_fill_rect(r, rc.x,         rc.y,         s, s, cr2, cg2, cb2, 160);
                    sdl_fill_rect(r, rc.x+rc.w-s,  rc.y,         s, s, cr2, cg2, cb2, 160);
                    sdl_fill_rect(r, rc.x,         rc.y+rc.h-s,  s, s, cr2, cg2, cb2, 160);
                    sdl_fill_rect(r, rc.x+rc.w-s,  rc.y+rc.h-s,  s, s, cr2, cg2, cb2, 160);
                }
            }

            /* Piece — skip animating piece's from-square */
            if (app->is_animating && row==app->anim_fr && col==app->anim_fc)
                continue;

            ChessPiece piece = app->game.board[row][col];
            if (piece.type != EMPTY) {
                SDL_Texture *tex = sdl_get_piece_texture(piece.type, piece.color);
                sdl_draw_piece(r, tex, rc.x+rc.w/2, rc.y+rc.h/2, rc.w-8);
            }
        }
    }

    /* Animated piece */
    if (app->is_animating && app->anim_tex) {
        float t = app->anim_progress;
        /* ease in-out */
        t = t * t * (3.0f - 2.0f * t);
        float fx, fy, tx, ty;
        square_centre(app, app->anim_fr, app->anim_fc, &fx, &fy);
        square_centre(app, app->anim_tr, app->anim_tc, &tx, &ty);
        float cx = fx + (tx - fx) * t;
        float cy = fy + (ty - fy) * t;
        sdl_draw_piece(r, app->anim_tex, (int)cx, (int)cy, SQUARE_SIZE - 8);
    }

    /* Board border */
    sdl_draw_rect(r, BOARD_MARGIN_X-1, BOARD_MARGIN_Y+MENU_H-1,
                  8*SQUARE_SIZE+2, 8*SQUARE_SIZE+2, 100, 80, 60, 255);

    /* File/rank labels */
    const char *files = "abcdefgh";
    const char *ranks = "87654321";
    for (int i = 0; i < 8; i++) {
        char buf[2] = {files[app->board_flipped ? 7-i : i], 0};
        render_text(r, g_font_sm, buf,
                    BOARD_MARGIN_X + i*SQUARE_SIZE + SQUARE_SIZE/2 - (int)(4*g_scale),
                    BOARD_MARGIN_Y + MENU_H + 8*SQUARE_SIZE + (int)(2*g_scale), 180, 160, 130);
        char buf2[2] = {ranks[app->board_flipped ? 7-i : i], 0};
        render_text(r, g_font_sm, buf2,
                    BOARD_MARGIN_X - (int)(14*g_scale),
                    BOARD_MARGIN_Y + MENU_H + i*SQUARE_SIZE + SQUARE_SIZE/2 - (int)(7*g_scale),
                    180, 160, 130);
    }
}

/* ============================================================================
 * Drawing — captured pieces strip
 * ============================================================================ */

static void draw_captured_strip(App *app, ChessColor captured_color,
                                  int strip_y, bool above) {
    /* Draw pieces captured from captured_color (i.e. owned by that color, now captured) */
    SDL_Renderer *r = app->renderer;
    int *counts = (captured_color == WHITE) ? app->white_captured : app->black_captured;
    int x = PANEL_X;
    int sz = (int)(18 * g_scale);
    for (int type = PAWN; type <= QUEEN; type++) {
        for (int n = 0; n < counts[type]; n++) {
            SDL_Texture *tex = sdl_get_piece_texture((PieceType)type, (ChessColor)captured_color);
            sdl_draw_piece(r, tex, x + sz/2, strip_y + sz/2, sz);
            x += sz + 2;
        }
    }
    (void)above;
}

/* ============================================================================
 * Drawing — promotion dialog
 * ============================================================================ */

static void draw_promotion(App *app) {
    SDL_Renderer *r = app->renderer;
    /* dim board */
    sdl_fill_rect(r, BOARD_MARGIN_X, BOARD_MARGIN_Y+MENU_H,
                  8*SQUARE_SIZE, 8*SQUARE_SIZE, 0, 0, 0, 160);

    int bx = BOARD_MARGIN_X + 8*SQUARE_SIZE/2 - 120;
    int by = BOARD_MARGIN_Y + MENU_H + 8*SQUARE_SIZE/2 - 50;
    sdl_fill_rect(r, bx, by, 240, 100, 30, 30, 40, 255);
    sdl_draw_rect(r, bx, by, 240, 100, 180, 160, 100, 255);
    render_text_centered(r, g_font_sm, "Promote pawn to:", bx+120, by+6, 220, 200, 100);

    ChessColor pc = (app->game.board[app->promo_row][app->promo_col].color);
    PieceType opts[] = {QUEEN, ROOK, BISHOP, KNIGHT};
    const char *labels[] = {"Q","R","B","N"};
    for (int i = 0; i < 4; i++) {
        int bx2 = bx + 20 + i*152;
        int by2 = by + 60;
        bool hov = (app->mouse_x >= bx2 && app->mouse_x < bx2+(int)(50*g_scale) &&
                    app->mouse_y >= by2 && app->mouse_y < by2+(int)(56*g_scale));
        sdl_fill_rect(r, bx2, by2, (int)(50*g_scale), (int)(56*g_scale),
                      hov ? 70 : 45, hov ? 70 : 45, hov ? 90 : 60, 255);
        sdl_draw_rect(r, bx2, by2, (int)(50*g_scale), (int)(56*g_scale), 150, 130, 80, 255);
        SDL_Texture *tex = sdl_get_piece_texture(opts[i], pc);
        sdl_draw_piece(r, tex, bx2+(int)(25*g_scale), by2+(int)(22*g_scale), (int)(38*g_scale));
        render_text_centered(r, g_font_sm, labels[i], bx2+(int)(25*g_scale), by2+(int)(44*g_scale), 200, 200, 200);
    }
}

/* ============================================================================
 * Drawing — panel
 * ============================================================================ */

static void draw_panel(App *app) {
    SDL_Renderer *r = app->renderer;
    int x = PANEL_X, y = BOARD_MARGIN_Y + MENU_H;

    sdl_fill_rect(r, x-(int)(4*g_scale), y, PANEL_W+(int)(4*g_scale), 8*SQUARE_SIZE, 35, 35, 42, 255);

    render_text_centered(r, g_font_lg, "BeatChess", x+PANEL_W/2, y+(int)(6*g_scale), 255, 220, 60);
    y += (int)(36*g_scale);

    const char *mode = app->player_vs_ai
        ? (app->player_is_white ? "Player(W) vs AI(B)" : "Player(B) vs AI(W)")
        : "AI vs AI";
    render_text(r, g_font_sm, mode, x, y, 160, 200, 230);
    y += (int)(20*g_scale);

    char buf[128];
    const char *turn = app->game.turn == WHITE ? "White" : "Black";
    snprintf(buf, sizeof(buf), "Turn: %s  Move: %d", turn, app->move_count);
    render_text(r, g_font_sm, buf, x, y, 200, 200, 200);
    y += (int)(20*g_scale);

    Uint8 sr = app->is_checkmate||app->resigned ? 255 : app->is_in_check ? 220 : 160;
    Uint8 sg = app->is_checkmate||app->resigned ?  60 : app->is_in_check ?  80 : 180;
    Uint8 sb = app->is_checkmate||app->resigned ?  60 : 200;
    render_text(r, g_font_sm, app->status, x, y, sr, sg, sb);
    y += (int)(26*g_scale);

    long wm=(long)app->white_ms, bm=(long)app->black_ms;
    snprintf(buf,sizeof(buf),"White: %ld:%02ld.%03ld",wm/60000,(wm%60000)/1000,wm%1000);
    render_text(r, g_font_sm, buf, x, y, 220, 220, 220);
    y += (int)(16*g_scale);
    snprintf(buf,sizeof(buf),"Black: %ld:%02ld.%03ld",bm/60000,(bm%60000)/1000,bm%1000);
    render_text(r, g_font_sm, buf, x, y, 180, 180, 180);
    y += (int)(22*g_scale);

    if (app->ai_thinking) {
        render_text(r, g_font_sm, "AI thinking...", x, y, 100, 220, 255);
        y += (int)(18*g_scale);
    }

    sdl_fill_rect(r, x, y, PANEL_W, 1, 70, 70, 90, 255);
    y += (int)(8*g_scale);

    /* Buttons */
    bool game_over = app->is_checkmate || app->is_stalemate || app->resigned;
    Button btns[] = {
        {x, y,                        PANEL_W, (int)(24*g_scale), "N - New Game",        0, true},
        {x, y+(int)(28*g_scale),  PANEL_W, (int)(24*g_scale), "U - Undo",            0, !game_over},
        {x, y+(int)(56*g_scale),  PANEL_W, (int)(24*g_scale), "R - Resign",          0, !game_over && app->player_vs_ai},
        {x, y+(int)(84*g_scale),  PANEL_W, (int)(24*g_scale), "A - Toggle AI Mode",  0, true},
        {x, y+(int)(112*g_scale), PANEL_W, (int)(24*g_scale), "B - Swap Color",      0, true},
        {x, y+(int)(140*g_scale), PANEL_W, (int)(24*g_scale), "F - Flip Board",      0, true},
        {x, y+(int)(168*g_scale), PANEL_W, (int)(24*g_scale), g_music_on ? "M - Music: ON" : "M - Music: OFF", 0, true},
        {x, y+(int)(196*g_scale), PANEL_W, (int)(24*g_scale), "? - Help",            0, true},
        {x, y+(int)(224*g_scale), PANEL_W, (int)(24*g_scale), "Q - Quit",            0, true},
    };
    for (int i = 0; i < 9; i++) {
        if (!btns[i].enabled) continue;
        bool hov = btn_hovered(&btns[i], app->mouse_x, app->mouse_y);
        /* Resign button is red-tinted */
        if (i == 2) btn_draw(r, &btns[i], hov, false, 65, 30, 30);
        else        btn_draw(r, &btns[i], hov, false);
    }
    y += (int)(256*g_scale);

    /* Captured pieces — pinned to bottom of panel, always visible */
    {
        int panel_bottom = BOARD_MARGIN_Y + MENU_H + 8*SQUARE_SIZE;
        int cap_sz = (int)(16 * g_scale);
        int cy = panel_bottom - cap_sz*2 - (int)(18*g_scale);
        render_text(r, g_font_sm, "Captured:", x, cy - (int)(16*g_scale), 140, 140, 160);
        /* Draw black captured (white pieces taken by black) */
        int cx = x;
        for (int type = PAWN; type <= QUEEN; type++) {
            for (int n = 0; n < app->white_captured[type]; n++) {
                SDL_Texture *tex = sdl_get_piece_texture((PieceType)type, WHITE);
                sdl_draw_piece(r, tex, cx + cap_sz/2, cy + cap_sz/2, cap_sz);
                cx += cap_sz + 1;
            }
        }
        cx = x;
        cy += cap_sz + 2;
        for (int type = PAWN; type <= QUEEN; type++) {
            for (int n = 0; n < app->black_captured[type]; n++) {
                SDL_Texture *tex = sdl_get_piece_texture((PieceType)type, BLACK);
                sdl_draw_piece(r, tex, cx + cap_sz/2, cy + cap_sz/2, cap_sz);
                cx += cap_sz + 1;
            }
        }
    }
}

/* ============================================================================
 * Drawing — menubar, overlays, screens
 * ============================================================================ */

static void draw_menubar(App *app) {
    SDL_Renderer *r = app->renderer;
    int item_h = (int)(22 * g_scale);
    int sep_h  = (int)(2  * g_scale);
    int dw     = (int)(180 * g_scale);   /* dropdown width */
    int mx1    = 0;                       /* File menu x */
    int mx2    = (int)(60  * g_scale);   /* Help menu x */
    int mw     = (int)(60  * g_scale);   /* each menu button width */

    /* Menubar background */
    sdl_fill_rect(r, 0, 0, LOGICAL_W, MENU_H, 28, 28, 36, 255);

    /* File button */
    bool fhov = (app->mouse_x >= mx1 && app->mouse_x < mx1+mw && app->mouse_y < MENU_H);
    if (fhov || app->file_menu_open) sdl_fill_rect(r, mx1, 0, mw, MENU_H, 55, 55, 70, 255);
    render_text(r, g_font_sm, "File", mx1 + (int)(6*g_scale), (int)(4*g_scale), 210, 210, 210);

    /* Help button */
    bool hhov = (app->mouse_x >= mx2 && app->mouse_x < mx2+mw && app->mouse_y < MENU_H);
    if (hhov || app->help_menu_open) sdl_fill_rect(r, mx2, 0, mw, MENU_H, 55, 55, 70, 255);
    render_text(r, g_font_sm, "Help", mx2 + (int)(6*g_scale), (int)(4*g_scale), 210, 210, 210);

    /* Title */
    render_text_centered(r, g_font_sm, "BeatChess SDL2 Edition", LOGICAL_W/2, (int)(4*g_scale), 255, 220, 60);

    /* File dropdown */
    if (app->file_menu_open) {
        const char *items[] = {"New Game (N)","Undo (U)","---","Save Game","Load Game","---","Quit (Q)"};
        int iy = MENU_H;
        for (int i = 0; i < 7; i++) {
            if (items[i][0] == '-') {
                sdl_fill_rect(r, mx1, iy, dw, sep_h, 70, 70, 90, 255);
                iy += sep_h; continue;
            }
            bool h = (app->mouse_x >= mx1 && app->mouse_x < mx1+dw &&
                      app->mouse_y >= iy && app->mouse_y < iy+item_h);
            sdl_fill_rect(r, mx1, iy, dw, item_h, h?55:35, h?55:35, h?70:45, 255);
            render_text(r, g_font_sm, items[i], mx1+(int)(8*g_scale), iy+(int)(4*g_scale), 210, 210, 210);
            iy += item_h;
        }
    }

    /* Help dropdown */
    if (app->help_menu_open) {
        const char *items[] = {"Help (?)","About"};
        int iy = MENU_H;
        for (int i = 0; i < 2; i++) {
            bool h = (app->mouse_x >= mx2 && app->mouse_x < mx2+dw &&
                      app->mouse_y >= iy && app->mouse_y < iy+item_h);
            sdl_fill_rect(r, mx2, iy, dw, item_h, h?55:35, h?55:35, h?70:45, 255);
            render_text(r, g_font_sm, items[i], mx2+(int)(6*g_scale), iy+(int)(4*g_scale), 210, 210, 210);
            iy += item_h;
        }
    }
}

static void draw_overlay_text(App *app) {
    if (!app->is_checkmate && !app->is_stalemate && !app->resigned && app->check_timer<=0) return;
    SDL_Renderer *r = app->renderer;
    const char *msg = app->is_checkmate ? "CHECKMATE"
                    : app->is_stalemate ? "STALEMATE"
                    : app->resigned     ? "RESIGNED"
                    : "CHECK";
    Uint8 mr = (app->is_checkmate||app->resigned) ? 255 : app->is_stalemate ? 180 : 220;
    Uint8 mg = (app->is_checkmate||app->resigned) ?  60 : app->is_stalemate ? 180 :  80;
    Uint8 mb = (app->is_checkmate||app->resigned) ?  60 : app->is_stalemate ? 180 :  80;
    int bx=BOARD_MARGIN_X, by=BOARD_MARGIN_Y+MENU_H, bw=8*SQUARE_SIZE, bh=8*SQUARE_SIZE;
    int tw,th; TTF_SizeUTF8(g_font_lg,msg,&tw,&th);
    int ox=bx+(bw-tw)/2-(int)(16*g_scale), oy=by+(bh-th)/2-(int)(12*g_scale);
    sdl_fill_rect(r,ox,oy,tw+(int)(32*g_scale),th+(int)(24*g_scale),10,10,10,210);
    sdl_draw_rect(r,ox,oy,tw+(int)(32*g_scale),th+(int)(24*g_scale),mr,mg,mb,255);
    render_text_centered(r,g_font_lg,msg,bx+bw/2,oy+(int)(12*g_scale),mr,mg,mb);
}

static void draw_help_screen(App *app) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r,0,0,LOGICAL_W,LOGICAL_H,15,15,20,240);
    int y = (int)(40*g_scale);
    render_text_centered(r,g_font_lg,"BeatChess — Help",LOGICAL_W/2,y,255,220,60); y+=(int)(40*g_scale);
    const char *lines[] = {
        "N          New Game",
        "U          Undo move (Player vs AI: undoes AI move too)",
        "R          Resign",
        "A          Toggle AI vs AI / Player vs AI",
        "B          Swap player colour",
        "F          Flip board",
        "M          Toggle music on/off",
        "?          Show this help",
        "Q / Esc    Quit",
        "",
        "Mouse:  Click a piece to select; valid moves shown as dots.",
        "        Click destination to move.  Hover for legal move hints.",
        "        File menu -> Save/Load game (.sav format).",
        "",
        "Press any key or click to return.",
        NULL
    };
    for (int i = 0; lines[i]; i++) {
        if (lines[i][0]) render_text(r,g_font_sm,lines[i],(int)(100*g_scale),y,200,200,200);
        y += (int)(19*g_scale);
    }
}

static void draw_about_screen(App *app) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r,0,0,LOGICAL_W,LOGICAL_H,15,15,20,240);
    int y=(int)(60*g_scale);
    render_text_centered(r,g_font_lg,"BeatChess",LOGICAL_W/2,y,255,220,60); y+=(int)(30*g_scale);
    render_text_centered(r,g_font_med,"SDL2 Edition",LOGICAL_W/2,y,180,180,200); y+=(int)(40*g_scale);
    render_text_centered(r,g_font_sm,"Copyright (c) 2025 Jason Brian Hall",LOGICAL_W/2,y,160,200,230); y+=(int)(25*g_scale);
    render_text_centered(r,g_font_sm,"MIT License",LOGICAL_W/2,y,100,200,100); y+=(int)(40*g_scale);
    render_text_centered(r,g_font_sm,"Press any key or click to return.",LOGICAL_W/2,y,140,140,160);
}

static void draw_file_dialog(App *app, bool is_save) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r,0,0,LOGICAL_W,LOGICAL_H,15,15,20,230);
    render_text_centered(r,g_font_lg,is_save?"Save Game":"Load Game",LOGICAL_W/2,20,255,220,60);
    int lx=(int)(60*g_scale),ly=(int)(60*g_scale),lw=LOGICAL_W-(int)(120*g_scale),lh=(int)(360*g_scale);
    sdl_draw_rect(r,lx,ly,lw,lh,80,80,110,255);
    int visible=lh/((int)(24*g_scale));
    for (int i=0; i<visible && (i+app->fb.scroll)<app->fb.count; i++) {
        int fi=i+app->fb.scroll; bool sel=(fi==app->fb.selected); int ry=ly+i*48;
        if (sel) sdl_fill_rect(r,lx+1,ry,lw-2,(int)(24*g_scale),60,80,120,255);
        render_text(r,g_font_sm,app->fb.entries[fi].name,lx+(int)(10*g_scale),ry+(int)(5*g_scale),
                    sel?255:200,sel?255:200,sel?255:200);
    }
    if (app->fb.count==0)
        render_text_centered(r,g_font_sm,"(no .sav files found)",LOGICAL_W/2,ly+lh/2-(int)(10*g_scale),140,140,160);
    if (is_save) {
        render_text(r,g_font_sm,"Filename:",lx,ly+lh+(int)(16*g_scale),180,180,180);
        sdl_draw_rect(r,lx,ly+lh+(int)(32*g_scale),(int)(400*g_scale),(int)(26*g_scale),100,100,140,255);
        char disp[260]; snprintf(disp,sizeof(disp),"%s_",app->fb_input);
        render_text(r,g_font_sm,disp,lx+(int)(6*g_scale),ly+lh+(int)(37*g_scale),220,220,100);
    }
    render_text(r,g_font_sm,
                is_save?"Enter: Save    Esc: Cancel    Up/Down: Browse"
                       :"Enter/dbl-click: Load    Esc: Cancel",
                lx,ly+lh+62,120,180,120);
}

static void draw_confirm_new_game(App *app) {
    SDL_Renderer *r = app->renderer;
    /* dim the background */
    sdl_fill_rect(r, 0, 0, LOGICAL_W, LOGICAL_H, 0, 0, 0, 160);

    int bw = (int)(320*g_scale), bh = (int)(110*g_scale);
    int bx = (LOGICAL_W - bw) / 2, by = (LOGICAL_H - bh) / 2;
    sdl_fill_rect(r, bx, by, bw, bh, 30, 30, 40, 255);
    sdl_draw_rect(r, bx, by, bw, bh, 160, 140, 80, 255);

    render_text_centered(r, g_font_med, "Start a new game?", LOGICAL_W/2, by + (int)(14*g_scale), 240, 220, 120);

    /* Yes button */
    Button yes_btn = {bx+(int)(30*g_scale), by+(int)(55*g_scale), (int)(110*g_scale), (int)(34*g_scale), "Yes", 0, true};
    Button no_btn  = {bx+bw-(int)(140*g_scale), by+(int)(55*g_scale), (int)(110*g_scale), (int)(34*g_scale), "No",  0, true};
    btn_draw(r, &yes_btn, btn_hovered(&yes_btn, app->mouse_x, app->mouse_y), false, 30, 60, 30);
    btn_draw(r, &no_btn,  btn_hovered(&no_btn,  app->mouse_x, app->mouse_y), false, 60, 30, 30);
}

static void draw_frame(App *app) {
    SDL_Renderer *r = app->renderer;
    sdl_fill_rect(r,0,0,LOGICAL_W,LOGICAL_H,20,20,25,255);
    switch (app->screen) {
        case SCREEN_HELP:  draw_help_screen(app);        break;
        case SCREEN_ABOUT: draw_about_screen(app);       break;
        case SCREEN_SAVE:  draw_file_dialog(app,true);   break;
        case SCREEN_LOAD:  draw_file_dialog(app,false);  break;
        case SCREEN_GAME:
            draw_board(app);
            draw_panel(app);
            draw_overlay_text(app);
            if (app->awaiting_promotion) draw_promotion(app);
            if (app->confirm_new_game)   draw_confirm_new_game(app);
            draw_menubar(app);
            break;
    }
    SDL_RenderPresent(r);
}

/* ============================================================================
 * Input handling
 * ============================================================================ */

static void request_new_game(App *app) {
    /* If game hasn't started yet, just start without asking */
    if (app->move_count == 0) { game_new(app); return; }
    app->confirm_new_game = true;
}

static void handle_game_key(App *app, SDL_Keycode key) {
    switch (key) {
        case SDLK_n: request_new_game(app); break;
        case SDLK_u: undo_move(app); break;
        case SDLK_r: player_resign(app); break;
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

static void handle_promotion_click(App *app, int px, int py) {
    int bx = BOARD_MARGIN_X + 8*SQUARE_SIZE/2 - 120;
    int by = BOARD_MARGIN_Y + MENU_H + 8*SQUARE_SIZE/2 - 50;
    PieceType opts[] = {QUEEN, ROOK, BISHOP, KNIGHT};
    for (int i = 0; i < 4; i++) {
        int bx2 = bx + (int)(10*g_scale) + i*(int)(56*g_scale), by2 = by + (int)(30*g_scale);
        if (px >= bx2 && px < bx2+(int)(50*g_scale) && py >= by2 && py < by2+(int)(56*g_scale)) {
            app->game.board[app->promo_row][app->promo_col].type = opts[i];
            app->awaiting_promotion = false;
            /* Now run the post-move logic */
            int fr=app->anim_fr, fc=app->anim_fc, tr=app->promo_row, tc=app->promo_col;
            bool was_capture = false;
            (void)was_capture;
            app->last_from_row=fr; app->last_from_col=fc;
            app->last_to_row=tr;   app->last_to_col=tc;
            app->has_last_move=true;
            app->move_count++;
            app->piece_selected=false;
            memset(app->legal_dests,0,sizeof(app->legal_dests));
            Uint32 now=SDL_GetTicks();
            double elapsed=now-app->move_start_ms;
            ChessColor jm=(app->game.turn==WHITE)?BLACK:WHITE;
            if (jm==WHITE) app->white_ms+=elapsed; else app->black_ms+=elapsed;
            app->move_start_ms=now;
            MoveHistory mh; ChessMove mv={fr,fc,tr,tc,0};
            mh.game_state=app->game; mh.move=mv; mh.time_elapsed=elapsed/1000.0;
            if (app->move_history_count<MAX_MOVE_HISTORY*2)
                app->move_history[app->move_history_count++]=mh;
            bool ic=chess_is_in_check(&app->game,app->game.turn);
            if (ic){app->is_in_check=true;app->check_timer=1.5f;}
            else   {app->is_in_check=false;app->check_timer=0;}
            audio_play_move();
            check_game_over(app);
            if (!app->is_checkmate&&!app->is_stalemate){
                if(ic)audio_play_check();
                const char*t=app->game.turn==WHITE?"White":"Black";
                snprintf(app->status,sizeof(app->status),"%s to move",t);
                chess_start_thinking(&app->thinking,&app->game);
            }
            return;
        }
    }
}

static void handle_board_click(App *app, int px, int py) {
    if (app->is_checkmate || app->is_stalemate || app->resigned) return;
    if (app->is_animating || app->awaiting_promotion) return;

    ChessColor player_color = app->player_is_white ? WHITE : BLACK;
    if (app->player_vs_ai && app->game.turn != player_color) return;

    int row, col;
    if (!pixel_to_board(app, px, py, &row, &col)) return;

    ChessPiece piece = app->game.board[row][col];

    if (!app->piece_selected) {
        ChessColor tc = app->player_vs_ai ? player_color : app->game.turn;
        if (piece.type != EMPTY && piece.color == tc) {
            app->selected_row = row; app->selected_col = col;
            app->piece_selected = true;
            calc_legal_dests(app);
        }
    } else {
        int fr = app->selected_row, fc = app->selected_col;
        if (fr == row && fc == col) {
            app->piece_selected = false;
            memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
            return;
        }
        ChessColor tc = app->player_vs_ai ? player_color : app->game.turn;
        if (piece.type != EMPTY && piece.color == tc) {
            app->selected_row = row; app->selected_col = col;
            calc_legal_dests(app);
            return;
        }
        if (chess_is_valid_move(&app->game, fr, fc, row, col)) {
            ChessGameState tmp = app->game;
            ChessMove mv = {fr, fc, row, col, 0};
            chess_make_move(&tmp, mv);
            if (!chess_is_in_check(&tmp, app->game.turn)) {
                /* track capture before animating */
                if (app->game.board[row][col].type != EMPTY) {
                    ChessPiece cap = app->game.board[row][col];
                    if (app->game.turn==WHITE) app->white_captured[cap.type]++;
                    else                       app->black_captured[cap.type]++;
                }
                commit_move(app, fr, fc, row, col);
            } else {
                snprintf(app->status, sizeof(app->status), "Illegal — king in check");
                app->piece_selected = false;
                memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
            }
        } else {
            snprintf(app->status, sizeof(app->status), "Illegal move");
            app->piece_selected = false;
            memset(app->legal_dests,    0, sizeof(app->legal_dests));
    memset(app->danger_squares, 0, sizeof(app->danger_squares));
    memset(app->check_squares,  0, sizeof(app->check_squares));
        }
    }
}

static void handle_menu_click(App *app, int px, int py) {
    if (py < MENU_H) {
        { int mw=(int)(60*g_scale);
        if (px < mw) { app->file_menu_open=!app->file_menu_open; app->help_menu_open=false; return; }
        if (px < mw*2){ app->help_menu_open=!app->help_menu_open; app->file_menu_open=false; return; } }
        app->file_menu_open=app->help_menu_open=false; return;
    }
    if (app->file_menu_open && px < (int)(180*g_scale)) {  /* file dropdown */
        int _ih=(int)(22*g_scale),_s=(int)(2*g_scale);
        int ihs[]={_ih,_ih,_s,_ih,_ih,_s,_ih};  /* scaled heights */ int iy=MENU_H;
        for (int i=0;i<7;i++){
            if (py>=iy && py<iy+ihs[i]) {
                app->file_menu_open=false;
                switch(i){
                    case 0:game_new(app);break;
                    case 1:undo_move(app);break;
                    case 3:fb_scan(&app->fb,".sav");app->fb_input[0]=0;app->fb_input_len=0;app->screen=SCREEN_SAVE;break;
                    case 4:fb_scan(&app->fb,".sav");app->screen=SCREEN_LOAD;break;
                    case 6:app->running=false;break;
                }
                return;
            }
            iy+=ihs[i];
        }
        app->file_menu_open=false; return;
    }
    if (app->help_menu_open && px>=(int)(60*g_scale) && px<(int)(60*g_scale)+(int)(180*g_scale)) {
        int iy=MENU_H;
        for (int i=0;i<2;i++){
            if (py>=iy&&py<iy+22){app->help_menu_open=false;if(i==0)app->screen=SCREEN_HELP;else app->screen=SCREEN_ABOUT;return;}
            iy+=22;
        }
        app->help_menu_open=false; return;
    }
    if (app->file_menu_open||app->help_menu_open){app->file_menu_open=app->help_menu_open=false;return;}

    /* Panel buttons */
    bool game_over = app->is_checkmate || app->is_stalemate || app->resigned;
    int base_y = BOARD_MARGIN_Y + MENU_H + (int)((36+20+20+26+16+22+8)*g_scale);
    if (px >= PANEL_X && px < LOGICAL_W) {
        int bys[]={0,(int)(28*g_scale),(int)(56*g_scale),(int)(84*g_scale),(int)(112*g_scale),(int)(140*g_scale),(int)(168*g_scale),(int)(196*g_scale),(int)(224*g_scale)};
        SDL_Keycode ks[]={SDLK_n,SDLK_u,SDLK_r,SDLK_a,SDLK_b,SDLK_f,SDLK_m,SDLK_QUESTION,SDLK_q};
        for (int i=0;i<9;i++){
            if (i==1&&game_over) continue;
            if (i==2&&(game_over||!app->player_vs_ai)) continue;
            if (py>=base_y+bys[i]&&py<base_y+bys[i]+(int)(24*g_scale)){handle_game_key(app,ks[i]);return;}
        }
        return;
    }

    if (app->awaiting_promotion) { handle_promotion_click(app, px, py); return; }
    handle_board_click(app, px, py);
}

static void handle_dialog_key(App *app, SDL_Keycode key, bool is_save) {
    switch (key) {
        case SDLK_ESCAPE: app->screen=SCREEN_GAME; break;
        case SDLK_UP:
            if (app->fb.selected>0){app->fb.selected--;if(app->fb.selected<app->fb.scroll)app->fb.scroll=app->fb.selected;}
            break;
        case SDLK_DOWN:
            if (app->fb.selected<app->fb.count-1){app->fb.selected++;if(app->fb.selected>=app->fb.scroll+15)app->fb.scroll++;}
            break;
        case SDLK_RETURN: {
            if (is_save) {
                char fn[300];
                if (app->fb_input_len>0) snprintf(fn,sizeof(fn),"%s",app->fb_input);
                else if (app->fb.count>0) snprintf(fn,sizeof(fn),"%s",app->fb.entries[app->fb.selected].name);
                else break;
                if (!strstr(fn,".sav")) strncat(fn,".sav",sizeof(fn)-strlen(fn)-1);
                BeatChessVisualization vis; vis.game=app->game;
                vis.move_history_count=app->move_history_count; vis.move_count=app->move_count;
                for(int i=0;i<app->move_history_count;i++) vis.move_history[i]=app->move_history[i];
                pgn_export_game(&vis,fn,"Player","BeatChess AI");
                snprintf(app->status,sizeof(app->status),"Saved: %s",fn);
            } else {
                if (app->fb.count==0) break;
                char *fn=app->fb.entries[app->fb.selected].name;
                BeatChessVisualization vis; memset(&vis,0,sizeof(vis));
                if (pgn_import_game(&vis,fn)){
                    app->game=vis.game; app->move_history_count=vis.move_history_count; app->move_count=vis.move_count;
                    for(int i=0;i<vis.move_history_count;i++) app->move_history[i]=vis.move_history[i];
                    snprintf(app->status,sizeof(app->status),"Loaded: %s",fn);
                    chess_start_thinking(&app->thinking,&app->game);
                }
            }
            app->screen=SCREEN_GAME; break;
        }
        case SDLK_BACKSPACE:
            if (is_save&&app->fb_input_len>0) app->fb_input[--app->fb_input_len]=0;
            break;
        default: break;
    }
}

static void handle_dialog_text(App *app, const char *text, bool is_save) {
    if (!is_save) return;
    for (int i=0; text[i] && app->fb_input_len<250; i++) {
        char c=text[i];
        if (c>=32&&c<127){app->fb_input[app->fb_input_len++]=c;app->fb_input[app->fb_input_len]=0;}
    }
}

/* ============================================================================
 * Cursor update (called every frame)
 * ============================================================================ */

static void update_cursor(App *app) {
    if (app->screen != SCREEN_GAME) { set_cursor(g_cursor_arrow); return; }
    if (app->awaiting_promotion) {
        /* check over promotion buttons */
        int bx=BOARD_MARGIN_X+8*SQUARE_SIZE/2-(int)(160*g_scale), by=BOARD_MARGIN_Y+MENU_H+8*SQUARE_SIZE/2-(int)(55*g_scale);
        for (int i=0;i<4;i++){
            int bx2=bx+(int)(10*g_scale)+i*(int)(56*g_scale), by2=by+(int)(30*g_scale);
            if (app->mouse_x>=bx2&&app->mouse_x<bx2+(int)(50*g_scale)&&app->mouse_y>=by2&&app->mouse_y<by2+(int)(56*g_scale)){
                set_cursor(g_cursor_hand); return;
            }
        }
        set_cursor(g_cursor_arrow); return;
    }

    /* Hand cursor over: panel buttons, menu items, clickable pieces */
    if (app->mouse_y < MENU_H) { set_cursor(g_cursor_hand); return; }

    if (app->mouse_x >= PANEL_X) { set_cursor(g_cursor_hand); return; }

    int row, col;
    if (pixel_to_board(app, app->mouse_x, app->mouse_y, &row, &col)) {
        ChessColor player_color = app->player_is_white ? WHITE : BLACK;
        ChessColor turn_color   = app->player_vs_ai ? player_color : app->game.turn;
        ChessPiece p = app->game.board[row][col];
        if ((p.type!=EMPTY && p.color==turn_color) ||
            (app->piece_selected && app->legal_dests[row][col])) {
            set_cursor(g_cursor_hand); return;
        }
    }
    set_cursor(g_cursor_arrow);
}

/* ============================================================================
 * AI update
 * ============================================================================ */

static void update_ai(App *app, float dt) {
    if (app->is_checkmate || app->is_stalemate || app->resigned) return;
    if (app->is_animating || app->awaiting_promotion) return;

    ChessColor player_color = app->player_is_white ? WHITE : BLACK;
    bool ai_turn = !app->player_vs_ai ||
                   (app->player_vs_ai && app->game.turn != player_color);
    if (!ai_turn) return;

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
    if (is_thinking || has_move) app->time_thinking += dt;
    if (!has_move) return;

    bool should_play = false;
    if (app->time_thinking >= 4.0f)                      should_play = true;
    else if (app->time_thinking >= 0.5f && depth >= 3)   should_play = true;
    if (!should_play) return;

    ChessMove mv = chess_get_best_move_now(&app->thinking);
    app->time_thinking = 0;

    if (!chess_is_valid_move(&app->game, mv.from_row, mv.from_col, mv.to_row, mv.to_col)) {
        chess_start_thinking(&app->thinking, &app->game); return;
    }
    ChessGameState tmp = app->game;
    chess_make_move(&tmp, mv);
    if (chess_is_in_check(&tmp, app->game.turn)) {
        chess_start_thinking(&app->thinking, &app->game); return;
    }

    /* track capture */
    if (app->game.board[mv.to_row][mv.to_col].type != EMPTY) {
        ChessPiece cap = app->game.board[mv.to_row][mv.to_col];
        if (app->game.turn==WHITE) app->white_captured[cap.type]++;
        else                       app->black_captured[cap.type]++;
    }
    commit_move(app, mv.from_row, mv.from_col, mv.to_row, mv.to_col);
    app->ai_thinking = false;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0) {
        fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1;
    }

    App *app = (App *)calloc(1, sizeof(App));
    if (!app) { fprintf(stderr,"OOM\n"); return 1; }

    app->hover_row = app->hover_col = -1;

    app->window = SDL_CreateWindow("BeatChess SDL2",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    900, 640, SDL_WINDOW_RESIZABLE);
    if (!app->window) { fprintf(stderr,"Window: %s\n",SDL_GetError()); return 1; }

    app->renderer = SDL_CreateRenderer(app->window, -1,
                                        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!app->renderer) { fprintf(stderr,"Renderer: %s\n",SDL_GetError()); return 1; }

    /* Layout scales to actual window size */
    int iw, ih;
    SDL_GetWindowSize(app->window, &iw, &ih);
    layout_update(iw, ih);
    SDL_RenderSetLogicalSize(app->renderer, LOGICAL_W, LOGICAL_H);

    cursors_init();

    if (sdl_load_chess_pieces(app->renderer) != 0)
        fprintf(stderr,"Warning: piece sprites failed to load\n");

    sdl_show_splashscreen(app->renderer, LOGICAL_W, LOGICAL_H);

    fonts_init();
    fonts_rebuild();  /* apply layout-computed sizes */
    audio_init();

    chess_init_zobrist();
    chess_clear_transposition_table();
    srand((unsigned)time(NULL));
    chess_init_thinking_state(&app->thinking);

    app->player_vs_ai    = true;
    app->player_is_white = true;
    app->screen          = SCREEN_GAME;
    app->running         = true;

    game_new(app);

    Uint32 last_ticks = SDL_GetTicks();
    SDL_StartTextInput();

    while (app->running) {
        Uint32 now = SDL_GetTicks();
        float dt   = (now - last_ticks) / 1000.0f;
        last_ticks = now;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: app->running=false; break;

                case SDL_KEYDOWN: {
                    SDL_Keycode k=ev.key.keysym.sym;
                    if (app->screen==SCREEN_HELP||app->screen==SCREEN_ABOUT){app->screen=SCREEN_GAME;break;}
                    if (app->screen==SCREEN_SAVE){handle_dialog_key(app,k,true);break;}
                    if (app->screen==SCREEN_LOAD){handle_dialog_key(app,k,false);break;}
                    if (app->awaiting_promotion) break; /* ignore keys during promotion */
                    if (app->confirm_new_game) { if(k==SDLK_y){app->confirm_new_game=false;game_new(app);} else if(k==SDLK_n){app->confirm_new_game=false;} break; }
                    if (k==SDLK_ESCAPE){
                        if (app->confirm_new_game){app->confirm_new_game=false;break;}
                        if (app->file_menu_open||app->help_menu_open){app->file_menu_open=app->help_menu_open=false;}
                        else app->running=false;
                        break;
                    }
                    handle_game_key(app,k);
                    break;
                }

                case SDL_TEXTINPUT:
                    if (app->screen==SCREEN_SAVE) handle_dialog_text(app,ev.text.text,true);
                    break;

                case SDL_MOUSEMOTION:
                    app->mouse_x=ev.motion.x; app->mouse_y=ev.motion.y;
                    pixel_to_board(app,app->mouse_x,app->mouse_y,&app->hover_row,&app->hover_col);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    app->mouse_x=ev.button.x; app->mouse_y=ev.button.y;
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (ev.button.button==SDL_BUTTON_LEFT){
                        int px=ev.button.x, py=ev.button.y;
                        app->mouse_x=px; app->mouse_y=py;
                        if (app->screen==SCREEN_HELP||app->screen==SCREEN_ABOUT){app->screen=SCREEN_GAME;break;}
                        if (app->screen==SCREEN_SAVE||app->screen==SCREEN_LOAD){
                            int ly=(int)(60*g_scale),lh=(int)(360*g_scale);
                            if (px>=60&&px<LOGICAL_W-60&&py>=ly&&py<ly+lh){
                                int idx=(py-ly)/((int)(24*g_scale))+app->fb.scroll;
                                if (idx>=0&&idx<app->fb.count){
                                    if (idx==app->fb.selected&&app->screen==SCREEN_LOAD)
                                        handle_dialog_key(app,SDLK_RETURN,false);
                                    else app->fb.selected=idx;
                                }
                            }
                            break;
                        }
                        if (app->awaiting_promotion){handle_promotion_click(app,px,py);break;}
                        if (app->confirm_new_game) {
                            /* Check yes/no buttons */
                            int bw=(int)(320*g_scale),bh=(int)(110*g_scale),bx=(LOGICAL_W-bw)/2,by=(LOGICAL_H-bh)/2;
                            if (px>=bx+(int)(30*g_scale)&&px<bx+(int)(140*g_scale)&&py>=by+(int)(55*g_scale)&&py<by+(int)(89*g_scale)) {
                                app->confirm_new_game=false; game_new(app);
                            } else if (px>=bx+bw-(int)(140*g_scale)&&px<bx+bw-(int)(30*g_scale)&&py>=by+(int)(55*g_scale)&&py<by+(int)(89*g_scale)) {
                                app->confirm_new_game=false;
                            } else {
                                app->confirm_new_game=false; /* click outside = cancel */
                            }
                            break;
                        }
                        handle_menu_click(app,px,py);
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    if (app->screen==SCREEN_SAVE||app->screen==SCREEN_LOAD){
                        app->fb.scroll-=ev.wheel.y;
                        if (app->fb.scroll<0)app->fb.scroll=0;
                        if (app->fb.scroll>app->fb.count-1)app->fb.scroll=app->fb.count>0?app->fb.count-1:0;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    if (ev.window.event==SDL_WINDOWEVENT_CLOSE) {
                        app->running=false;
                    } else if (ev.window.event==SDL_WINDOWEVENT_RESIZED ||
                               ev.window.event==SDL_WINDOWEVENT_SIZE_CHANGED) {
                        int nw = ev.window.data1, nh = ev.window.data2;
                        layout_update(nw, nh);
                        SDL_RenderSetLogicalSize(app->renderer, LOGICAL_W, LOGICAL_H);
                        fonts_rebuild();
                    }
                    break;
            }
        }

        /* Update */
        if (app->screen == SCREEN_GAME) {
            /* Animation */
            if (app->is_animating) {
                app->anim_progress += ANIM_SPEED * dt;
                if (app->anim_progress >= 1.0f) {
                    app->anim_progress = 1.0f;
                    if (app->post_anim_pending && !app->awaiting_promotion)
                        finish_animation(app);
                    else if (app->awaiting_promotion) {
                        /* animation done, show promotion dialog */
                        app->is_animating = false;
                        app->game = app->post_anim_game;
                        app->post_anim_pending = false;
                    }
                }
            }

            if (app->check_timer > 0) { app->check_timer -= dt; if (app->check_timer<0)app->check_timer=0; }

            if (!app->is_checkmate&&!app->is_stalemate&&!app->resigned&&!app->is_animating&&!app->awaiting_promotion){
                bool ic=chess_is_in_check(&app->game,app->game.turn);
                if (ic&&!app->is_in_check){app->is_in_check=true;app->check_timer=1.5f;}
                else if (!ic) app->is_in_check=false;
            }

            update_cursor(app);
            update_ai(app, dt);
        }

        draw_frame(app);
    }

    SDL_StopTextInput();
    chess_cleanup_thinking_state(&app->thinking);
    audio_shutdown();
    sdl_destroy_chess_pieces();
    cursors_shutdown();
    fonts_shutdown();
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    free(app);
    SDL_Quit();
    return 0;
}
