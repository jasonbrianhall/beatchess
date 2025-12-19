/*
 * BeatChess Save/Load Module
 * Saves as binary move_history, loads both binary and PGN format
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "beatchess.h"

/* ============================================================================
 * Helper: Convert move to algebraic notation using move_history context
 * ============================================================================
 */

static void move_to_algebraic(ChessGameState *before_state, ChessMove move, char *notation) {
    // Get the piece that moved from the BEFORE state
    ChessPiece piece = before_state->board[move.from_row][move.from_col];
    
    char result[64] = {0};
    int idx = 0;
    
    // Check for castling
    if (piece.type == KING && move.from_col == 4) {
        if (move.to_col == 6) {
            strcpy(notation, "O-O");
            return;
        } else if (move.to_col == 2) {
            strcpy(notation, "O-O-O");
            return;
        }
    }
    
    // Check for capture
    bool is_capture = before_state->board[move.to_row][move.to_col].type != EMPTY;
    
    // Add piece character (not for pawns, unless capture)
    if (piece.type == PAWN) {
        if (is_capture) {
            result[idx++] = 'a' + move.from_col;
        }
    } else {
        // Non-pawn pieces: R, B, N, Q, K
        switch (piece.type) {
            case ROOK:   result[idx++] = 'R'; break;
            case KNIGHT: result[idx++] = 'N'; break;
            case BISHOP: result[idx++] = 'B'; break;
            case QUEEN:  result[idx++] = 'Q'; break;
            case KING:   result[idx++] = 'K'; break;
            default: break;
        }
    }
    
    // Add capture notation
    if (is_capture) {
        result[idx++] = 'x';
    }
    
    // Add destination square
    result[idx++] = 'a' + move.to_col;
    result[idx++] = '8' - move.to_row;
    
    // Add promotion notation if applicable
    if (piece.type == PAWN && (move.to_row == 0 || move.to_row == 7)) {
        result[idx++] = '=';
        result[idx++] = 'Q';
    }
    
    result[idx] = '\0';
    strcpy(notation, result);
}

/* ============================================================================
 * EXPORT: Save game as PGN with move_history, also save binary backup
 * ============================================================================
 */

bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name) {
    // Save binary backup first
    char backup_filename[256];
    snprintf(backup_filename, sizeof(backup_filename), "%s.bin", filename);
    
    FILE *binf = fopen(backup_filename, "wb");
    if (binf) {
        fwrite(&chess->move_history_count, sizeof(int), 1, binf);
        fwrite(chess->move_history, sizeof(MoveHistory), chess->move_history_count, binf);
        fclose(binf);
    }
    
    // Save PGN file
    FILE *f = fopen(filename, "w");
    if (!f) return false;
    
    // Get current date
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    
    // Write PGN headers
    fprintf(f, "[Event \"BeatChess Game\"]\n");
    fprintf(f, "[Site \"BeatChess\"]\n");
    fprintf(f, "[Date \"%04d.%02d.%02d\"]\n",
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday);
    fprintf(f, "[Round \"1\"]\n");
    fprintf(f, "[White \"%s\"]\n", white_name ? white_name : "Human");
    fprintf(f, "[Black \"%s\"]\n", black_name ? black_name : "Computer");
    fprintf(f, "[Result \"*\"]\n");
    fprintf(f, "\n");
    
    // Write moves in algebraic notation from move_history
    int move_number = 1;
    int chars_on_line = 0;
    
    for (int i = 0; i < chess->move_history_count; i++) {
        MoveHistory *mh = &chess->move_history[i];
        
        // Convert move to algebraic notation using the BEFORE state
        char notation[64];
        move_to_algebraic(&mh->game_state, mh->move, notation);
        
        // Write move number for white's moves
        char move_text[80];
        if (i % 2 == 0) {
            sprintf(move_text, "%d. %s ", move_number, notation);
        } else {
            sprintf(move_text, "%s ", notation);
        }
        
        // Check line length
        if (chars_on_line + strlen(move_text) > 80) {
            fprintf(f, "\n");
            chars_on_line = 0;
        }
        
        fprintf(f, "%s", move_text);
        chars_on_line += strlen(move_text);
        
        // Increment move number after black's move
        if (i % 2 == 1) {
            move_number++;
        }
    }
    
    fprintf(f, "*\n");
    fclose(f);
    
    printf("Game saved to: %s\n", filename);
    return true;
}

/* ============================================================================
 * IMPORT: Load binary game directly
 * ============================================================================
 */

bool pgn_import_game(BeatChessVisualization *chess, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    // Read move count
    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    if (count < 0 || count > MAX_MOVE_HISTORY) {
        fclose(f);
        return false;
    }
    
    // Read move_history array
    if (fread(chess->move_history, sizeof(MoveHistory), count, f) != (size_t)count) {
        fclose(f);
        return false;
    }
    
    chess->move_history_count = count;
    
    // Restore board to final position
    if (count > 0) {
        chess->game = chess->move_history[count - 1].game_state;
    }
    
    fclose(f);
    printf("Game loaded from: %s (%d moves)\n", filename, count);
    return true;
}
EOF
cat /mnt/user-data/outputs/pgn.c
Output

/*
 * BeatChess Save/Load Module
 * Saves as binary move_history, loads both binary and PGN format
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "beatchess.h"

/* ============================================================================
 * Helper: Convert move to algebraic notation using move_history context
 * ============================================================================
 */

static void move_to_algebraic(ChessGameState *before_state, ChessMove move, char *notation) {
    // Get the piece that moved from the BEFORE state
    ChessPiece piece = before_state->board[move.from_row][move.from_col];
    
    char result[64] = {0};
    int idx = 0;
    
    // Check for castling
    if (piece.type == KING && move.from_col == 4) {
        if (move.to_col == 6) {
            strcpy(notation, "O-O");
            return;
        } else if (move.to_col == 2) {
            strcpy(notation, "O-O-O");
            return;
        }
    }
    
    // Check for capture
    bool is_capture = before_state->board[move.to_row][move.to_col].type != EMPTY;
    
    // Add piece character (not for pawns, unless capture)
    if (piece.type == PAWN) {
        if (is_capture) {
            result[idx++] = 'a' + move.from_col;
        }
    } else {
        // Non-pawn pieces: R, B, N, Q, K
        switch (piece.type) {
            case ROOK:   result[idx++] = 'R'; break;
            case KNIGHT: result[idx++] = 'N'; break;
            case BISHOP: result[idx++] = 'B'; break;
            case QUEEN:  result[idx++] = 'Q'; break;
            case KING:   result[idx++] = 'K'; break;
            default: break;
        }
    }
    
    // Add capture notation
    if (is_capture) {
        result[idx++] = 'x';
    }
    
    // Add destination square
    result[idx++] = 'a' + move.to_col;
    result[idx++] = '8' - move.to_row;
    
    // Add promotion notation if applicable
    if (piece.type == PAWN && (move.to_row == 0 || move.to_row == 7)) {
        result[idx++] = '=';
        result[idx++] = 'Q';
    }
    
    result[idx] = '\0';
    strcpy(notation, result);
}

/* ============================================================================
 * EXPORT: Save game as PGN with move_history, also save binary backup
 * ============================================================================
 */

bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name) {
    // Save binary backup first
    char backup_filename[256];
    snprintf(backup_filename, sizeof(backup_filename), "%s.bin", filename);
    
    FILE *binf = fopen(backup_filename, "wb");
    if (binf) {
        fwrite(&chess->move_history_count, sizeof(int), 1, binf);
        fwrite(chess->move_history, sizeof(MoveHistory), chess->move_history_count, binf);
        fclose(binf);
    }
    
    // Save PGN file
    FILE *f = fopen(filename, "w");
    if (!f) return false;
    
    // Get current date
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    
    // Write PGN headers
    fprintf(f, "[Event \"BeatChess Game\"]\n");
    fprintf(f, "[Site \"BeatChess\"]\n");
    fprintf(f, "[Date \"%04d.%02d.%02d\"]\n",
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday);
    fprintf(f, "[Round \"1\"]\n");
    fprintf(f, "[White \"%s\"]\n", white_name ? white_name : "Human");
    fprintf(f, "[Black \"%s\"]\n", black_name ? black_name : "Computer");
    fprintf(f, "[Result \"*\"]\n");
    fprintf(f, "\n");
    
    // Write moves in algebraic notation from move_history
    int move_number = 1;
    int chars_on_line = 0;
    
    for (int i = 0; i < chess->move_history_count; i++) {
        MoveHistory *mh = &chess->move_history[i];
        
        // Convert move to algebraic notation using the BEFORE state
        char notation[64];
        move_to_algebraic(&mh->game_state, mh->move, notation);
        
        // Write move number for white's moves
        char move_text[80];
        if (i % 2 == 0) {
            sprintf(move_text, "%d. %s ", move_number, notation);
        } else {
            sprintf(move_text, "%s ", notation);
        }
        
        // Check line length
        if (chars_on_line + strlen(move_text) > 80) {
            fprintf(f, "\n");
            chars_on_line = 0;
        }
        
        fprintf(f, "%s", move_text);
        chars_on_line += strlen(move_text);
        
        // Increment move number after black's move
        if (i % 2 == 1) {
            move_number++;
        }
    }
    
    fprintf(f, "*\n");
    fclose(f);
    
    printf("Game saved to: %s\n", filename);
    return true;
}

/* ============================================================================
 * IMPORT: Load binary game directly
 * ============================================================================
 */

bool pgn_import_game(BeatChessVisualization *chess, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    // Read move count
    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    if (count < 0 || count > MAX_MOVE_HISTORY) {
        fclose(f);
        return false;
    }
    
    // Read move_history array
    if (fread(chess->move_history, sizeof(MoveHistory), count, f) != (size_t)count) {
        fclose(f);
        return false;
    }
    
    chess->move_history_count = count;
    
    // Restore board to final position
    if (count > 0) {
        chess->game = chess->move_history[count - 1].game_state;
    }
    
    fclose(f);
    printf("Game loaded from: %s (%d moves)\n", filename, count);
    return true;
}
