/*
 * xboard.cpp — XBoard/UCI protocol engine subprocess wrapper
 *
 * Supports Linux (fork/pipe) and Windows (CreateProcess/CreatePipe).
 * Both platforms use pthreads — mingw-w64 ships pthreadGC2 which provides
 * pthread_t/mutex/cond natively on Windows, so no shims are needed.
 *
 * Linux:   g++ ... xboard.cpp -lpthread
 * Windows: x86_64-w64-mingw32-g++ ... xboard.cpp -lpthread
 */

#include "xboard_engine.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <io.h>       /* _open_osfhandle, _fdopen */
#  include <fcntl.h>
#else
#  include <unistd.h>
#  include <signal.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#endif

/* ============================================================================
 * FEN generation  (shared)
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

    p += snprintf(p, end - p, " %c", game->turn == WHITE ? 'w' : 'b');

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

    if (game->en_passant_col >= 0) {
        char ep_file = 'a' + game->en_passant_col;
        int  ep_rank = 8 - game->en_passant_row;
        p += snprintf(p, end - p, " %c%d", ep_file, ep_rank);
    } else {
        p += snprintf(p, end - p, " -");
    }

    p += snprintf(p, end - p, " 0 1");
    *p = '\0';
}

/* ============================================================================
 * XBoard move parsing  (shared)
 * ============================================================================ */

bool xboard_parse_move(const char *token, ChessMove *out_move) {
    if (!token || strlen(token) < 4) return false;
    if (token[0] < 'a' || token[0] > 'h') return false;
    if (token[1] < '1' || token[1] > '8') return false;
    if (token[2] < 'a' || token[2] > 'h') return false;
    if (token[3] < '1' || token[3] > '8') return false;

    out_move->from_col = token[0] - 'a';
    out_move->from_row = 8 - (token[1] - '0');
    out_move->to_col   = token[2] - 'a';
    out_move->to_row   = 8 - (token[3] - '0');
    out_move->score    = 0;
    return true;
}

/* ============================================================================
 * Send helpers  (shared — FILE* works on both platforms)
 * ============================================================================ */

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
 * Reader thread  (shared)
 * ============================================================================ */

static void *reader_thread_fn(void *arg) {
    XBoardEngine *eng = (XBoardEngine *)arg;
    char line[512];

    while (eng->engine_ok && fgets(line, sizeof(line), eng->from_engine)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                           || line[len-1] == ' ')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        const char *mv_token = NULL;

        if (strncmp(line, "move ", 5) == 0) {
            mv_token = line + 5;
        } else if (strncmp(line, "My move is", 10) == 0) {
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
        } else if (eng->protocol == ENGINE_PROTOCOL_UCI) {
            if (strncmp(line, "bestmove ", 9) == 0) {
                const char *tok = line + 9;
                if (strncmp(tok, "(none)", 6) != 0) {
                    ChessMove mv;
                    if (xboard_parse_move(tok, &mv)) {
                        SDL_Log("[engine] bestmove: %s", tok);
                        pthread_mutex_lock(&eng->lock);
                        eng->best_move = mv;
                        eng->has_move  = true;
                        eng->thinking  = false;
                        pthread_cond_signal(&eng->cond);
                        pthread_mutex_unlock(&eng->lock);
                    }
                }
            } else if (strncmp(line, "id name ", 8) == 0) {
                pthread_mutex_lock(&eng->lock);
                strncpy(eng->engine_name, line + 8, sizeof(eng->engine_name) - 1);
                pthread_mutex_unlock(&eng->lock);
                SDL_Log("[engine] %s", line);
            } else if (strncmp(line, "info ", 5) == 0) {
                SDL_Log("[engine thinking] %s", line);
            } else {
                SDL_Log("[engine] %s", line);
            }
        }

        if (eng->protocol == ENGINE_PROTOCOL_XBOARD) {
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
    }

    pthread_mutex_lock(&eng->lock);
    eng->engine_ok = false;
    eng->thinking  = false;
    pthread_cond_signal(&eng->cond);
    pthread_mutex_unlock(&eng->lock);

    return NULL;
}

/* ============================================================================
 * Platform-specific subprocess spawn / kill
 * ============================================================================ */

#ifdef _WIN32

static bool spawn_engine(XBoardEngine *eng, const char *engine_cmd) {
    HANDLE pipe_stdin_r  = NULL, pipe_stdin_w  = NULL;
    HANDLE pipe_stdout_r = NULL, pipe_stdout_w = NULL;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;

    if (!CreatePipe(&pipe_stdin_r,  &pipe_stdin_w,  &sa, 0) ||
        !CreatePipe(&pipe_stdout_r, &pipe_stdout_w, &sa, 0)) {
        SDL_Log("[xboard] CreatePipe failed: %lu", GetLastError());
        return false;
    }

    /* Parent ends must not be inherited by the child */
    SetHandleInformation(pipe_stdin_w,  HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(pipe_stdout_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput   = pipe_stdin_r;
    si.hStdOutput  = pipe_stdout_w;
    si.hStdError   = INVALID_HANDLE_VALUE;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    char cmd_buf[512];
    strncpy(cmd_buf, engine_cmd, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';

    if (!CreateProcessA(NULL, cmd_buf, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        SDL_Log("[xboard] CreateProcess failed: %lu", GetLastError());
        CloseHandle(pipe_stdin_r);  CloseHandle(pipe_stdin_w);
        CloseHandle(pipe_stdout_r); CloseHandle(pipe_stdout_w);
        return false;
    }

    /* Close child's ends and the thread handle — we only need the process */
    CloseHandle(pipe_stdin_r);
    CloseHandle(pipe_stdout_w);
    CloseHandle(pi.hThread);

    eng->child_process = pi.hProcess;

    /* Wrap HANDLEs in FILE* so fprintf/fgets work identically to POSIX */
    int fd_w = _open_osfhandle((intptr_t)pipe_stdin_w,  _O_WRONLY);
    int fd_r = _open_osfhandle((intptr_t)pipe_stdout_r, _O_RDONLY);
    eng->to_engine   = (fd_w >= 0) ? _fdopen(fd_w, "w") : NULL;
    eng->from_engine = (fd_r >= 0) ? _fdopen(fd_r, "r") : NULL;

    if (!eng->to_engine || !eng->from_engine) {
        SDL_Log("[xboard] _fdopen failed");
        TerminateProcess(eng->child_process, 1);
        CloseHandle(eng->child_process);
        eng->child_process = NULL;
        return false;
    }

    return true;
}

static void kill_engine(XBoardEngine *eng) {
    if (eng->child_process) {
        TerminateProcess(eng->child_process, 0);
        WaitForSingleObject(eng->child_process, 1000);
        CloseHandle(eng->child_process);
        eng->child_process = NULL;
    }
}

#else  /* POSIX */

static bool spawn_engine(XBoardEngine *eng, const char *engine_cmd) {
    int to_child[2], from_child[2];
    if (pipe(to_child) < 0 || pipe(from_child) < 0) {
        perror("xboard: pipe");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("xboard: fork"); return false; }

    if (pid == 0) {
        dup2(to_child[0],   STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[1]);
        close(from_child[0]);
        FILE *devnull = fopen("/dev/null", "w");
        if (devnull) dup2(fileno(devnull), STDERR_FILENO);
        execl("/bin/sh", "sh", "-c", engine_cmd, (char *)NULL);
        _exit(127);
    }

    close(to_child[0]);
    close(from_child[1]);

    eng->child_pid   = pid;
    eng->to_engine   = fdopen(to_child[1],   "w");
    eng->from_engine = fdopen(from_child[0], "r");

    if (!eng->to_engine || !eng->from_engine) {
        perror("xboard: fdopen");
        kill(pid, SIGKILL);
        return false;
    }

    return true;
}

static void kill_engine(XBoardEngine *eng) {
    waitpid(eng->child_pid, NULL, WNOHANG);
    kill(eng->child_pid, SIGTERM);
}

#endif /* _WIN32 */

/* ============================================================================
 * Common init
 * ============================================================================ */

static bool common_init(XBoardEngine *eng, const char *engine_cmd) {
    memset(eng, 0, sizeof(*eng));
    strncpy(eng->engine_cmd, engine_cmd, sizeof(eng->engine_cmd) - 1);

    if (!spawn_engine(eng, engine_cmd))
        return false;

    eng->engine_ok = true;
    pthread_mutex_init(&eng->lock, NULL);
    pthread_cond_init(&eng->cond, NULL);

    if (pthread_create(&eng->reader_thread, NULL, reader_thread_fn, eng) != 0) {
        SDL_Log("[xboard] pthread_create failed");
        fclose(eng->to_engine);
        fclose(eng->from_engine);
        kill_engine(eng);
        eng->engine_ok = false;
        return false;
    }

    return true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

bool xboard_engine_init(XBoardEngine *eng, const char *engine_cmd) {
    if (!common_init(eng, engine_cmd)) return false;
    eng->protocol = ENGINE_PROTOCOL_XBOARD;
    engine_send(eng, "xboard");
    engine_send(eng, "protover 2");
    engine_send(eng, "post");
    engine_send(eng, "sd " XBOARD_DEFAULT_DEPTH_STR);
    engine_send(eng, "new");
    engine_send(eng, "force");
    return true;
}

bool xboard_engine_init_uci(XBoardEngine *eng, const char *engine_cmd) {
    if (!common_init(eng, engine_cmd)) return false;
    eng->protocol = ENGINE_PROTOCOL_UCI;
    engine_send(eng, "uci");
    engine_send(eng, "setoption name Threads value 1");
    engine_send(eng, "setoption name Hash value 32");
    engine_send(eng, "isready");
    engine_send(eng, "ucinewgame");
    return true;
}

void xboard_engine_quit(XBoardEngine *eng) {
    if (!eng->engine_ok) return;
    eng->engine_ok = false;

    if (eng->protocol == ENGINE_PROTOCOL_UCI)
        engine_send(eng, "stop");
    engine_send(eng, "quit");

    if (eng->to_engine)   { fclose(eng->to_engine);   eng->to_engine   = NULL; }
    if (eng->from_engine) { fclose(eng->from_engine); eng->from_engine = NULL; }

    pthread_detach(eng->reader_thread);
    kill_engine(eng);

    pthread_mutex_destroy(&eng->lock);
    pthread_cond_destroy(&eng->cond);
}

void xboard_engine_set_depth(XBoardEngine *eng, int depth) {
    engine_sendf(eng, "sd %d", depth);
}

void xboard_engine_set_time(XBoardEngine *eng, int total_ms) {
    eng->time_limit_ms     = total_ms;
    eng->time_remaining_ms = total_ms;
    if (eng->protocol == ENGINE_PROTOCOL_UCI) return;
    if (total_ms > 0) {
        int minutes = total_ms / 60000;
        int seconds = (total_ms % 60000) / 1000;
        if (seconds > 0)
            engine_sendf(eng, "level 0 %d:%02d 0", minutes, seconds);
        else
            engine_sendf(eng, "level 0 %d 0", minutes);
    } else {
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
    eng->has_move  = false;
    eng->thinking  = true;
    eng->last_game = *game;
    pthread_mutex_unlock(&eng->lock);

    char fen[128];
    chess_game_to_fen(game, fen, sizeof(fen));

    if (eng->protocol == ENGINE_PROTOCOL_UCI) {
        engine_sendf(eng, "position fen %s", fen);
        if (eng->time_limit_ms > 0) {
            int ms = eng->time_remaining_ms;
            engine_sendf(eng, "go wtime %d btime %d", ms, ms);
        } else {
            engine_sendf(eng, "go depth %d", XBOARD_DEFAULT_DEPTH);
        }
    } else {
        engine_send(eng, "force");
        engine_sendf(eng, "setboard %s", fen);
        if (eng->time_limit_ms > 0)
            engine_sendf(eng, "time %d", eng->time_remaining_ms / 10);
        engine_send(eng, "go");
    }
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
        mv.from_row = -1;
    }
    pthread_mutex_unlock(&eng->lock);
    return mv;
}
