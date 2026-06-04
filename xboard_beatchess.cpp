/*
 * xboard_beatchess.cpp — BeatChess XBoard/WinBoard protocol engine
 *
 * XBoard protocol compliance:
 *   - Handles: xboard, protover, new, force, go, usermove, setboard,
 *              ping, sd, depth, level, time, otim, result, undo, remove,
 *              white, black, computer, post, nopost, hard, easy, random,
 *              draw, accepted, rejected, quit, analyze, exit
 *   - Features declared: myname, usermove, setboard, ping, sigint, sigterm, done
 *   - Promotion: e7e8q / e7e8r / e7e8b / e7e8n parsed (promotes to queen always
 *     unless/until chess_make_move supports promotion argument)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>
#include <stdarg.h>
#include "beatchess.h"
#include "chess_ai_move.h"

/* ============================================================================
 * Engine state
 * ============================================================================ */

typedef enum {
    MODE_FORCE,   /* observe only, do not move */
    MODE_PLAY,    /* engine should move when it's its turn */
    MODE_ANALYZE  /* analysis mode (not fully implemented) */
} EngineMode;

static ChessGameState  g_game;
static EngineMode      g_mode         = MODE_FORCE;
static ChessColor      g_engine_color = BLACK;   /* which side the engine plays */
static int             g_search_depth = 4;
static FILE           *g_log          = NULL;
static bool            g_post         = false;   /* show thinking output */
static bool            g_moved_this_cmd = false; /* prevent double-move in one iteration */

/* ============================================================================
 * Debug logging
 * ============================================================================ */

static void dbg(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

/* ============================================================================
 * Move string helpers
 * ============================================================================ */

static void move_to_str(ChessMove m, char *buf) {
    /* buf must be at least 6 bytes */
    snprintf(buf, 6, "%c%d%c%d",
             'a' + m.from_col, 8 - m.from_row,
             'a' + m.to_col,   8 - m.to_row);
}

/* Parse "e2e4" or "e7e8q" — promotion piece stored in move.score as piece char,
 * 0 if none.  Returns false on parse failure. */
static bool parse_move(const char *s, ChessMove *out) {
    if (!s || strlen(s) < 4) return false;
    if (s[0] < 'a' || s[0] > 'h') return false;
    if (s[1] < '1' || s[1] > '8') return false;
    if (s[2] < 'a' || s[2] > 'h') return false;
    if (s[3] < '1' || s[3] > '8') return false;

    out->from_col = s[0] - 'a';
    out->from_row = 8 - (s[1] - '0');
    out->to_col   = s[2] - 'a';
    out->to_row   = 8 - (s[3] - '0');
    out->score    = s[4] ? (int)(unsigned char)tolower(s[4]) : 0; /* promo piece */
    return true;
}

/* ============================================================================
 * FEN parser — needed for "setboard"
 * ============================================================================ */

static PieceType fen_char_to_type(char c) {
    switch (tolower(c)) {
        case 'p': return PAWN;
        case 'n': return KNIGHT;
        case 'b': return BISHOP;
        case 'r': return ROOK;
        case 'q': return QUEEN;
        case 'k': return KING;
        default:  return EMPTY;
    }
}

static bool setboard_fen(ChessGameState *game, const char *fen) {
    /* Clear board */
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            game->board[r][c] = {EMPTY, NONE};

    game->white_king_moved   = false;
    game->black_king_moved   = false;
    game->white_rook_a_moved = false;
    game->white_rook_h_moved = false;
    game->black_rook_a_moved = false;
    game->black_rook_h_moved = false;
    game->en_passant_col     = -1;
    game->en_passant_row     = -1;

    const char *p = fen;

    /* 1. Piece placement */
    int row = 0, col = 0;
    while (*p && *p != ' ') {
        if (*p == '/') { row++; col = 0; }
        else if (*p >= '1' && *p <= '8') { col += *p - '0'; }
        else {
            if (row < 8 && col < 8) {
                PieceType  t = fen_char_to_type(*p);
                ChessColor c2 = isupper((unsigned char)*p) ? WHITE : BLACK;
                game->board[row][col] = {t, c2};
                col++;
            }
        }
        p++;
    }
    if (*p == ' ') p++;

    /* 2. Active color */
    game->turn = (*p == 'w') ? WHITE : BLACK;
    while (*p && *p != ' ') p++;
    if (*p == ' ') p++;

    /* 3. Castling availability */
    game->white_king_moved   = true;
    game->black_king_moved   = true;
    game->white_rook_a_moved = true;
    game->white_rook_h_moved = true;
    game->black_rook_a_moved = true;
    game->black_rook_h_moved = true;
    while (*p && *p != ' ') {
        switch (*p) {
            case 'K': game->white_king_moved = false; game->white_rook_h_moved = false; break;
            case 'Q': game->white_king_moved = false; game->white_rook_a_moved = false; break;
            case 'k': game->black_king_moved = false; game->black_rook_h_moved = false; break;
            case 'q': game->black_king_moved = false; game->black_rook_a_moved = false; break;
        }
        p++;
    }
    if (*p == ' ') p++;

    /* 4. En passant target square */
    if (*p && *p != '-') {
        game->en_passant_col = p[0] - 'a';
        game->en_passant_row = 8 - (p[1] - '0');
    }
    /* Skip rest (halfmove/fullmove clocks — we don't use them) */
    return true;
}

/* ============================================================================
 * Engine move — compute and send one move
 * ============================================================================ */

static void engine_make_move(void) {
    if (g_mode != MODE_PLAY) return;
    if (g_game.turn != g_engine_color) return;

    ChessGameStatus status = chess_check_game_status(&g_game);
    if (status != CHESS_PLAYING) return;

    ChessAIConfig config;
    config.search_depth        = g_search_depth;
    config.threshold_centipawns = 25;
    config.use_randomization   = true;

    ChessAIMoveResult result = chess_ai_compute_move(&g_game, config);

    if (result.move.from_row < 0) {
        dbg("[ENGINE] No legal move found\n");
        return;
    }

    chess_make_move(&g_game, result.move);

    char ms[6];
    move_to_str(result.move, ms);
    printf("move %s\n", ms);
    fflush(stdout);
    dbg("[ENGINE] move %s (score %d)\n", ms, result.score);

    g_moved_this_cmd = true;
}

/* ============================================================================
 * Command handlers
 * ============================================================================ */

static void cmd_xboard(void)          { /* nothing needed */ }

static void cmd_protover(int ver) {
    (void)ver;
    /* Send all features, then done=1 */
    printf("feature myname=\"BeatChess\"\n");
    printf("feature usermove=1\n");
    printf("feature setboard=1\n");
    printf("feature ping=1\n");
    printf("feature reuse=1\n");
    printf("feature sigint=0\n");
    printf("feature sigterm=0\n");
    printf("feature done=1\n");
    fflush(stdout);
}

static void cmd_new(void) {
    chess_init_board(&g_game);
    g_engine_color = BLACK;   /* XBoard default: engine plays Black */
    g_mode         = MODE_PLAY;
    g_moved_this_cmd = false;
    dbg("[CMD] new — engine plays Black\n");
}

static void cmd_force(void) {
    g_mode = MODE_FORCE;
    dbg("[CMD] force\n");
}

static void cmd_go(void) {
    /* Engine takes over the side currently to move */
    g_engine_color = g_game.turn;
    g_mode         = MODE_PLAY;
    dbg("[CMD] go — engine now plays %s\n",
        g_engine_color == WHITE ? "White" : "Black");
    engine_make_move();
}

static void cmd_usermove(const char *mv_str) {
    ChessMove m;
    if (!parse_move(mv_str, &m)) {
        printf("Error (illegal move): %s\n", mv_str);
        fflush(stdout);
        dbg("[CMD] usermove parse failed: %s\n", mv_str);
        return;
    }

    if (!chess_is_valid_move(&g_game, m.from_row, m.from_col, m.to_row, m.to_col)) {
        printf("Illegal move: %s\n", mv_str);
        fflush(stdout);
        dbg("[CMD] usermove illegal: %s\n", mv_str);
        return;
    }

    chess_make_move(&g_game, m);
    dbg("[CMD] usermove %s applied, turn now %s\n",
        mv_str, g_game.turn == WHITE ? "White" : "Black");

    /* Check game-over after opponent's move */
    ChessGameStatus status = chess_check_game_status(&g_game);
    if (status == CHESS_CHECKMATE_WHITE) {
        printf("0-1 {Black mates}\n"); fflush(stdout);
        dbg("[STATUS] Black wins by checkmate\n");
        return;
    }
    if (status == CHESS_CHECKMATE_BLACK) {
        printf("1-0 {White mates}\n"); fflush(stdout);
        dbg("[STATUS] White wins by checkmate\n");
        return;
    }
    if (status == CHESS_STALEMATE) {
        printf("1/2-1/2 {Stalemate}\n"); fflush(stdout);
        dbg("[STATUS] Stalemate\n");
        return;
    }

    /* Respond with our move if it's now our turn */
    engine_make_move();
}

static void cmd_setboard(const char *fen) {
    if (!setboard_fen(&g_game, fen)) {
        printf("tellusererror Bad FEN: %s\n", fen);
        fflush(stdout);
    }
    dbg("[CMD] setboard %s\n", fen);
}

static void cmd_ping(int n) {
    printf("pong %d\n", n);
    fflush(stdout);
    dbg("[CMD] ping %d -> pong %d\n", n, n);
}

static void cmd_sd(int depth) {
    if (depth >= 1 && depth <= 20) g_search_depth = depth;
    dbg("[CMD] sd %d\n", depth);
}

static void cmd_undo(void) {
    /* We don't maintain full history in this engine; acknowledge silently.
     * A full implementation would pop the last move off a stack. */
    dbg("[CMD] undo (not implemented — position unchanged)\n");
}

static void cmd_remove(void) {
    /* remove = undo two half-moves */
    dbg("[CMD] remove (not implemented — position unchanged)\n");
}

static void cmd_result(const char *rest) {
    /* Game is over; go back to force mode */
    g_mode = MODE_FORCE;
    dbg("[CMD] result %s\n", rest ? rest : "");
}

static void cmd_white(void) {
    /* Deprecated XBoard command: white to move, engine plays Black */
    g_game.turn    = WHITE;
    g_engine_color = BLACK;
    dbg("[CMD] white (deprecated): white to move, engine plays Black\n");
}

static void cmd_black(void) {
    /* Deprecated XBoard command: black to move, engine plays White */
    g_game.turn    = BLACK;
    g_engine_color = WHITE;
    dbg("[CMD] black (deprecated): black to move, engine plays White\n");
}

/* ============================================================================
 * Main loop
 * ============================================================================ */

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin,  NULL, _IONBF, 0);

    g_log = fopen("beatchess_engine.log", "w");
    dbg("=== BeatChess XBoard Engine ===\n");

    chess_init_board(&g_game);

    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        /* Strip trailing whitespace */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                           || line[len-1] == ' '))
            line[--len] = '\0';
        if (len == 0) continue;

        dbg("[IN] %s\n", line);
        g_moved_this_cmd = false;

        /* ---- Dispatch ---- */

        if (strcmp(line, "xboard") == 0) {
            cmd_xboard();
        }
        else if (strncmp(line, "protover", 8) == 0) {
            int ver = 1;
            sscanf(line + 8, " %d", &ver);
            cmd_protover(ver);
        }
        else if (strcmp(line, "new") == 0) {
            cmd_new();
        }
        else if (strcmp(line, "force") == 0) {
            cmd_force();
        }
        else if (strcmp(line, "go") == 0) {
            cmd_go();
        }
        else if (strncmp(line, "usermove ", 9) == 0) {
            cmd_usermove(line + 9);
        }
        else if (strncmp(line, "setboard ", 9) == 0) {
            cmd_setboard(line + 9);
        }
        else if (strncmp(line, "ping ", 5) == 0) {
            int n = 0;
            sscanf(line + 5, "%d", &n);
            cmd_ping(n);
        }
        else if (strncmp(line, "sd ", 3) == 0) {
            int d = g_search_depth;
            sscanf(line + 3, "%d", &d);
            cmd_sd(d);
        }
        else if (strncmp(line, "depth ", 6) == 0) {
            int d = g_search_depth;
            sscanf(line + 6, "%d", &d);
            cmd_sd(d);
        }
        else if (strcmp(line, "undo") == 0) {
            cmd_undo();
        }
        else if (strcmp(line, "remove") == 0) {
            cmd_remove();
        }
        else if (strncmp(line, "result ", 7) == 0) {
            cmd_result(line + 7);
        }
        else if (strcmp(line, "white") == 0) {
            cmd_white();
        }
        else if (strcmp(line, "black") == 0) {
            cmd_black();
        }
        else if (strcmp(line, "quit") == 0) {
            dbg("[CMD] quit\n");
            break;
        }

        /* Commands that are acknowledged but require no action */
        else if (strncmp(line, "level ",    6) == 0 ||
                 strncmp(line, "time ",     5) == 0 ||
                 strncmp(line, "otim ",     5) == 0 ||
                 strcmp (line, "post")         == 0 ||
                 strcmp (line, "nopost")       == 0 ||
                 strcmp (line, "hard")         == 0 ||
                 strcmp (line, "easy")         == 0 ||
                 strcmp (line, "random")       == 0 ||
                 strcmp (line, "computer")     == 0 ||
                 strcmp (line, "draw")         == 0 ||
                 strncmp(line, "accepted ",  9) == 0 ||
                 strcmp (line, "accepted")      == 0 ||
                 strncmp(line, "rejected ",  9) == 0 ||
                 strcmp (line, "rejected")      == 0 ||
                 strcmp (line, "analyze")       == 0 ||
                 strcmp (line, "exit")          == 0 ||
                 strcmp (line, ".")             == 0) {
            dbg("[CMD] %s (no action)\n", line);

            /* analyze/exit: go to force mode so we don't auto-move */
            if (strcmp(line, "analyze") == 0) g_mode = MODE_FORCE;
            if (strcmp(line, "exit")    == 0) g_mode = MODE_PLAY;
        }
        else {
            /* Unknown command — XBoard protocol says ignore unknown commands */
            dbg("[CMD] unknown (ignored): %s\n", line);
        }
    }

    dbg("[SHUTDOWN] Engine exiting\n");
    if (g_log) fclose(g_log);
    return 0;
}
