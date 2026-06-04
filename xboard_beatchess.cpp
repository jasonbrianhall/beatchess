/*
 * BeatChess WINBOARD Engine - Fixed Version
 * Uses the actual chess_ai_move.cpp AI engine for move calculation
 * 
 * FIXES:
 * - Proper handling of all three game modes (WvA, BvA, AvA)
 * - Game status checking to detect checkmate/stalemate
 * - Controlled move generation without board corruption
 * - Proper "go" command implementation
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

typedef enum { WvA, BvA, AvA } GameMode;

static int search_depth = 4;
static FILE *debug_log = NULL;
static GameMode game_mode = WvA;
static bool waiting_for_go_in_ava = false;  /* For AvA mode, wait for explicit "go" */

void debug_print(const char *fmt, ...) {
    if (!debug_log) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(debug_log, fmt, args);
    va_end(args);
    fflush(debug_log);
}

bool in_bounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

char *move_to_str(ChessMove m) {
    static char buf[10];
    snprintf(buf, 10, "%c%d%c%d", 'a' + m.from_col, 8 - m.from_row, 'a' + m.to_col, 8 - m.to_row);
    return buf;
}

ChessMove str_to_move(const char *str) {
    return (ChessMove){8 - (str[1] - '0'), str[0] - 'a', 8 - (str[3] - '0'), str[2] - 'a', 0};
}

/**
 * Play out an entire AvA game (AI vs AI)
 * Keeps making moves until checkmate or stalemate
 */
void play_ava_game(ChessGameState *game, ChessAIConfig config) {
    debug_print("[AvA] Starting AI vs AI game\n");
    
    int move_count = 0;
    while (1) {
        /* Check game status */
        ChessGameStatus status = chess_check_game_status(game);
        
        if (status == CHESS_CHECKMATE_WHITE) {
            debug_print("[AvA] Game Over: BLACK WINS (White checkmated)\n");
            break;
        }
        if (status == CHESS_CHECKMATE_BLACK) {
            debug_print("[AvA] Game Over: WHITE WINS (Black checkmated)\n");
            break;
        }
        if (status == CHESS_STALEMATE) {
            debug_print("[AvA] Game Over: STALEMATE\n");
            break;
        }
        
        /* Compute and make move */
        debug_print("[AvA] Move %d (%s to move)\n", move_count + 1, 
                   game->turn == WHITE ? "WHITE" : "BLACK");
        
        ChessAIMoveResult result = chess_ai_compute_move(game, config);
        
        if (result.move.from_row < 0) {
            debug_print("[AvA] ERROR: No valid move found!\n");
            break;
        }
        
        /* Make the move internally */
        chess_make_move(game, result.move);
        
        /* Output the move */
        printf("move %s\n", move_to_str(result.move));
        fflush(stdout);
        
        debug_print("[AvA] Sent: %s (score: %d)\n", move_to_str(result.move), result.score);
        
        move_count++;
        
        /* Safety check: prevent infinite loops */
        if (move_count > 500) {
            debug_print("[AvA] WARNING: Game exceeded 500 moves, stopping\n");
            break;
        }
    }
    
    debug_print("[AvA] Game finished after %d moves\n", move_count);
}

int main(void) {
    ChessGameState game;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    debug_log = fopen("beatchess.log", "w");
    if (debug_log) {
        debug_print("=== BeatChess Engine ===\n");
    }
    
    chess_init_board(&game);

    ChessAIConfig config;
    config.search_depth = search_depth;
    config.threshold_centipawns = 25;
    config.use_randomization = true;

    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        debug_print("[IN] %s\n", line);
        
        if (strcmp(line, "xboard") == 0) {
            debug_print("[CMD] xboard\n");
        } 
        else if (strncmp(line, "protover", 8) == 0) {
            debug_print("[CMD] protover\n");
            printf("feature myname=\"BeatChess\"\n");
            printf("feature usermove=1\n");
            printf("feature sigint=0 sigterm=0\n");
            printf("feature done=1\n");
            fflush(stdout);
        }
        else if (strcmp(line, "new") == 0) {
            debug_print("[CMD] new\n");
            chess_init_board(&game);
            game_mode = WvA;  /* Default to WvA */
            waiting_for_go_in_ava = false;
            debug_print("[STATE] Game mode: WvA (default), waiting for color assignment\n");
        }
        else if (strcmp(line, "go") == 0) {
            debug_print("[CMD] go\n");
            
            if (game_mode == AvA) {
                /* In AvA mode, "go" means play the whole game */
                play_ava_game(&game, config);
                waiting_for_go_in_ava = false;
            } else {
                /* In WvA/BvA, "go" means make one move if it's our turn */
                if ((game_mode == WvA && game.turn == BLACK) ||
                    (game_mode == BvA && game.turn == WHITE)) {
                    
                    debug_print("[GO] Computing move\n");
                    ChessAIMoveResult result = chess_ai_compute_move(&game, config);
                    
                    if (result.move.from_row >= 0) {
                        chess_make_move(&game, result.move);
                        printf("move %s\n", move_to_str(result.move));
                        fflush(stdout);
                        debug_print("[GO] Move: %s\n", move_to_str(result.move));
                    } else {
                        debug_print("[GO] ERROR: No valid move\n");
                    }
                } else {
                    debug_print("[GO] Not our turn\n");
                }
            }
        }
        else if (strncmp(line, "usermove ", 9) == 0) {
            const char *mv = line + 9;
            debug_print("[CMD] usermove: %s\n", mv);
            debug_print("[TURN-BEFORE] %s\n", game.turn == WHITE ? "WHITE" : "BLACK");
        
            ChessMove m = str_to_move(mv);
            chess_make_move(&game, m);
            debug_print("[TURN-AFTER] %s\n", game.turn == WHITE ? "WHITE" : "BLACK");
            
            /* Check if game has ended after opponent move */
            ChessGameStatus status = chess_check_game_status(&game);
            if (status == CHESS_CHECKMATE_WHITE) {
                debug_print("[STATUS] BLACK WINS (checkmate)\n");
            } else if (status == CHESS_CHECKMATE_BLACK) {
                debug_print("[STATUS] WHITE WINS (checkmate)\n");
            } else if (status == CHESS_STALEMATE) {
                debug_print("[STATUS] STALEMATE\n");
            }
        }
        else if (strncmp(line, "level", 5) == 0) {
            debug_print("[CMD] level (ignored)\n");
        }
        else if (strncmp(line, "time", 4) == 0) {
            debug_print("[CMD] time (ignored)\n");
        }
        else if (strncmp(line, "otim", 4) == 0) {
            debug_print("[CMD] otim (ignored)\n");
        }
        else if (strncmp(line, "computer", 8) == 0) {
            debug_print("[CMD] computer (opponent is AI)\n");
        }
        else if (strncmp(line, "accepted", 8) == 0) {
            debug_print("[CMD] accepted\n");
        }
        else if (strcmp(line, "random") == 0) {
            debug_print("[CMD] random\n");
        }
        else if (strcmp(line, "post") == 0) {
            debug_print("[CMD] post\n");
        }
        else if (strcmp(line, "hard") == 0) {
            debug_print("[CMD] hard\n");
        }
        else if (strcmp(line, "easy") == 0) {
            debug_print("[CMD] easy\n");
        }
        else if (strcmp(line, "white") == 0) {
            debug_print("[CMD] white - engine plays WHITE\n");
            game_mode = BvA;  /* Engine is White, opponent is Black */
            debug_print("[STATE] Game mode: BvA (engine plays White)\n");
        }
        else if (strcmp(line, "black") == 0) {
            debug_print("[CMD] black - engine plays BLACK\n");
            game_mode = WvA;  /* Engine is Black, opponent is White */
            debug_print("[STATE] Game mode: WvA (engine plays Black)\n");
        }
        else if (strcmp(line, "quit") == 0) {
            debug_print("[CMD] quit\n");
            break;
        }
        else if (strlen(line) > 0) {
            debug_print("[CMD] unknown: %s\n", line);
        }
        
        /* Handle automatic moves in WvA/BvA when game is still playing */
        if (game_mode != AvA) {  /* Not in AvA mode */
            ChessGameStatus status = chess_check_game_status(&game);
            
            if (status == CHESS_PLAYING) {
                /* Auto-move if it's our turn */
                if ((game_mode == WvA && game.turn == BLACK) ||
                    (game_mode == BvA && game.turn == WHITE)) {
                    
                    debug_print("[AUTO] Computing move\n");
                    ChessAIMoveResult result = chess_ai_compute_move(&game, config);
                    
                    if (result.move.from_row >= 0) {
                        chess_make_move(&game, result.move);
                        printf("move %s\n", move_to_str(result.move));
                        fflush(stdout);
                        debug_print("[AUTO] Move: %s\n", move_to_str(result.move));
                    }
                }
            }
        }
        
        debug_print("[LOOP] Ready for next input\n\n");
    }
    
    if (debug_log) {
        debug_print("\n[SHUTDOWN] Engine shutting down\n");
        fclose(debug_log);
    }
    return 0;
}
