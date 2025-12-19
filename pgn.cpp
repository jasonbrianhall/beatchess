/*
 * BeatChess Save/Load Module
 * Binary format only
 */

#include <stdio.h>
#include <string.h>
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
