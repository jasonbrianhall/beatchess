/*
 * pgn.c - PGN Save/Load Implementation for BeatChess
 * Handles saving and loading games in Portable Game Notation format
 */

#include "beatchess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

/* Platform-specific includes */
#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <unistd.h>
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */

#define PGN_BUFFER_SIZE 65536
#define PGN_LINE_SIZE 256
#define MAX_NOTATION_LENGTH 20

/* ============================================================================
 * HELPER: Convert 0-7 row/col to algebraic notation (a-h, 1-8)
 * ============================================================================
 */

static void square_to_algebraic(int row, int col, char *notation) {
    notation[0] = 'a' + col;
    notation[1] = '8' - row;  // Row 0 = rank 8, row 7 = rank 1
    notation[2] = '\0';
}

static void algebraic_to_square(const char *notation, int *row, int *col) {
    *col = notation[0] - 'a';
    *row = '8' - notation[1];
}

/* ============================================================================
 * Get piece character for algebraic notation
 * ============================================================================
 */

static char piece_to_char(PieceType type) {
    switch (type) {
        case PAWN:   return ' ';   // Pawns have no letter
        case KNIGHT: return 'N';
        case BISHOP: return 'B';
        case ROOK:   return 'R';
        case QUEEN:  return 'Q';
        case KING:   return 'K';
        default:     return '?';
    }
}

static PieceType char_to_piece(char c) {
    switch (c) {
        case 'N': return KNIGHT;
        case 'B': return BISHOP;
        case 'R': return ROOK;
        case 'Q': return QUEEN;
        case 'K': return KING;
        case 'P': return PAWN;
        default:  return EMPTY;
    }
}

/* ============================================================================
 * Convert move to algebraic notation
 * ============================================================================
 */

void move_to_algebraic(ChessGameState *game, ChessMove move, char *notation) {
    char result[MAX_NOTATION_LENGTH] = {0};
    int idx = 0;
    
    ChessPiece piece = game->board[move.from_row][move.from_col];
    
    // Check for castling
    if (piece.type == KING && abs(move.to_col - move.from_col) == 2) {
        if (move.to_col > move.from_col) {
            strcpy(notation, "O-O");  // Kingside castling
        } else {
            strcpy(notation, "O-O-O");  // Queenside castling
        }
        return;
    }
    
    // Add piece character (empty for pawns)
    char piece_char = piece_to_char(piece.type);
    if (piece_char != ' ') {
        result[idx++] = piece_char;
    }
    
    // Check for capture
    bool is_capture = game->board[move.to_row][move.to_col].type != EMPTY;
    
    // For pawn captures, include file letter
    if (piece.type == PAWN && is_capture) {
        result[idx++] = 'a' + move.from_col;
    }
    
    // Add capture notation
    if (is_capture) {
        result[idx++] = 'x';
    }
    
    // Add destination square
    result[idx++] = 'a' + move.to_col;
    result[idx++] = '8' - move.to_row;
    
    // Check for pawn promotion
    if (piece.type == PAWN && (move.to_row == 0 || move.to_row == 7)) {
        result[idx++] = '=';
        result[idx++] = 'Q';  // Assume queen (most common)
    }
    
    // Make a copy of the game and execute the move to check for check/mate
    ChessGameState test_game = *game;
    chess_make_move(&test_game, move);
    
    // Check if resulting position is checkmate or check
    bool is_check = chess_is_in_check(&test_game, test_game.turn);
    
    if (is_check) {
        // Check if it's actually checkmate
        ChessMove dummy_moves[256];
        int num_moves = chess_get_all_moves(&test_game, test_game.turn, dummy_moves);
        if (num_moves == 0) {
            result[idx++] = '#';  // Checkmate
        } else {
            result[idx++] = '+';  // Check
        }
    }
    
    result[idx] = '\0';
    strcpy(notation, result);
}

/* ============================================================================
 * EXPORT: Save game to PGN file
 * ============================================================================
 */

bool pgn_export_game(BeatChessVisualization *chess, const char *filename,
                     const char *white_name, const char *black_name) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
        return false;
    }
    
    // Get current date
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    
    // Determine game result
    const char *result_str = "*";  // Default: ongoing
    
    switch (chess->status) {
        case CHESS_CHECKMATE_WHITE:
            result_str = "1-0";  // White won
            break;
        case CHESS_CHECKMATE_BLACK:
            result_str = "0-1";  // Black won
            break;
        case CHESS_STALEMATE:
            result_str = "1/2-1/2";  // Draw
            break;
        case CHESS_PLAYING:
        default:
            result_str = "*";  // Game in progress
            break;
    }
    
    // Write PGN headers
    fprintf(f, "[Event \"BeatChess Game\"]\n");
    fprintf(f, "[Site \"BeatChess\"]\n");
    fprintf(f, "[Date \"%04d.%02d.%02d\"]\n",
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday);
    fprintf(f, "[Round \"1\"]\n");
    fprintf(f, "[White \"%s\"]\n", white_name ? white_name : "Player 1");
    fprintf(f, "[Black \"%s\"]\n", black_name ? black_name : "Player 2");
    fprintf(f, "[Result \"%s\"]\n", result_str);
    fprintf(f, "[TimeControl \"-\"]\n");
    fprintf(f, "\n");  // Blank line before moves
    
    // Write moves in algebraic notation
    int move_number = 1;
    int chars_on_line = 0;
    const int MAX_CHARS_PER_LINE = 80;
    
    for (int i = 0; i < chess->move_history_count; i++) {
        MoveHistory *mh = &chess->move_history[i];
        
        // Convert move to algebraic notation
        char notation[MAX_NOTATION_LENGTH];
        move_to_algebraic(&mh->game_state, mh->move, notation);
        
        // Print move number at start of white's moves
        char move_text[64];
        if (i % 2 == 0) {
            sprintf(move_text, "%d. %s ", move_number, notation);
        } else {
            sprintf(move_text, "%s ", notation);
        }
        
        // Check if we need a line break
        if (chars_on_line + strlen(move_text) > MAX_CHARS_PER_LINE) {
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
    
    // Write final result
    fprintf(f, "%s\n", result_str);
    
    fclose(f);
    printf("Game saved to: %s\n", filename);
    return true;
}

/* ============================================================================
 * Helper: Parse algebraic move and find matching legal move
 * ============================================================================
 */

static bool parse_algebraic_move(ChessGameState *game, const char *notation, ChessMove *out_move) {
    // Handle castling
    if (strcmp(notation, "O-O") == 0 || strcmp(notation, "0-0") == 0) {
        out_move->from_row = game->turn == WHITE ? 7 : 0;
        out_move->from_col = 4;
        out_move->to_row = game->turn == WHITE ? 7 : 0;
        out_move->to_col = 6;
        return chess_is_valid_move(game, out_move->from_row, out_move->from_col,
                                   out_move->to_row, out_move->to_col);
    }
    
    if (strcmp(notation, "O-O-O") == 0 || strcmp(notation, "0-0-0") == 0) {
        out_move->from_row = game->turn == WHITE ? 7 : 0;
        out_move->from_col = 4;
        out_move->to_row = game->turn == WHITE ? 7 : 0;
        out_move->to_col = 2;
        return chess_is_valid_move(game, out_move->from_row, out_move->from_col,
                                   out_move->to_row, out_move->to_col);
    }
    
    // Parse piece type and destination
    int idx = 0;
    PieceType piece_type = PAWN;
    char from_file = -1;  // For disambiguation
    int from_rank = -1;
    char dest_file, dest_rank;
    
    // Check for piece character (N, B, R, Q, K)
    if (idx < (int)strlen(notation) && isupper(notation[idx]) && notation[idx] != 'O') {
        piece_type = char_to_piece(notation[idx]);
        idx++;
    }
    
    // Check for disambiguation (file and/or rank before destination)
    // This is complex, skip for now - just handle basic moves
    
    // Find destination square (last two chars before promotion/check/mate)
    int notation_len = strlen(notation);
    
    // Remove trailing +, #, = and promotion piece
    while (notation_len > 0 && (notation[notation_len-1] == '+' || 
           notation[notation_len-1] == '#' || isalpha(notation[notation_len-1]))) {
        notation_len--;
    }
    
    // Find the two-char destination square
    while (notation_len >= 2) {
        if (notation[notation_len-2] >= 'a' && notation[notation_len-2] <= 'h' &&
            notation[notation_len-1] >= '1' && notation[notation_len-1] <= '8') {
            dest_file = notation[notation_len-2];
            dest_rank = notation[notation_len-1];
            break;
        }
        notation_len--;
    }
    
    if (notation_len < 2) {
        return false;  // Invalid notation
    }
    
    int dest_col = dest_file - 'a';
    int dest_row = '8' - dest_rank;
    
    // Find the piece that can move to this destination
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            ChessPiece p = game->board[r][c];
            
            // Check piece type and color
            if (p.type != piece_type || p.color != game->turn) continue;
            
            // Check disambiguation if provided
            if (from_file >= 0 && c != (from_file - 'a')) continue;
            if (from_rank >= 0 && r != ('8' - from_rank)) continue;
            
            // Check if this piece can legally move to destination
            if (chess_is_valid_move(game, r, c, dest_row, dest_col)) {
                out_move->from_row = r;
                out_move->from_col = c;
                out_move->to_row = dest_row;
                out_move->to_col = dest_col;
                return true;
            }
        }
    }
    
    return false;  // No legal move found
}

/* ============================================================================
 * Helper: Extract tokens from PGN line
 * ============================================================================
 */

static int tokenize_pgn_line(char *line, char **tokens, int max_tokens) {
    int count = 0;
    char *token = strtok(line, " \t\n\r");
    
    while (token && count < max_tokens) {
        // Skip empty tokens
        if (strlen(token) > 0) {
            tokens[count++] = token;
        }
        token = strtok(NULL, " \t\n\r");
    }
    
    return count;
}

/* ============================================================================
 * IMPORT: Load game from PGN file
 * ============================================================================
 */

bool pgn_import_game(BeatChessVisualization *chess, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s' for reading\n", filename);
        return false;
    }
    
    // Reset game
    chess_init_board(&chess->game);
    chess->move_history_count = 0;
    chess->status = CHESS_PLAYING;
    chess->move_count = 0;
    
    char line[PGN_LINE_SIZE];
    bool in_moves = false;
    int moves_parsed = 0;
    int moves_failed = 0;
    
    // Read file line by line
    while (fgets(line, sizeof(line), f)) {
        // Skip header lines
        if (line[0] == '[') {
            in_moves = false;
            continue;
        }
        
        // Skip empty lines
        if (strlen(line) < 2 || (line[0] == '\n' && line[1] == '\0')) {
            in_moves = true;  // Start of moves section
            continue;
        }
        
        if (in_moves) {
            // Make a copy since strtok modifies the string
            char line_copy[PGN_LINE_SIZE];
            strncpy(line_copy, line, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            
            // Tokenize the line
            char *tokens[256];
            int token_count = tokenize_pgn_line(line_copy, tokens, 256);
            
            for (int i = 0; i < token_count; i++) {
                const char *token = tokens[i];
                
                // Skip move numbers (contain a dot)
                if (strchr(token, '.')) {
                    continue;
                }
                
                // Skip result markers
                if (strcmp(token, "1-0") == 0 || strcmp(token, "0-1") == 0 ||
                    strcmp(token, "1/2-1/2") == 0 || strcmp(token, "*") == 0) {
                    continue;
                }
                
                // Try to parse as move
                ChessMove move = {0};
                if (parse_algebraic_move(&chess->game, token, &move)) {
                    if (chess_is_valid_move(&chess->game, move.from_row, move.from_col,
                                           move.to_row, move.to_col)) {
                        // Save game state before move
                        chess->move_history[chess->move_history_count].game_state = chess->game;
                        chess->move_history[chess->move_history_count].move = move;
                        chess->move_history[chess->move_history_count].time_elapsed = 0.0;
                        
                        // Execute the move
                        chess_make_move(&chess->game, move);
                        chess->move_history_count++;
                        chess->move_count++;
                        moves_parsed++;
                        
                        if (chess->move_history_count >= MAX_MOVE_HISTORY) {
                            fprintf(stderr, "Warning: Move history buffer full\n");
                            break;
                        }
                    } else {
                        moves_failed++;
                        fprintf(stderr, "Warning: Illegal move in PGN: %s (turn=%s)\n",
                                token, chess->game.turn == WHITE ? "White" : "Black");
                    }
                } else {
                    // Not a valid move notation
                    if (strlen(token) > 1 && isalpha(token[0])) {
                        moves_failed++;
                    }
                }
            }
        }
    }
    
    fclose(f);
    
    printf("Game loaded: %d moves parsed, %d failed\n", moves_parsed, moves_failed);
    
    // Update game status
    chess->status = chess_check_game_status(&chess->game);
    
    return (moves_failed == 0);  // Success if no failed moves
}

/* ============================================================================
 * Utility: List available PGN files in a directory
 * ============================================================================
 */

#ifdef _WIN32
#include <windows.h>

int pgn_list_files(const char *directory, char **filenames, int max_files) {
    WIN32_FIND_DATA find_data;
    HANDLE find_handle;
    char search_path[512];
    int count = 0;
    
    snprintf(search_path, sizeof(search_path), "%s\\*.pgn", directory);
    
    find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return 0;  // No files found
    }
    
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (count < max_files) {
                filenames[count] = malloc(strlen(find_data.cFileName) + 1);
                strcpy(filenames[count], find_data.cFileName);
                count++;
            }
        }
    } while (FindNextFile(find_handle, &find_data));
    
    FindClose(find_handle);
    return count;
}

#else
#include <dirent.h>

int pgn_list_files(const char *directory, char **filenames, int max_files) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    dir = opendir(directory);
    if (!dir) {
        return 0;  // Cannot open directory
    }
    
    while ((entry = readdir(dir)) && count < max_files) {
        // Check if filename ends with .pgn
        int len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".pgn") == 0) {
            filenames[count] = malloc(strlen(entry->d_name) + 1);
            strcpy(filenames[count], entry->d_name);
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

#endif

/* ============================================================================
 * Get default save directory
 * ============================================================================
 */

void pgn_get_default_directory(char *path, size_t path_size) {
#ifdef MSDOS
    strcpy(path, ".");  // Current directory on DOS
#elif defined(_WIN32)
    // Windows: My Documents
    const char *home = getenv("USERPROFILE");
    if (home) {
        snprintf(path, path_size, "%s\\Documents\\BeatChess", home);
    } else {
        strcpy(path, ".\\BeatChess");
    }
#else
    // Linux/Mac: home directory
    const char *home = getenv("HOME");
    if (home) {
        snprintf(path, path_size, "%s/.beatchess", home);
    } else {
        strcpy(path, ".");
    }
#endif
}

/* ============================================================================
 * Create save directory if it doesn't exist
 * ============================================================================
 */

bool pgn_ensure_directory(const char *directory) {
#ifdef _WIN32
    return CreateDirectory(directory, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(directory, 0755) == 0 || errno == EEXIST;
#endif
}
