/*
 * XBoard/UCI Protocol Interface for BeatChess Engine
 * 
 * This provides command-line xboard compatibility so the engine can be used
 * with chess GUIs like XBoard, WinBoard, Arena, Lichess, Chess.com, etc.
 * 
 * To use: Compile this with beatchess.cpp and use as engine in your GUI
 * 
 * Usage:
 *   xboard
 *   new
 *   setboard rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
 *   go
 *   <engine outputs move>
 *   move e2e4
 *   go
 *   ... etc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <climits>
#include "beatchess.h"

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================
 */

static ChessGameState g_game;
static bool g_xboard_mode = false;
static bool g_quit_requested = false;
static int g_depth = 4;  // Default search depth
static int g_movestogo = 0;
static int g_movetime = 0;

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

// Convert board position (row, col) to algebraic notation (e.g., "e2")
void pos_to_algebraic(int row, int col, char *buf) {
    buf[0] = 'a' + col;
    buf[1] = '8' - row;
    buf[2] = '\0';
}

// Convert move to algebraic notation (e.g., "e2e4")
void move_to_algebraic(ChessMove move, char *buf) {
    pos_to_algebraic(move.from_row, move.from_col, buf);
    pos_to_algebraic(move.to_row, move.to_col, buf + 2);
    
    // Check for pawn promotion
    ChessPiece piece = g_game.board[move.from_row][move.from_col];
    if (piece.type == PAWN) {
        if ((piece.color == WHITE && move.to_row == 0) || 
            (piece.color == BLACK && move.to_row == 7)) {
            buf[4] = 'q';  // Always promote to queen
            buf[5] = '\0';
        } else {
            buf[4] = '\0';
        }
    } else {
        buf[4] = '\0';
    }
}

// Parse algebraic notation to move (e.g., "e2e4" -> ChessMove)
bool algebraic_to_move(const char *alg, ChessMove *move) {
    if (strlen(alg) < 4) return false;
    
    int from_col = alg[0] - 'a';
    int from_row = 8 - (alg[1] - '0');
    int to_col = alg[2] - 'a';
    int to_row = 8 - (alg[3] - '0');
    
    if (from_col < 0 || from_col > 7 || from_row < 0 || from_row > 7 ||
        to_col < 0 || to_col > 7 || to_row < 0 || to_row > 7) {
        return false;
    }
    
    move->from_row = from_row;
    move->from_col = from_col;
    move->to_row = to_row;
    move->to_col = to_col;
    move->score = 0;
    
    return chess_is_valid_move(&g_game, from_row, from_col, to_row, to_col);
}

// Parse FEN string and setup board
bool parse_fen(const char *fen) {
    chess_init_board(&g_game);
    
    // Very basic FEN parser - handles piece placement only
    // Full FEN format: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    
    int row = 0, col = 0;
    const char *p = fen;
    
    // Parse piece placement
    while (*p && *p != ' ' && row < 8) {
        if (*p == '/') {
            row++;
            col = 0;
            p++;
        } else if (isdigit(*p)) {
            col += (*p - '0');
            p++;
        } else {
            // Piece character
            PieceType type = EMPTY;
            ChessColor color = NONE;
            
            switch (tolower(*p)) {
                case 'p': type = PAWN; break;
                case 'n': type = KNIGHT; break;
                case 'b': type = BISHOP; break;
                case 'r': type = ROOK; break;
                case 'q': type = QUEEN; break;
                case 'k': type = KING; break;
                default: p++; continue;
            }
            
            color = isupper(*p) ? WHITE : BLACK;
            
            if (col < 8) {
                g_game.board[row][col].type = type;
                g_game.board[row][col].color = color;
                col++;
            }
            p++;
        }
    }
    
    // Skip whitespace and parse side to move
    while (*p == ' ') p++;
    if (*p) {
        g_game.turn = (*p == 'w') ? WHITE : BLACK;
    }
    
    return true;
}

// Get best move using minimax search
ChessMove get_best_move(int depth) {
    ChessMove best_move = {0, 0, 0, 0, 0};
    ChessMove moves[256];
    int move_count = chess_get_all_moves(&g_game, g_game.turn, moves);
    
    if (move_count == 0) {
        return best_move;  // No legal moves
    }
    
    int best_score = g_game.turn == WHITE ? INT_MIN : INT_MAX;
    
    // Simple move search with evaluation
    for (int i = 0; i < move_count; i++) {
        ChessGameState temp = g_game;
        chess_make_move(&temp, moves[i]);
        
        // Use transposition table and enhanced minimax
        KillerMoveTable killers = {0};
        int score = chess_minimax_enhanced(&temp, depth - 1, depth, 
                                          -1000000, 1000000, 
                                          g_game.turn == WHITE ? false : true,
                                          &killers);
        
        if (g_game.turn == WHITE) {
            if (score > best_score) {
                best_score = score;
                best_move = moves[i];
            }
        } else {
            if (score < best_score) {
                best_score = score;
                best_move = moves[i];
            }
        }
    }
    
    return best_move;
}

/* ============================================================================
 * XBOARD PROTOCOL COMMANDS
 * ============================================================================
 */

void handle_xboard() {
    g_xboard_mode = true;
    printf("feature myname=\"BeatChess\" variants=\"normal\" sigint=false\n");
    printf("feature done=1\n");
    fflush(stdout);
}

void handle_new() {
    chess_clear_transposition_table();
    chess_init_zobrist();
    chess_init_board(&g_game);
}

void handle_setboard(const char *fen) {
    parse_fen(fen);
}

void handle_go() {
    ChessMove best_move = get_best_move(g_depth);
    
    if (best_move.from_row == 0 && best_move.from_col == 0 && 
        best_move.to_row == 0 && best_move.to_col == 0) {
        // Check if this is a real move or no legal moves
        ChessMove test_moves[256];
        int count = chess_get_all_moves(&g_game, g_game.turn, test_moves);
        if (count == 0) {
            printf("0-1\n");  // Stalemate/checkmate
        }
    } else {
        char move_str[6];
        move_to_algebraic(best_move, move_str);
        printf("move %s\n", move_str);
        fflush(stdout);
        
        // Make the move
        chess_make_move(&g_game, best_move);
    }
}

void handle_move(const char *move_str) {
    ChessMove move;
    if (algebraic_to_move(move_str, &move)) {
        // Validate move doesn't leave king in check
        ChessGameState temp = g_game;
        chess_make_move(&temp, move);
        if (!chess_is_in_check(&temp, g_game.turn)) {
            chess_make_move(&g_game, move);
        } else {
            printf("Illegal move: %s\n", move_str);
            fflush(stdout);
        }
    } else {
        printf("Illegal move: %s\n", move_str);
        fflush(stdout);
    }
}

void handle_level(const char *args) {
    // Parse "level <moves> <time> <inc>" (time in minutes)
    // For simplicity, we'll just extract depth
    // Example: "level 40 5 0" = 40 moves in 5 minutes
    int moves, time_min, inc;
    if (sscanf(args, "%d %d %d", &moves, &time_min, &inc) == 3) {
        // Rough heuristic: allocate time equally across moves
        // For now, just use a reasonable depth
        g_depth = 4;
        g_movestogo = moves;
    }
}

void handle_time(const char *arg) {
    // Time in centiseconds
    sscanf(arg, "%d", &g_movetime);
}

void handle_depth(const char *arg) {
    sscanf(arg, "%d", &g_depth);
    if (g_depth < 1) g_depth = 1;
    if (g_depth > 12) g_depth = 12;
}

void handle_quit() {
    g_quit_requested = true;
}

/* ============================================================================
 * MAIN XBOARD LOOP
 * ============================================================================
 */

void xboard_loop() {
    char line[256];
    char *token, *rest;
    
    // Initialize
    chess_init_zobrist();
    chess_clear_transposition_table();
    chess_init_board(&g_game);
    
    while (!g_quit_requested && fgets(line, sizeof(line), stdin)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        // Tokenize command
        rest = line;
        token = strsep(&rest, " ");
        
        if (!token) continue;
        
        // Handle commands
        if (strcmp(token, "xboard") == 0) {
            handle_xboard();
        } 
        else if (strcmp(token, "new") == 0) {
            handle_new();
        }
        else if (strcmp(token, "setboard") == 0) {
            // Rest of line is FEN
            if (rest) {
                handle_setboard(rest);
            }
        }
        else if (strcmp(token, "go") == 0) {
            handle_go();
        }
        else if (strcmp(token, "move") == 0) {
            if (rest) {
                handle_move(rest);
            }
        }
        else if (strcmp(token, "level") == 0) {
            if (rest) {
                handle_level(rest);
            }
        }
        else if (strcmp(token, "time") == 0) {
            if (rest) {
                handle_time(rest);
            }
        }
        else if (strcmp(token, "depth") == 0) {
            if (rest) {
                handle_depth(rest);
            }
        }
        else if (strcmp(token, "quit") == 0) {
            handle_quit();
        }
        else if (strcmp(token, "result") == 0) {
            // Game result notification
            handle_quit();
        }
        else if (strcmp(token, "hint") == 0) {
            // Suggest a move
            ChessMove hint = get_best_move(g_depth);
            char move_str[6];
            move_to_algebraic(hint, move_str);
            printf("Hint: %s\n", move_str);
            fflush(stdout);
        }
        else if (strcmp(token, "accepted") == 0 || strcmp(token, "rejected") == 0) {
            // Feature negotiations - ignore
        }
        // Ignore unknown commands
    }
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================
 */

int main(int argc, char *argv[]) {
    // Check if running in xboard mode
    // Typically invoked with "xboard" as first command from GUI
    
    // Initialize
    chess_init_zobrist();
    chess_clear_transposition_table();
    
    // Run xboard protocol loop
    xboard_loop();
    
    return 0;
}
