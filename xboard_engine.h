#ifndef XBOARD_ENGINE_H
#define XBOARD_ENGINE_H

/*
 * xboard_engine.h — XBoard engine subprocess interface
 * Implementation is in xboard.cpp (your existing file, extended).
 *
 * mingw-w64 ships its own pthread.h/pthreadGC2 that provides pthread_t,
 * pthread_mutex_t, and pthread_cond_t on Windows, so we use it uniformly.
 * The only platform-specific field is the child process handle.
 */

#include "beatchess.h"

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef _WIN32
#  include <unistd.h>
#  include <sys/types.h>
#endif

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#ifndef XBOARD_DEFAULT_DEPTH
#define XBOARD_DEFAULT_DEPTH 4
#endif
#define XBOARD_STRINGIFY(x) #x
#define XBOARD_TOSTR(x)     XBOARD_STRINGIFY(x)
#define XBOARD_DEFAULT_DEPTH_STR XBOARD_TOSTR(XBOARD_DEFAULT_DEPTH)

/* ============================================================================
 * XBoardEngine state
 * ============================================================================ */

typedef enum { ENGINE_PROTOCOL_XBOARD, ENGINE_PROTOCOL_UCI } EngineProtocol;

typedef struct {
    FILE   *to_engine;
    FILE   *from_engine;

#ifdef _WIN32
    HANDLE  child_process;
#else
    pid_t   child_pid;
#endif

    pthread_t       reader_thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;

    ChessMove best_move;
    bool      has_move;
    bool      thinking;

    ChessGameState last_game;
    bool           engine_ok;

    char engine_cmd[256];
    char engine_name[128];   /* from "feature myname=..." or "id name ..." */

    int  time_limit_ms;      /* per-side time budget in ms; 0 = fixed depth */
    int  time_remaining_ms;

    EngineProtocol protocol;
} XBoardEngine;

/* ============================================================================
 * API — implemented in xboard.cpp
 * ============================================================================ */

bool      xboard_engine_init(XBoardEngine *eng, const char *engine_cmd);
bool      xboard_engine_init_uci(XBoardEngine *eng, const char *engine_cmd);
void      xboard_engine_quit(XBoardEngine *eng);
void      xboard_engine_set_depth(XBoardEngine *eng, int depth);
void      xboard_engine_set_time(XBoardEngine *eng, int total_ms);
void      xboard_start_thinking(XBoardEngine *eng, ChessGameState *game);
void      xboard_move_made(XBoardEngine *eng, int elapsed_ms);
bool      xboard_has_move(XBoardEngine *eng);
bool      xboard_is_thinking(XBoardEngine *eng);
ChessMove xboard_get_best_move_now(XBoardEngine *eng);
bool      xboard_parse_move(const char *token, ChessMove *out_move);
void      chess_game_to_fen(ChessGameState *game, char *buf, size_t buf_len);

#endif /* XBOARD_ENGINE_H */
