#ifndef XBOARD_ENGINE_H
#define XBOARD_ENGINE_H

/*
 * xboard_engine.h — XBoard/WinBoard protocol engine subprocess wrapper
 * Header-only implementation (no separate .cpp needed).
 *
 * Spawn any XBoard engine as a subprocess:
 *   XBoardEngine eng;
 *   xboard_engine_init(&eng, "gnuchess --xboard");
 *   xboard_start_thinking(&eng, &game);
 *   if (xboard_has_move(&eng)) ChessMove mv = xboard_get_best_move_now(&eng);
 *   xboard_engine_quit(&eng);
 */

#include "beatchess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>

#ifndef XBOARD_DEFAULT_DEPTH
#define XBOARD_DEFAULT_DEPTH 4
#endif
#define XBOARD_STRINGIFY(x) #x
#define XBOARD_TOSTR(x)     XBOARD_STRINGIFY(x)
#define XBOARD_DEFAULT_DEPTH_STR XBOARD_TOSTR(XBOARD_DEFAULT_DEPTH)

/* ============================================================================
 * XBoardEngine state
 * ============================================================================ */

typedef struct {
    FILE   *to_engine;
    FILE   *from_engine;
    pid_t   child_pid;

    pthread_t      reader_thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;

    ChessMove best_move;
    bool      has_move;
    bool      thinking;

    ChessGameState last_game;
    bool           engine_ok;

    char engine_cmd[256];
    char engine_name[128];   /* populated from "feature myname=..." during handshake */
} XBoardEngine;

/* ============================================================================
 * Implementation (static/inline — header-only)
 * ============================================================================ */

static void xb_engine_send(XBoardEngine *eng, const char *line) {
    if (!eng->engine_ok || !eng->to_engine) return;
    fprintf(eng->to_engine, "%s\n", line);
    fflush(eng->to_engine);
}

static void xb_engine_sendf(XBoardEngine *eng, const char *fmt, ...) {
    if (!eng->engine_ok || !eng->to_engine) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(eng->to_engine, fmt, ap);
    va_end(ap);
    fprintf(eng->to_engine, "\n");
    fflush(eng->to_engine);
}

static char xb_piece_to_fen(ChessPiece p) {
    char c;
    switch (p.type) {
        case PAWN:   c = 'p'; break;
        case KNIGHT: c = 'n'; break;
        case BISHOP: c = 'b'; break;
        case ROOK:   c = 'r'; break;
        case QUEEN:  c = 'q'; break;
        case KING:   c = 'k'; break;
        default:     return 0;
    }
    return (p.color == WHITE) ? (char)(c - 32) : c;
}

static void chess_game_to_fen(ChessGameState *game, char *buf, size_t buf_len) {
    char *p = buf, *end = buf + buf_len - 1;
    for (int row = 0; row < BOARD_SIZE && p < end; row++) {
        int empty = 0;
        for (int col = 0; col < BOARD_SIZE && p < end; col++) {
            ChessPiece piece = game->board[row][col];
            if (piece.type == EMPTY) {
                empty++;
            } else {
                if (empty) { p += snprintf(p, (size_t)(end-p), "%d", empty); empty = 0; }
                char c = xb_piece_to_fen(piece);
                if (p < end) *p++ = c;
            }
        }
        if (empty && p < end) { p += snprintf(p, (size_t)(end-p), "%d", empty); }
        if (row < 7 && p < end) *p++ = '/';
    }
    p += snprintf(p, (size_t)(end-p), " %c", game->turn == WHITE ? 'w' : 'b');
    char castling[5] = {0}; int ci = 0;
    if (!game->white_king_moved) {
        if (!game->white_rook_h_moved) castling[ci++] = 'K';
        if (!game->white_rook_a_moved) castling[ci++] = 'Q';
    }
    if (!game->black_king_moved) {
        if (!game->black_rook_h_moved) castling[ci++] = 'k';
        if (!game->black_rook_a_moved) castling[ci++] = 'q';
    }
    if (ci == 0) castling[ci++] = '-';
    castling[ci] = '\0';
    p += snprintf(p, (size_t)(end-p), " %s", castling);
    if (game->en_passant_col >= 0)
        p += snprintf(p, (size_t)(end-p), " %c%d", 'a'+game->en_passant_col, 8-game->en_passant_row);
    else
        p += snprintf(p, (size_t)(end-p), " -");
    snprintf(p, (size_t)(end-p), " 0 1");
    *end = '\0';
}

static bool xboard_parse_move(const char *token, ChessMove *out) {
    if (!token || strlen(token) < 4) return false;
    if (token[0]<'a'||token[0]>'h') return false;
    if (token[1]<'1'||token[1]>'8') return false;
    if (token[2]<'a'||token[2]>'h') return false;
    if (token[3]<'1'||token[3]>'8') return false;
    out->from_col = token[0]-'a';
    out->from_row = 8-(token[1]-'0');
    out->to_col   = token[2]-'a';
    out->to_row   = 8-(token[3]-'0');
    out->score    = 0;
    return true;
}

static void *xb_reader_thread(void *arg) {
    XBoardEngine *eng = (XBoardEngine *)arg;
    char line[512];
    while (eng->engine_ok && fgets(line, sizeof(line), eng->from_engine)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r'||line[len-1]==' '))
            line[--len] = '\0';
        if (len == 0) continue;

        const char *mv_token = NULL;
        if (strncmp(line, "move ", 5) == 0) {
            mv_token = line + 5;
        } else if (strncmp(line, "My move is", 10) == 0) {
            const char *colon = strchr(line+10, ':');
            mv_token = colon ? colon+2 : line+11;
            while (*mv_token == ' ') mv_token++;
        }

        if (mv_token) {
            ChessMove mv;
            if (xboard_parse_move(mv_token, &mv)) {
                pthread_mutex_lock(&eng->lock);
                eng->best_move = mv;
                eng->has_move  = true;
                eng->thinking  = false;
                pthread_cond_signal(&eng->cond);
                pthread_mutex_unlock(&eng->lock);
            }
        }

        /* Parse "feature myname="GNU Chess 6.2.9"" from protover 2 handshake */
        const char *fn = strstr(line, "myname=");
        if (fn) {
            fn += 7;  /* skip "myname=" */
            char name[128] = {0};
            if (*fn == '"') {
                /* quoted: feature myname="GNU Chess 6.2.9" */
                fn++;
                int ni = 0;
                while (*fn && *fn != '"' && ni < 127) name[ni++] = *fn++;
            } else {
                /* unquoted: feature myname=GnuChess */
                int ni = 0;
                while (*fn && *fn != ' ' && *fn != '\r' && ni < 127) name[ni++] = *fn++;
            }
            if (name[0]) {
                pthread_mutex_lock(&eng->lock);
                strncpy(eng->engine_name, name, sizeof(eng->engine_name)-1);
                pthread_mutex_unlock(&eng->lock);
            }
        }
    }
    pthread_mutex_lock(&eng->lock);
    eng->engine_ok = false;
    eng->thinking  = false;
    pthread_cond_signal(&eng->cond);
    pthread_mutex_unlock(&eng->lock);
    return NULL;
}

static bool xboard_engine_init(XBoardEngine *eng, const char *engine_cmd) {
    memset(eng, 0, sizeof(*eng));
    strncpy(eng->engine_cmd, engine_cmd, sizeof(eng->engine_cmd)-1);
    eng->engine_ok = false;

    int to_child[2], from_child[2];
    if (pipe(to_child) < 0 || pipe(from_child) < 0) return false;

    pid_t pid = fork();
    if (pid < 0) { close(to_child[0]); close(to_child[1]); close(from_child[0]); close(from_child[1]); return false; }

    if (pid == 0) {
        dup2(to_child[0],   STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[1]); close(from_child[0]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) dup2(devnull, STDERR_FILENO);
        execl("/bin/sh", "sh", "-c", engine_cmd, (char *)NULL);
        _exit(127);
    }

    close(to_child[0]); close(from_child[1]);
    eng->child_pid   = pid;
    eng->to_engine   = fdopen(to_child[1],   "w");
    eng->from_engine = fdopen(from_child[0], "r");
    if (!eng->to_engine || !eng->from_engine) { kill(pid, SIGKILL); return false; }

    eng->engine_ok = true;
    pthread_mutex_init(&eng->lock, NULL);
    pthread_cond_init(&eng->cond, NULL);

    xb_engine_send(eng, "xboard");
    xb_engine_send(eng, "protover 2");
    xb_engine_send(eng, "level 0 5 0");
    xb_engine_send(eng, "sd " XBOARD_DEFAULT_DEPTH_STR);
    xb_engine_send(eng, "new");
    xb_engine_send(eng, "force");

    if (pthread_create(&eng->reader_thread, NULL, xb_reader_thread, eng) != 0) {
        xb_engine_send(eng, "quit");
        fclose(eng->to_engine); fclose(eng->from_engine);
        kill(pid, SIGKILL);
        eng->engine_ok = false;
        return false;
    }
    return true;
}

static void xboard_engine_quit(XBoardEngine *eng) {
    if (!eng->engine_ok) return;
    eng->engine_ok = false;
    xb_engine_send(eng, "quit");
    fclose(eng->to_engine);   eng->to_engine   = NULL;
    fclose(eng->from_engine); eng->from_engine = NULL;
    pthread_join(eng->reader_thread, NULL);
    waitpid(eng->child_pid, NULL, 0);
    pthread_mutex_destroy(&eng->lock);
    pthread_cond_destroy(&eng->cond);
}

static void xboard_engine_set_depth(XBoardEngine *eng, int depth) {
    xb_engine_sendf(eng, "sd %d", depth);
}

static void xboard_start_thinking(XBoardEngine *eng, ChessGameState *game) {
    if (!eng->engine_ok) return;
    pthread_mutex_lock(&eng->lock);
    eng->has_move  = false;
    eng->thinking  = true;
    eng->last_game = *game;
    pthread_mutex_unlock(&eng->lock);
    char fen[128];
    chess_game_to_fen(game, fen, sizeof(fen));
    xb_engine_send(eng, "force");
    xb_engine_sendf(eng, "setboard %s", fen);
    xb_engine_send(eng, "go");
}

static bool xboard_has_move(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    bool r = eng->has_move;
    pthread_mutex_unlock(&eng->lock);
    return r;
}

static bool xboard_is_thinking(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    bool r = eng->thinking;
    pthread_mutex_unlock(&eng->lock);
    return r;
}

static ChessMove xboard_get_best_move_now(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    ChessMove mv;
    if (eng->has_move) {
        mv = eng->best_move;
        eng->has_move = false;
    } else {
        memset(&mv, 0, sizeof(mv));
        mv.from_row = -1;
    }
    pthread_mutex_unlock(&eng->lock);
    return mv;
}

#endif /* XBOARD_ENGINE_H */
