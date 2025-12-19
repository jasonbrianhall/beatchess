/*
 * BeatChess Save/Load Module
 * Binary format only
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "beatchess.h"

bool ensure_bin_extension(char *filename, size_t bufsize) {
    const char *ext = ".sav";
    size_t len_f = strlen(filename);
    size_t len_e = strlen(ext);

    // Case-insensitive check: .sav, .sav, .sav, etc.
    if (len_f >= len_e &&
        strcasecmp(filename + (len_f - len_e), ext) == 0)
        return true;

    // Not enough room
    if (len_f + len_e + 1 > bufsize)
        return false;

    // Append .sav
    memcpy(filename + len_f, ext, len_e + 1);
    return true;
}

bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name)
{
    if (!filename || !chess) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return false;
    }

    // Copy filename into a mutable buffer
    char base_filename[256];
    strncpy(base_filename, filename, sizeof(base_filename) - 1);
    base_filename[sizeof(base_filename) - 1] = '\0';

    // Strip .sav (case-insensitive)
    size_t len = strlen(base_filename);
    const char *ext = ".sav";
    size_t ext_len = strlen(ext);

    if (len >= ext_len &&
        strcasecmp(base_filename + (len - ext_len), ext) == 0)
    {
        base_filename[len - ext_len] = '\0';
    }

    // Build final .sav filename
    char bin_filename[256];
    if (snprintf(bin_filename, sizeof(bin_filename), "%s.sav", base_filename)
        >= sizeof(bin_filename))
    {
        fprintf(stderr, "Error: filename too long\n");
        return false;
    }

    FILE *f = fopen(bin_filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s for writing\n", bin_filename);
        return false;
    }

    // Write move count
    fwrite(&chess->move_history_count, sizeof(int), 1, f);

    // Write move history
    fwrite(chess->move_history, sizeof(MoveHistory),
           chess->move_history_count, f);

    fclose(f);
    printf("Game saved to: %s\n", bin_filename);
    return true;
}


bool pgn_import_game(BeatChessVisualization *chess, const char *filename) {
    char bin_filename[256];
    strncpy(bin_filename, filename, sizeof(bin_filename));
    bin_filename[sizeof(bin_filename) - 1] = '\0';

    if (!ensure_bin_extension(bin_filename, sizeof(bin_filename))) {
        fprintf(stderr, "Error: filename too long to append .sav\n");
        return false;
    }

    FILE *f = fopen(bin_filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s\n", bin_filename);
        return false;
    }

    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1) {
        fprintf(stderr, "Error: Could not read move count\n");
        fclose(f);
        return false;
    }

    if (count < 0 || count > MAX_MOVE_HISTORY) {
        fprintf(stderr, "Error: Invalid move count: %d\n", count);
        fclose(f);
        return false;
    }

    if (fread(chess->move_history, sizeof(MoveHistory), count, f) != (size_t)count) {
        fprintf(stderr, "Error: Could not read move history\n");
        fclose(f);
        return false;
    }

    chess->move_history_count = count;

    if (count > 0) {
        chess->game = chess->move_history[count - 1].game_state;
    }

    fclose(f);
    printf("Game loaded from: %s (%d moves)\n", bin_filename, count);
    return true;
}

/**
 * Print chess board using UTF-8 piece symbols
 */
void print_board_utf8(ChessGameState *game) {
    const char *white_pieces[] = {" ", "♟", "♞", "♝", "♜", "♛", "♚"};
    const char *black_pieces[] = {" ", "♙", "♘", "♗", "♖", "♕", "♔"};
    
    printf("  a b c d e f g h\n");
    for (int row = 0; row < 8; row++) {
        printf("%d ", row + 1);
        for (int col = 0; col < 8; col++) {
            ChessPiece piece = game->board[row][col];
            if (piece.type == EMPTY) {
                printf(". ");
            } else if (piece.color == WHITE) {
                printf("%s ", white_pieces[piece.type]);
            } else {
                printf("%s ", black_pieces[piece.type]);
            }
        }
        printf("%d\n", row + 1);
    }
    printf("  a b c d e f g h\n");
}

/**
 * Convert a move to algebraic notation
 * Handles regular moves, captures, castling
 */
void move_to_algebraic(ChessGameState *game, ChessMove move, char *notation) {
    if (!game || !notation) {
        strcpy(notation, "???");
        return;
    }
    
    ChessPiece piece = game->board[move.from_row][move.from_col];
    
    printf("\n--- BOARD STATE ---\n");
    print_board_utf8(game);
    printf("Move: [%d,%d] -> [%d,%d]\n", move.from_row, move.from_col, move.to_row, move.to_col);
    
    // Get piece character based on actual enum value
    char piece_char;
    switch (piece.type) {
        case PAWN:   piece_char = '\0'; break;  // pawns have no letter
        case KNIGHT: piece_char = 'N'; break;
        case BISHOP: piece_char = 'B'; break;
        case ROOK:   piece_char = 'R'; break;
        case QUEEN:  piece_char = 'Q'; break;
        case KING:   piece_char = 'K'; break;
        default:     piece_char = '?'; break;
    }
    
    // Check for castling
    if (piece.type == KING && abs(move.to_col - move.from_col) == 2) {
        if (move.to_col > move.from_col) {
            strcpy(notation, "O-O");
        } else {
            strcpy(notation, "O-O-O");
        }
        return;
    }
    
    char to_file = 'a' + move.to_col;
    char to_rank = '1' + move.to_row;
    
    bool is_capture = game->board[move.to_row][move.to_col].type != EMPTY;
    
    if (piece.type == PAWN) {
        if (is_capture) {
            char from_file = 'a' + move.from_col;
            snprintf(notation, 20, "%cx%c%c", from_file, to_file, to_rank);
        } else {
            snprintf(notation, 20, "%c%c", to_file, to_rank);
        }
    } else if (piece_char != '?') {
        if (is_capture) {
            snprintf(notation, 20, "%cx%c%c", piece_char, to_file, to_rank);
        } else {
            snprintf(notation, 20, "%c%c%c", piece_char, to_file, to_rank);
        }
    } else {
        snprintf(notation, 20, "%c%c", to_file, to_rank);
    }
    printf("Notation: %s\n\n", notation);
}
