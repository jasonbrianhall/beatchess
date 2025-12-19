/*
 * BeatChess Save/Load Module
 * Binary format only
 */

#include <stdio.h>
#include <string.h>
#include "beatchess.h"

bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name) {
    if (!filename || !chess) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return false;
    }
    
    // Remove .bin extension if present in filename
    char base_filename[256];
    strncpy(base_filename, filename, sizeof(base_filename) - 1);
    base_filename[sizeof(base_filename) - 1] = '\0';
    
    // Strip .bin if it's there
    if (strlen(base_filename) > 4) {
        if (strcmp(base_filename + strlen(base_filename) - 4, ".bin") == 0) {
            base_filename[strlen(base_filename) - 4] = '\0';
        }
    }
    
    // Save binary
    char bin_filename[256];
    snprintf(bin_filename, sizeof(bin_filename), "%s.bin", base_filename);
    
    FILE *f = fopen(bin_filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s for writing\n", bin_filename);
        return false;
    }
    
    // Write move count
    fwrite(&chess->move_history_count, sizeof(int), 1, f);
    
    // Write entire move_history array
    fwrite(chess->move_history, sizeof(MoveHistory), chess->move_history_count, f);
    
    fclose(f);
    printf("Game saved to: %s\n", bin_filename);
    return true;
}

bool pgn_import_game(BeatChessVisualization *chess, const char *filename) {
    char bin_filename[256];
    snprintf(bin_filename, sizeof(bin_filename), "%s.bin", filename);
    FILE *f = fopen(bin_filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s\n", filename);
        return false;
    }
    
    // Read move count
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
    
    // Read move_history array
    if (fread(chess->move_history, sizeof(MoveHistory), count, f) != (size_t)count) {
        fprintf(stderr, "Error: Could not read move history\n");
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
