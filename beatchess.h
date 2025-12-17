#ifndef BEATCHESS_H
#define BEATCHESS_H

#include <stdbool.h>
#include <string.h>

/* Platform detection and conditional includes */
#ifdef MSDOS
    /* DOS/DJGPP environment - no pthread support */
    #define BEATCHESS_DOS 1
    #define BEATCHESS_HAS_PTHREAD 0
#else
    /* Unix/Linux/Windows with modern compiler - pthread support */
    #define BEATCHESS_DOS 0
    #define BEATCHESS_HAS_PTHREAD 1
    #include <pthread.h>
#endif

#define BOARD_SIZE 8
#define MAX_CHESS_DEPTH 10
#define BEAT_HISTORY_SIZE 10
#define MAX_MOVES_BEFORE_DRAW 300
#define MAX_MOVE_HISTORY (2 * MAX_MOVES_BEFORE_DRAW)  /* 600 - straight buffer, 2x moves before stalemate */
/* ============================================================================
 * Straight Buffer Macros for Move History (no circular wrapping)
 * ============================================================================
 */
#define MOVE_HISTORY_AT(buffer, idx) ((buffer)[(idx)])
#define MOVE_HISTORY_IS_FULL(count) ((count) >= MAX_MOVE_HISTORY)

typedef enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING } PieceType;
typedef enum { NONE, WHITE, BLACK } ChessColor;

typedef struct {
    PieceType type;
    ChessColor color;
} ChessPiece;

typedef struct {
    int from_row, from_col;
    int to_row, to_col;
    int score;
} ChessMove;

typedef struct {
    ChessPiece board[BOARD_SIZE][BOARD_SIZE];
    ChessColor turn;
    bool white_king_moved, black_king_moved;
    bool white_rook_a_moved, white_rook_h_moved;
    bool black_rook_a_moved, black_rook_h_moved;
    int en_passant_col; // -1 if no en passant available, otherwise the column
    int en_passant_row; // The row where en passant capture would land
} ChessGameState;

/* Platform-specific ChessThinkingState */
typedef struct {
    ChessGameState game;
    ChessMove best_move;
    int best_score;
    int current_depth;
    bool has_move;
    bool thinking;
#if BEATCHESS_HAS_PTHREAD
    pthread_mutex_t lock;
    pthread_t thread;
#endif
} ChessThinkingState;

typedef enum {
    CHESS_PLAYING,
    CHESS_CHECKMATE_WHITE,
    CHESS_CHECKMATE_BLACK,
    CHESS_STALEMATE
} ChessGameStatus;

typedef struct {
    ChessGameState game_state;
    ChessMove move;
    double time_elapsed;  // Time spent on this move
} MoveHistory;

typedef struct {
    // Game state
    ChessGameState game;
    ChessThinkingState thinking_state;
    ChessGameStatus status;
    
    // Animation state (GTK version uses these, DOS doesn't)
    double piece_x[BOARD_SIZE][BOARD_SIZE];
    double piece_y[BOARD_SIZE][BOARD_SIZE];
    double target_x[BOARD_SIZE][BOARD_SIZE];
    double target_y[BOARD_SIZE][BOARD_SIZE];
    
    // Last move highlight
    double last_move_glow;
    double status_flash_timer;
    double status_flash_color[3]; // RGB
    int last_eval_change;
    
    // Beat detection (for sound-reactive features)
    double beat_volume_history[BEAT_HISTORY_SIZE];
    int beat_history_index;
    double time_since_last_move;
    double beat_threshold;
    
    // Animation state
    int animating_from_row, animating_from_col;
    int animating_to_row, animating_to_col;
    double animation_progress;
    bool is_animating;
    
    // Evaluation bar (GTK visualizations)
    double eval_bar_position; // -1 to 1, smoothed
    double eval_bar_target;

    int beats_since_game_over;
    bool waiting_for_restart;

    double time_thinking;          // How long AI has been thinking
    double min_think_time;         // Minimum time before auto-play (e.g., 0.5s)
    int good_move_threshold;       // Score threshold to auto-play (e.g., 200)
    bool auto_play_enabled;        // Whether to auto-play good moves
    
    // Check/Checkmate/Stalemate display
    bool is_in_check;              // True if current player is in check
    double check_display_timer;    // Display "CHECK" for 1 second (0 = not displayed)
    bool is_checkmate;             // True if game is in checkmate (permanent display)
    bool is_stalemate;             // True if game is in stalemate (permanent display)
    
    // GTK UI buttons and controls
    double reset_button_x, reset_button_y;
    double reset_button_width, reset_button_height;
    bool reset_button_hovered;
    double reset_button_glow;
    bool reset_button_was_pressed;
    
    double pvsa_button_x, pvsa_button_y;
    double pvsa_button_width, pvsa_button_height;
    bool pvsa_button_hovered;
    double pvsa_button_glow;
    bool pvsa_button_was_pressed;
    
    double undo_button_x, undo_button_y;
    double undo_button_width, undo_button_height;
    bool undo_button_hovered;
    double undo_button_glow;
    bool undo_button_was_pressed;
    
    double flip_button_x, flip_button_y;
    double flip_button_width, flip_button_height;
    bool flip_button_hovered;
    double flip_button_glow;
    bool flip_button_was_pressed;
    
    // Common state for all platforms
    int last_from_row, last_from_col;
    int last_to_row, last_to_col;
    
    // Status display
    char status_text[256];
    
    // Visual elements
    double board_offset_x, board_offset_y;
    double cell_size;
    int move_count;
    
    MoveHistory move_history[MAX_MOVE_HISTORY*2];
    int move_history_count;      // Total number of moves made (increments continuously)
    
    // Time tracking
    double white_total_time;  // Cumulative time for White
    double black_total_time;  // Cumulative time for Black
    double current_move_start_time;  // When current move phase started (for display)
    double last_move_end_time;  // When the last move was completed (for accurate timing)
    
    // Player control state
    bool player_vs_ai;  // true = Player vs AI, false = AI vs AI
    bool board_flipped;  // true = board flipped (player plays Black), false = normal (player plays White)
    
    int selected_piece_row, selected_piece_col;  // Currently selected piece
    bool has_selected_piece;
    int selected_piece_was_pressed;  // Mouse state for selection
    
} BeatChessVisualization;

#ifdef MSDOS
/* ============================================================================
 * Color definitions for Allegro 4
 * ============================================================================
 */

#define COLOR_BLACK     0
#define COLOR_BLUE      1
#define COLOR_GREEN     2
#define COLOR_CYAN      3
#define COLOR_RED       4
#define COLOR_MAGENTA   5
#define COLOR_YELLOW    6
#define COLOR_WHITE     7
#define COLOR_GRAY      8

/* ============================================================================
 * UI Button Structure
 * ============================================================================
 */

typedef struct {
    int x, y, w, h;
    const char *label;
    int hotkey;  /* Keyboard shortcut */
    bool enabled;
} Button;

/* ============================================================================
 * Global game state
 * ============================================================================
 */

typedef struct {
    ChessGameState game;
    
    /* Board state */
    int selected_row, selected_col;
    int piece_selected_row, piece_selected_col;  /* Track where piece was selected from */
    bool piece_selected;
    int last_move_from_row, last_move_from_col;
    int last_move_to_row, last_move_to_col;
    bool has_last_move;
    
    /* History - straight buffer (no circular wrapping) */
    ChessGameState history[MAX_MOVE_HISTORY*2];
    MoveHistory move_history[MAX_MOVE_HISTORY*2];     /* Use MoveHistory for timing info */
    int history_size;              /* Number of moves currently in buffer (0 to MAX_MOVE_HISTORY) */
    int history_capacity;          /* Buffer capacity (should be MAX_MOVE_HISTORY) */
    
    /* UI state */
    bool show_help;
    bool show_about;
    bool show_menu;
    int menu_selected;
    bool ai_vs_ai;
    bool player_is_white;
    
    /* AI state */
    int ai_move_delay;
    int ai_move_counter;
    bool ai_thinking;
    bool ai_computing;  /* NEW: true when AI is actually in compute_ai_move() */
    ChessMove ai_best_move;
    int ai_search_depth;
    int ai_total_moves;
    int ai_evaluated_moves;
    
    /* Timer state */
    int white_time_seconds;
    int white_time_frames;
    int black_time_seconds;
    int black_time_frames;
    bool timer_started;
    int ai_thinking_start_time;  /* Track when AI started thinking */
    
    /* Check/Checkmate/Stalemate display */
    bool is_in_check;              /* True if current player is in check */
    double check_display_timer;    /* Display "CHECK" for 1 second (0 = not displayed) */
    bool is_checkmate;             /* True if game is in checkmate */
    bool is_stalemate;             /* True if game is in stalemate */
    
    /* Move history with timing information */
    int move_count;                /* Total number of moves made (for display: "Move: N") */
    int move_history_count;        /* Count of moves in history (for circular buffer) */
    int move_history_index;        /* Current write position in circular buffer */
    
    /* Time tracking per move */
    double white_total_time;       /* Cumulative time for White */
    double black_total_time;       /* Cumulative time for Black */
    double current_move_start_time; /* When current move phase started */
    double last_move_end_time;     /* When the last move was completed */
    
} ChessGUI;

#endif

/* Function declarations */
bool chess_can_undo(BeatChessVisualization *chess);
void chess_init_board(ChessGameState *game);
bool chess_is_valid_move(ChessGameState *game, int fr, int fc, int tr, int tc);
void chess_execute_move(ChessGameState *game, int fr, int fc, int tr, int tc);
bool chess_is_in_bounds(int r, int c);
bool chess_is_path_clear(ChessGameState *game, int fr, int fc, int tr, int tc);
void chess_make_move(ChessGameState *game, ChessMove move);

/**
 * Add a move to the history using circular buffer logic
 */
void chess_save_move_history(BeatChessVisualization *chess, ChessMove move, double time_elapsed);

int chess_get_all_moves(ChessGameState *game, ChessColor color, ChessMove *moves);
int chess_evaluate_position(ChessGameState *game);
bool chess_is_in_check(ChessGameState *game, ChessColor color);
int chess_minimax(ChessGameState *game, int depth, int alpha, int beta, bool maximizing);
void save_position_to_history();
void save_position_to_history_with_move(int from_r, int from_c, int to_r, int to_c);
void undo_move();

#endif // BEATCHESS_H
