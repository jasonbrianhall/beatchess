#ifndef XBOARD_ENGINE_H
#define XBOARD_ENGINE_H

/*
 * xboard_engine.h — XBoard engine subprocess interface
 * Implementation is in xboard.cpp (your existing file, extended).
 */

#include "beatchess.h"

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

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

    pthread_t       reader_thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;

    ChessMove best_move;
    bool      has_move;
    bool      thinking;

    ChessGameState last_game;
    bool           engine_ok;

    char engine_cmd[256];
    char engine_name[128];   /* from "feature myname=..." handshake */
} XBoardEngine;

/* ============================================================================
 * API — implemented in xboard.cpp
 * ============================================================================ */

bool      xboard_engine_init(XBoardEngine *eng, const char *engine_cmd);
void      xboard_engine_quit(XBoardEngine *eng);
void      xboard_engine_set_depth(XBoardEngine *eng, int depth);
void      xboard_start_thinking(XBoardEngine *eng, ChessGameState *game);
bool      xboard_has_move(XBoardEngine *eng);
bool      xboard_is_thinking(XBoardEngine *eng);
ChessMove xboard_get_best_move_now(XBoardEngine *eng);
bool      xboard_parse_move(const char *token, ChessMove *out_move);
void      chess_game_to_fen(ChessGameState *game, char *buf, size_t buf_len);

#endif /* XBOARD_ENGINE_H */
