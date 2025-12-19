/*
 * BeatChess Save/Load Module - Binary Format
 * Saves/loads move_history array directly
 */

#include <stdio.h>
#include <string.h>
#include "beatchess.h"

bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    
    // Write move count
    fwrite(&chess->move_history_count, sizeof(int), 1, f);
    
    // Write entire move_history array
    fwrite(chess->move_history, sizeof(MoveHistory), chess->move_history_count, f);
    
    fclose(f);
    printf("Game saved to: %s\n", filename);
    return true;
}

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
