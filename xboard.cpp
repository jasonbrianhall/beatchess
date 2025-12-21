/*
 * BeatChess WINBOARD Engine
 * Uses the actual chess_ai_move.cpp AI engine for move calculation
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

static ChessGameState game;
static int search_depth = 4;
static FILE *debug_log = NULL;

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

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    debug_log = fopen("beatchess.log", "w");
    if (debug_log) {
        debug_print("=== BeatChess Engine ===\n");
    }
    
    chess_init_board(&game);
    
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        debug_print("[IN] %s\n", line);
        
        if (strcmp(line, "xboard") == 0) {
            debug_print("[CMD] xboard mode\n");
        } 
        else if (strncmp(line, "protover", 8) == 0) {
            debug_print("[CMD] protover command\n");
            printf("feature done=1\n");
            fflush(stdout);
        }
        else if (strcmp(line, "new") == 0) {
            debug_print("[CMD] new game\n");
            chess_init_board(&game);
        }
        else if (strcmp(line, "go") == 0) {
            debug_print("[CMD] go - using chess_ai_compute_move()\n");
            ChessAIConfig config;
            config.search_depth = search_depth;
            config.threshold_centipawns = 25;
            config.use_randomization = false;
            
            ChessAIMoveResult result = chess_ai_compute_move(&game, config);
            if (result.move.from_row >= 0) {
                printf("%s\n", move_to_str(result.move));
                fflush(stdout);
                debug_print("[OUT] %s (score=%d, evaluated=%d)\n", 
                           move_to_str(result.move), result.score, result.total_moves_evaluated);
                //chess_make_move(&game, result.move);
            }
        }
        else if (strlen(line) >= 4 && isalpha(line[0]) && isdigit(line[1]) && 
                 isalpha(line[2]) && isdigit(line[3])) {
            debug_print("[CMD] opponent move: %s\n", line);
            ChessMove m = str_to_move(line);
            //chess_make_move(&game, m);
        }
        else if (strncmp(line, "level", 5) == 0) {
            debug_print("[CMD] level (time control) - ignored\n");
        }
        else if (strncmp(line, "time", 4) == 0) {
            debug_print("[CMD] time - ignored\n");
        }
        else if (strncmp(line, "otim", 4) == 0) {
            debug_print("[CMD] otim - ignored\n");
        }
        else if (strncmp(line, "accepted", 8) == 0) {
            debug_print("[CMD] accepted - feature acknowledged\n");
        }
        else if (strcmp(line, "random") == 0) {
            debug_print("[CMD] random mode\n");
        }
        else if (strcmp(line, "post") == 0) {
            debug_print("[CMD] post thinking\n");
        }
        else if (strcmp(line, "hard") == 0) {
            debug_print("[CMD] hard mode\n");
        }
        else if (strcmp(line, "easy") == 0) {
            debug_print("[CMD] easy mode\n");
        }
        else if (strcmp(line, "white") == 0) {
            debug_print("[CMD] white to move\n");
            game.turn = WHITE;
        }
        else if (strcmp(line, "black") == 0) {
            debug_print("[CMD] black to move\n");
            game.turn = BLACK;
        }
        else if (strcmp(line, "quit") == 0) {
            debug_print("[CMD] quit\n");
            break;
        }
        else if (strlen(line) > 0) {
            debug_print("[CMD] unknown: %s\n", line);
        }
    }
    
    if (debug_log) {
        debug_print("[SHUTDOWN] Engine shutting down\n");
        fclose(debug_log);
    }
    return 0;
}
