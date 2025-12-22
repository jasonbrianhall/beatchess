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

typedef enum { WvA, BvA, AvA } GameMode;

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
    ChessGameState game;
    int GameMode = WvA;  

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    debug_log = fopen("beatchess.log", "w");
    if (debug_log) {
        debug_print("=== BeatChess Engine ===\n");
    }
    
    chess_init_board(&game);
    bool computerset=false;

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
            debug_print("[CMD] xboard mfode\n");
        } 
        else if (strncmp(line, "protover", 8) == 0) {
            debug_print("[CMD] protover command\n");

            // Tell xboard who we are
            printf("feature myname=\"BeatChess\"\n");

            // Tell xboard we expect moves as: `usermove e2e4`
            printf("feature usermove=1\n");

            // (Optional but good) We can handle SIGINT/SIGTERM cleanly
            printf("feature sigint=0 sigterm=0\n");

            // We're done telling xboard our capabilities
            printf("feature done=1\n");
            fflush(stdout);
        }

        else if (strcmp(line, "new") == 0) {
            debug_print("[CMD] new game\n");
            chess_init_board(&game);
            computerset=false;
        }
        else if (strcmp(line, "go") == 0) {
            debug_print("[CMD] go - using chess_ai_compute_move()\n");
            debug_print("[Turn] %s\n", (game.turn == WHITE ? "white" : "black"));
          
            
        }
        else if (strncmp(line, "usermove ", 9) == 0) {
            const char *mv = line + 9;   // skip "usermove "
            debug_print("[CMD] opponent move: %s\n", mv);
            debug_print("[Turn] %s\n", (game.turn == WHITE ? "white" : "black"));
        
            ChessMove m = str_to_move(mv);
            chess_make_move(&game, m);

            debug_print("[Turn] %s\n", (game.turn == WHITE ? "white" : "black"));
        } else if (strncmp(line, "level", 5) == 0) {
            debug_print("[CMD] level (time control) - ignored\n");
        }
        else if (strncmp(line, "time", 4) == 0) {
            debug_print("[CMD] time - ignored\n");
        }
        else if (strncmp(line, "otim", 4) == 0) {
            debug_print("[CMD] otim - ignored\n");
        }
        else if (strncmp(line, "computer", 8) == 0) {
            debug_print("[CMD] computer - AI Mode\n");
            computerset=true;
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
            debug_print("[CMD] white\n");
            if (computerset==true) {
                 debug_print("[CMD] AI is white\n");
            }
        }
        else if (strcmp(line, "black") == 0) {
            debug_print("[CMD] black\n");
            if (computerset==true) {
                 debug_print("[CMD] AI is black and white\n");
                 GameMode=AvA;
            } else {
                 debug_print("[CMD] Human is Black, AI is White\n");
                 GameMode=BvA;
            }
        }
        else if (strcmp(line, "quit") == 0) {
            debug_print("[CMD] quit\n");
            break;
        }
        else if (strlen(line) > 0) {
            debug_print("[CMD] unknown: %s\n", line);
        }
        if ((GameMode==WvA && game.turn == BLACK) || (GameMode==BvA && game.turn == WHITE) || (GameMode==AvA)) {
            debug_print("[AI Move WvA]\n");
            ChessAIMoveResult result = chess_ai_compute_move(&game, config);
            if (result.move.from_row >= 0) {
                 chess_make_move(&game, result.move);

                debug_print("[After] move %s\n", move_to_str(result.move));

                printf("move %s\n", move_to_str(result.move));
                fflush(stdout);
                debug_print("[Turn] %s\n", (game.turn == WHITE ? "white" : "black"));
            }
            else {
                debug_print("[After] Invalid move %s\n", move_to_str(result.move));
            }
        } else {
            debug_print("Game mode %i turn %s\n", GameMode, (game.turn == WHITE ? "white" : "black"));
        }
        debug_print("Loop is finished waiting for input\n");

    }
    
    if (debug_log) {
        debug_print("[SHUTDOWN] Engine shutting down\n");
        fclose(debug_log);
    }
    return 0;
}
