/*
 * xboard_engine.cpp — XBoard/WinBoard protocol engine subprocess wrapper
 *
 * Compile alongside your other SDL sources:
 *   g++ ... xboard_engine.cpp -lpthread
 */

#include "xboard_engine.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/* Send a line to the engine (adds newline, flushes). */
static void engine_send(XBoardEngine *eng, const char *line) {
    if (!eng->engine_ok || !eng->to_engine) return;
    fprintf(eng->to_engine, "%s\n", line);
    fflush(eng->to_engine);
}

static void engine_sendf(XBoardEngine *eng, const char *fmt, ...) {
    if (!eng->engine_ok || !eng->to_engine) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(eng->to_engine, fmt, ap);
    va_end(ap);
    fprintf(eng->to_engine, "\n");
    fflush(eng->to_engine);
}

/* ============================================================================
 * FEN generation
 * ============================================================================ */

static char piece_to_fen_char(ChessPiece p) {
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
    return (p.color == WHITE) ? (char)toupper(c) : c;
}

void chess_game_to_fen(ChessGameState *game, char *buf, size_t buf_len) {
    char *p = buf;
    char *end = buf + buf_len - 1;

    /* Piece placement — rank 8 (row 0) to rank 1 (row 7) */
    for (int row = 0; row < BOARD_SIZE && p < end; row++) {
        int empty = 0;
        for (int col = 0; col < BOARD_SIZE && p < end; col++) {
            ChessPiece piece = game->board[row][col];
            if (piece.type == EMPTY) {
                empty++;
            } else {
                if (empty) { p += snprintf(p, end - p, "%d", empty); empty = 0; }
                char c = piece_to_fen_char(piece);
                if (p < end) *p++ = c;
            }
        }
        if (empty && p < end) { p += snprintf(p, end - p, "%d", empty); }
        if (row < 7 && p < end) *p++ = '/';
    }

    /* Active color */
    p += snprintf(p, end - p, " %c", game->turn == WHITE ? 'w' : 'b');

    /* Castling availability */
    char castling[5] = {0};
    int ci = 0;
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
    p += snprintf(p, end - p, " %s", castling);

    /* En passant target square */
    if (game->en_passant_col >= 0) {
        char ep_file = 'a' + game->en_passant_col;
        int  ep_rank = 8 - game->en_passant_row;   /* row 0 = rank 8 */
        p += snprintf(p, end - p, " %c%d", ep_file, ep_rank);
    } else {
        p += snprintf(p, end - p, " -");
    }

    /* Halfmove clock and fullmove number (we don't track these precisely) */
    p += snprintf(p, end - p, " 0 1");

    *p = '\0';
}

/* ============================================================================
 * XBoard move parsing  (e.g. "e2e4", "e7e8q")
 * ============================================================================ */

bool xboard_parse_move(const char *token, ChessMove *out_move) {
    /* Expect at least 4 chars: file rank file rank */
    if (!token || strlen(token) < 4) return false;
    if (token[0] < 'a' || token[0] > 'h') return false;
    if (token[1] < '1' || token[1] > '8') return false;
    if (token[2] < 'a' || token[2] > 'h') return false;
    if (token[3] < '1' || token[3] > '8') return false;

    int from_col = token[0] - 'a';
    int from_row = 8 - (token[1] - '0');   /* rank 8 = row 0 */
    int to_col   = token[2] - 'a';
    int to_row   = 8 - (token[3] - '0');

    out_move->from_col = from_col;
    out_move->from_row = from_row;
    out_move->to_col   = to_col;
    out_move->to_row   = to_row;
    out_move->score    = 0;
    /* Promotion piece (token[4]) is ignored for now — chess_execute_move
     * in beatchess already promotes to queen by default. */
    return true;
}

/* ============================================================================
 * Reader thread — runs in background, blocks on fgets from engine stdout
 * ============================================================================ */

static void *reader_thread_fn(void *arg) {
    XBoardEngine *eng = (XBoardEngine *)arg;
    char line[512];

    while (eng->engine_ok && fgets(line, sizeof(line), eng->from_engine)) {
        /* Strip trailing whitespace */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                           || line[len-1] == ' ')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        /* XBoard engines send move responses as:
         *   "move e2e4"        (most engines)
         *   "My move is: e2e4" (gnuchess legacy format)
         * Also skip lines starting with '#' (comments) or known noise. */

        const char *mv_token = NULL;

        if (strncmp(line, "move ", 5) == 0) {
            mv_token = line + 5;
        } else if (strncmp(line, "My move is", 10) == 0) {
            /* "My move is: e2e4" or "My move is e2e4" */
            const char *colon = strchr(line + 10, ':');
            mv_token = colon ? colon + 2 : line + 11;
            while (*mv_token == ' ') mv_token++;
        }

        if (mv_token) {
            ChessMove mv;
            if (xboard_parse_move(mv_token, &mv)) {
                SDL_Log("[engine] move: %s", mv_token);
                pthread_mutex_lock(&eng->lock);
                eng->best_move = mv;
                eng->has_move  = true;
                eng->thinking  = false;
                pthread_cond_signal(&eng->cond);
                pthread_mutex_unlock(&eng->lock);
            }
        } else {
            /* Log thinking lines: XBoard thinking output starts with a
             * numeric depth field, e.g. " 4  +42  12  98234  e2e4 e7e5 ..."
             * Also log any other non-empty line for visibility. */
            char *p = line;
            while (*p == ' ') p++;
            if (*p >= '0' && *p <= '9')
                SDL_Log("[engine thinking] %s", line);
            else
                SDL_Log("[engine] %s", line);
        }
        /* Parse engine name from protover 2 handshake:
         *   feature myname="GNU Chess 6.2.9"
         *   feature myname=Crafty */
        const char *fn = strstr(line, "myname=");
        if (fn) {
            fn += 7;
            char name[128] = {0};
            int ni = 0;
            if (*fn == '"') {
                fn++;
                while (*fn && *fn != '"' && ni < 127) name[ni++] = *fn++;
            } else {
                while (*fn && *fn != ' ' && ni < 127) name[ni++] = *fn++;
            }
            if (name[0]) {
                pthread_mutex_lock(&eng->lock);
                strncpy(eng->engine_name, name, sizeof(eng->engine_name) - 1);
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

/* ============================================================================
 * Public API implementation
 * ============================================================================ */

bool xboard_engine_init(XBoardEngine *eng, const char *engine_cmd) {
    memset(eng, 0, sizeof(*eng));
    strncpy(eng->engine_cmd, engine_cmd, sizeof(eng->engine_cmd) - 1);
    eng->engine_ok = false;

    /* Create two pipes: parent→child (stdin) and child→parent (stdout) */
    int to_child[2], from_child[2];
    if (pipe(to_child) < 0 || pipe(from_child) < 0) {
        perror("xboard_engine_init: pipe");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("xboard_engine_init: fork");
        return false;
    }

    if (pid == 0) {
        /* Child process */
        dup2(to_child[0],   STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        /* Close unused ends */
        close(to_child[1]);
        close(from_child[0]);
        /* Redirect stderr to /dev/null to avoid polluting our pipe */
        FILE *devnull = fopen("/dev/null", "w");
        if (devnull) dup2(fileno(devnull), STDERR_FILENO);

        execl("/bin/sh", "sh", "-c", engine_cmd, (char *)NULL);
        _exit(127);   /* execl failed */
    }

    /* Parent process */
    close(to_child[0]);
    close(from_child[1]);

    eng->child_pid   = pid;
    eng->to_engine   = fdopen(to_child[1],   "w");
    eng->from_engine = fdopen(from_child[0], "r");

    if (!eng->to_engine || !eng->from_engine) {
        perror("xboard_engine_init: fdopen");
        kill(pid, SIGKILL);
        return false;
    }

    eng->engine_ok = true;

    pthread_mutex_init(&eng->lock, NULL);
    pthread_cond_init(&eng->cond, NULL);

    /* XBoard handshake */
    engine_send(eng, "xboard");
    engine_send(eng, "protover 2");
    engine_send(eng, "post");            /* enable thinking output           */
    /* Time control set later via xboard_engine_set_time(); default to fixed depth */
    engine_send(eng, "sd " XBOARD_DEFAULT_DEPTH_STR);
    engine_send(eng, "new");
    engine_send(eng, "force");   /* engine won't move until we say "go" */

    /* Start background reader thread */
    if (pthread_create(&eng->reader_thread, NULL, reader_thread_fn, eng) != 0) {
        perror("xboard_engine_init: pthread_create");
        engine_send(eng, "quit");
        fclose(eng->to_engine);
        fclose(eng->from_engine);
        kill(pid, SIGKILL);
        eng->engine_ok = false;
        return false;
    }

    return true;
}

void xboard_engine_quit(XBoardEngine *eng) {
    if (!eng->engine_ok) return;
    eng->engine_ok = false;
    engine_send(eng, "quit");
    fclose(eng->to_engine);   eng->to_engine   = NULL;
    fclose(eng->from_engine); eng->from_engine = NULL;
    pthread_join(eng->reader_thread, NULL);
    waitpid(eng->child_pid, NULL, 0);
    pthread_mutex_destroy(&eng->lock);
    pthread_cond_destroy(&eng->cond);
}

void xboard_engine_set_depth(XBoardEngine *eng, int depth) {
    engine_sendf(eng, "sd %d", depth);
}

void xboard_engine_set_time(XBoardEngine *eng, int total_ms) {
    eng->time_limit_ms    = total_ms;
    eng->time_remaining_ms = total_ms;
    if (total_ms > 0) {
        /* "level moves minutes increment" — game in N minutes, 0 increment */
        int minutes = total_ms / 60000;
        int seconds = (total_ms % 60000) / 1000;
        if (seconds > 0)
            engine_sendf(eng, "level 0 %d:%02d 0", minutes, seconds);
        else
            engine_sendf(eng, "level 0 %d 0", minutes);
    } else {
        /* Back to fixed depth, no time control */
        engine_sendf(eng, "sd %d", XBOARD_DEFAULT_DEPTH);
    }
}

void xboard_move_made(XBoardEngine *eng, int elapsed_ms) {
    if (eng->time_limit_ms > 0) {
        eng->time_remaining_ms -= elapsed_ms;
        if (eng->time_remaining_ms < 0) eng->time_remaining_ms = 0;
    }
}

void xboard_start_thinking(XBoardEngine *eng, ChessGameState *game) {
    if (!eng->engine_ok) return;

    pthread_mutex_lock(&eng->lock);
    eng->has_move = false;
    eng->thinking = true;
    eng->last_game = *game;
    pthread_mutex_unlock(&eng->lock);

    /* Use "setboard <FEN>" to sync position, then "go" */
    char fen[128];
    chess_game_to_fen(game, fen, sizeof(fen));
    engine_send(eng, "force");          /* halt engine if it was pondering   */
    engine_sendf(eng, "setboard %s", fen);
    if (eng->time_limit_ms > 0) {
        /* Tell engine how much time remains (in centiseconds) */
        int cs = eng->time_remaining_ms / 10;
        engine_sendf(eng, "time %d", cs);
    }
    engine_send(eng, "go");             /* engine plays the side to move      */
}

bool xboard_has_move(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    bool r = eng->has_move;
    pthread_mutex_unlock(&eng->lock);
    return r;
}

bool xboard_is_thinking(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    bool r = eng->thinking;
    pthread_mutex_unlock(&eng->lock);
    return r;
}

ChessMove xboard_get_best_move(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    ChessMove mv = eng->best_move;
    eng->has_move = false;
    pthread_mutex_unlock(&eng->lock);
    return mv;
}

ChessMove xboard_get_best_move_now(XBoardEngine *eng) {
    pthread_mutex_lock(&eng->lock);
    ChessMove mv;
    if (eng->has_move) {
        mv = eng->best_move;
        eng->has_move = false;
    } else {
        memset(&mv, 0, sizeof(mv));
        mv.from_row = -1;   /* sentinel: no move available yet */
    }
    pthread_mutex_unlock(&eng->lock);
    return mv;
}
