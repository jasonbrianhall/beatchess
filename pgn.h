/*
 * pgn.h - PGN Save/Load Header for BeatChess
 */

#ifndef PGN_H
#define PGN_H

#include "beatchess.h"
#include <stdbool.h>

/* ============================================================================
 * CORE PGN FUNCTIONS
 * ============================================================================
 */

/**
 * Export a game to PGN format
 * 
 * Parameters:
 *   chess: Pointer to BeatChessVisualization structure with game state
 *   filename: Path to output .pgn file
 *   white_name: Name of white player (can be NULL for default)
 *   black_name: Name of black player (can be NULL for default)
 * 
 * Returns: true if successful, false otherwise
 * 
 * Example:
 *   pgn_export_game(chess, "mygame.pgn", "Magnus", "Fabiano");
 */
bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name);

/**
 * Import a game from PGN format
 * 
 * Parameters:
 *   chess: Pointer to BeatChessVisualization structure (will be reset)
 *   filename: Path to input .pgn file
 * 
 * Returns: true if successful, false on error
 * 
 * Example:
 *   pgn_import_game(chess, "mygame.pgn");
 */
bool pgn_import_game(BeatChessVisualization *chess, const char *filename);

/**
 * Convert a move to algebraic notation string
 * 
 * Parameters:
 *   game: Current game state (for context)
 *   move: The move to convert
 *   notation: Output string (should be at least 20 chars)
 * 
 * Example output: "e4", "Nf3", "Qh5#", "O-O", "exd5"
 */
void move_to_algebraic(ChessGameState *game, ChessMove move, char *notation);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * List all PGN files in a directory
 * 
 * Parameters:
 *   directory: Directory to search
 *   filenames: Array of char* to store results (caller must free)
 *   max_files: Maximum number of files to return
 * 
 * Returns: Number of files found
 * 
 * Note: Caller is responsible for freeing the allocated strings
 */
int pgn_list_files(const char *directory, char **filenames, int max_files);

/**
 * Get default save directory for PGN files
 * 
 * Parameters:
 *   path: Buffer to write path to
 *   path_size: Size of path buffer
 * 
 * Returns: Platform-specific default directory
 *   - Windows: %USERPROFILE%\Documents\BeatChess
 *   - Linux/Mac: ~/.beatchess
 *   - DOS: Current directory
 */
void pgn_get_default_directory(char *path, size_t path_size);

/**
 * Ensure directory exists (create if necessary)
 * 
 * Parameters:
 *   directory: Directory path
 * 
 * Returns: true if directory exists or was created successfully
 */
bool pgn_ensure_directory(const char *directory);

#endif // PGN_H
