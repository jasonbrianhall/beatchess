#ifndef XBOARD_ENGINE_H
#define XBOARD_ENGINE_H

/*
 * xboard_engine.h — XBoard/WinBoard protocol engine subprocess wrapper
 *
 * Spawns an external chess engine (gnuchess, crafty, stockfish --xboard, etc.)
 * as a child process and communicates over stdin/stdout using the XBoard protocol.
 *
 * Drop-in companion to ChessThinkingState: call xboard_start_thinking() instead
 * of chess_start_thinking(), and xboard_get_best_move() instead of
 * chess_get_best_move_now().
 *
 * Usage:
 *   XBoardEngine eng;
 *   xboard_engine_init(&eng, "gnuchess --xboard");   // or "stockfish"
 *   ...
 *   xboard_start_thinking(&eng, &game_state);
 *   // later in your update loop:
 *   if (xboard_has_move(&eng)) {
 *       ChessMove mv = xboard_get_best_move(&eng);
 *       // apply mv ...
 *   }
 *   xboard_engine_quit(&eng);
 */

#include "beatchess.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#ifndef XBOARD_DEFAULT_DEPTH
#define XBOARD_DEFAULT_DEPTH 4
#endif
/* Stringify helper for compile-time integer → string */
#define XBOARD_STRINGIFY(x) #x
#define XBOARD_TOSTR(x)     XBOARD_STRINGIFY(x)
#define XBOARD_DEFAULT_DEPTH_STR XBOARD_TOSTR(XBOARD_DEFAULT_DEPTH)

/* ============================================================================
 * XBoardEngine state
 * ============================================================================ */

typedef struct {
    /* Child process pipes */
    FILE   *to_engine;       /* write commands here  */
    FILE   *from_engine;     /* read responses here  */
    pid_t   child_pid;

    /* Reader thread */
    pthread_t      reader_thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;

    /* Move result (written by reader thread, read by main thread) */
    ChessMove best_move;
    bool      has_move;
    bool      thinking;

    /* Internal game sync state */
    ChessGameState last_game;  /* the game we sent to the engine */
    bool           engine_ok;  /* false if engine process died   */

    /* Engine command string, e.g. "gnuchess --xboard" */
    char engine_cmd[256];
} XBoardEngine;


/* ============================================================================
 * Public API
 * ============================================================================ */

/*
 * Spawn the engine subprocess and perform the XBoard handshake.
 * engine_cmd: shell command to run, e.g. "gnuchess --xboard",
 *             "crafty", "stockfish" (stockfish supports xboard via --xboard or
 *             UCI-to-xboard bridge).
 * Returns true on success.
 */
bool xboard_engine_init(XBoardEngine *eng, const char *engine_cmd);

/*
 * Send "quit" to the engine and close all handles.
 */
void xboard_engine_quit(XBoardEngine *eng);

/*
 * Set a new search depth hint (sent as "sd <depth>" before each "go").
 * Default is MAX_CHESS_DEPTH from beatchess.h.
 */
void xboard_engine_set_depth(XBoardEngine *eng, int depth);

/*
 * Tell the engine to think about the current position.
 * Sends "setboard <FEN>" then "go".
 * The result is delivered asynchronously; poll with xboard_has_move().
 */
void xboard_start_thinking(XBoardEngine *eng, ChessGameState *game);

/*
 * Returns true if the engine has produced a move since the last
 * xboard_start_thinking() call.
 */
bool xboard_has_move(XBoardEngine *eng);

/*
 * Returns true while the engine is still calculating.
 */
bool xboard_is_thinking(XBoardEngine *eng);

/*
 * Retrieve the best move (only valid when xboard_has_move() is true).
 * Clears the pending move flag.
 */
ChessMove xboard_get_best_move(XBoardEngine *eng);

/*
 * Convenience: mirror of chess_get_best_move_now() — returns the move
 * if ready, else returns a zeroed ChessMove with from_row == -1.
 */
ChessMove xboard_get_best_move_now(XBoardEngine *eng);

/*
 * Convenience wrappers so you can use XBoardEngine wherever you used
 * ChessThinkingState, without renaming every call site:
 */
static inline void chess_start_thinking_xboard(XBoardEngine *eng,
                                                ChessGameState *game) {
    xboard_start_thinking(eng, game);
}

/* ============================================================================
 * FEN helpers (also useful elsewhere)
 * ============================================================================ */

/*
 * Generate a FEN string for the given game state.
 * buf must be at least 128 bytes.
 */
void chess_game_to_fen(ChessGameState *game, char *buf, size_t buf_len);

/*
 * Parse an XBoard "move" token (e.g. "e2e4", "e7e8q") into a ChessMove.
 * Returns false if the string cannot be parsed.
 */
bool xboard_parse_move(const char *token, ChessMove *out_move);

#endif /* XBOARD_ENGINE_H */
