/*
 * SAV to PGN Converter
 * Converts BeatChess .sav files to PGN format
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "beatchess.h"

/* Forward declarations */
bool pgn_import_game(BeatChessVisualization *chess, const char *filename);
void move_to_algebraic(ChessGameState *game, ChessMove move, char *notation);

/**
 * Export game to PGN format from a BeatChessVisualization struct
 */
bool export_to_pgn(BeatChessVisualization *chess, const char *output_filename) {
    if (!chess || !output_filename) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return false;
    }
    
    FILE *f = fopen(output_filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s for writing\n", output_filename);
        return false;
    }
    
    // Write PGN headers
    fprintf(f, "[Event \"?\"]\n");
    fprintf(f, "[Site \"?\"]\n");
    fprintf(f, "[Date \"????.??.??\"]\n");
    fprintf(f, "[Round \"?\"]\n");
    fprintf(f, "[White \"?\"]\n");
    fprintf(f, "[Black \"?\"]\n");
    fprintf(f, "[Result \"*\"]\n");
    fprintf(f, "\n");
    
    // Reconstruct game from move history and export moves
    ChessGameState game_state;
    chess_init_board(&game_state);
    
    int move_num = 1;
    int moves_written_this_line = 0;
    
    for (int i = 0; i < chess->move_history_count; i++) {
        MoveHistory hist = chess->move_history[i];
        ChessMove move = hist.move;
        
        // Write move number at start of white's move
        if (game_state.turn == WHITE) {
            if (moves_written_this_line >= 4) {
                fprintf(f, "\n");
                moves_written_this_line = 0;
            }
            fprintf(f, "%d. ", move_num);
        }
        
        // Convert move to algebraic notation
        char notation[20] = {0};
        move_to_algebraic(&game_state, move, notation);
        fprintf(f, "%s ", notation);
        moves_written_this_line++;
        
        // Execute the move on our reconstruction
        chess_make_move(&game_state, move);
        
        // Increment move number after black's move
        if (game_state.turn == WHITE) {
            move_num++;
        }
    }
    
    fprintf(f, "\n*\n");
    fclose(f);
    
    printf("Game exported to: %s\n", output_filename);
    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.sav> [output.pgn]\n", argv[0]);
        fprintf(stderr, "  If output.pgn is not specified, input_converted.pgn will be used\n");
        return 1;
    }
    
    const char *input_file = argv[1];
    const char *output_file = argc > 2 ? argv[2] : NULL;
    
    // Generate output filename if not provided
    char generated_output[256] = {0};
    if (!output_file) {
        strncpy(generated_output, input_file, sizeof(generated_output) - 1);
        
        // Strip .sav extension if present
        size_t len = strlen(generated_output);
        if (len >= 4 && strcasecmp(generated_output + len - 4, ".sav") == 0) {
            generated_output[len - 4] = '\0';
        }
        
        // Append .pgn
        strncat(generated_output, ".pgn", sizeof(generated_output) - strlen(generated_output) - 1);
        output_file = generated_output;
    }
    
    printf("Converting %s -> %s\n", input_file, output_file);
    
    // Initialize chess board
    BeatChessVisualization chess = {};
    chess_init_board(&chess.game);
    
    // Load the game from .sav file
    if (!pgn_import_game(&chess, input_file)) {
        fprintf(stderr, "Error: Failed to load game from %s\n", input_file);
        return 1;
    }
    
    // Export to PGN
    if (!export_to_pgn(&chess, output_file)) {
        fprintf(stderr, "Error: Failed to export to PGN\n");
        return 1;
    }
    
    printf("Success! Converted %d moves\n", chess.move_history_count);
    return 0;
}
