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
bool export_to_pgn(BeatChessVisualization *chess, const char *output_filename);


/**
 * Export game to PGN format from a BeatChessVisualization struct
 */


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
