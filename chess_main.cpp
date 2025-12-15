#include <gtk/gtk.h>
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "beatchess.h"
#include "visualization.h"

// Define fallback version if not provided by Makefile
#ifndef VERSION
#define VERSION "1.0"
#endif

#define MAX_MOVES_BEFORE_DRAW 150
#define MAX_MOVE_HISTORY 256

typedef enum {
    SKILL_MORONIC = 2,
    SKILL_EASY = 4,
    SKILL_MEDIUM = 6,
    SKILL_HARD = 8,
    SKILL_EXPERT = 10
} ChessSkillLevel;

typedef struct {
    ChessGameState game_state;
    ChessMove move;
} MoveHistoryEntry;

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *status_label;
    GtkWidget *flip_button;
    GtkWidget *undo_button;
    GtkWidget *time_label;
    GtkWidget *skill_combo;
    GtkWidget *player_color_item;  // Menu item for player color toggle
    
    ChessGameState game;
    ChessThinkingState thinking_state;
    ChessGameStatus status;
    
    int selected_row;
    int selected_col;
    bool has_selection;
    
    int last_from_row, last_from_col;
    int last_to_row, last_to_col;
    
    double cell_size;
    double board_offset_x;
    double board_offset_y;
    
    bool player_is_white;
    bool zero_players;  // AI vs AI mode
    bool two_player;    // Two-player mode (human vs human)
    bool board_flipped; // Board perspective
    
    int move_count;
    double ai_think_time;  // Time AI has been thinking
    
    // Move history for undo
    MoveHistoryEntry move_history[MAX_MOVE_HISTORY];
    int move_history_count;
    
    // Time tracking
    double white_total_time;
    double black_total_time;
    double current_move_start_time;
    double last_move_end_time;
    gint time_update_source;  // Timer for updating time display
    
    // Skill level
    ChessSkillLevel skill_level;
    int last_depth_reached;  // Track the last completed search depth
} ChessGUI;

// Forward declarations
void draw_piece(cairo_t *cr, PieceType type, ChessColor color, double x, double y, double size, double dance_offset);
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data);
gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);
void update_status_text(ChessGUI *gui);
void make_ai_move(ChessGUI *gui);
gboolean ai_move_timeout(gpointer data);
void on_flip_board(GtkWidget *widget, gpointer data);
void on_undo_move(GtkWidget *widget, gpointer data);
void record_move(ChessGUI *gui, ChessMove move);
void undo_last_move(ChessGUI *gui);
gboolean update_time_display(gpointer data);
double get_current_time(void);
void on_skill_changed(GtkComboBox *combo, gpointer data);
void set_search_depth(ChessGUI *gui, ChessSkillLevel skill);
void on_menu_help(GtkMenuItem *menuitem, gpointer user_data);  // Help menu callback
void on_toggle_player_color(GtkWidget *widget, gpointer data);  // New: toggle player color

// External chess engine functions
extern void chess_start_thinking_depth(ChessThinkingState *ts, ChessGameState *game, int max_depth);

// Get current time in seconds
double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Set search depth based on skill level
void set_search_depth(ChessGUI *gui, ChessSkillLevel skill) {
    gui->skill_level = skill;
    
    // Restart thinking with new depth if game is in progress
    if (gui->status == CHESS_PLAYING) {
        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)skill);
    }
}

// Handle skill level combo box change
void on_skill_changed(GtkComboBox *combo, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    gint active = gtk_combo_box_get_active(combo);
    
    ChessSkillLevel skills[] = {
        SKILL_MORONIC,
        SKILL_EASY,
        SKILL_MEDIUM,
        SKILL_HARD,
        SKILL_EXPERT
    };
    
    if (active >= 0 && active < 5) {
        set_search_depth(gui, skills[active]);
    }
}


// Toggle player color (White vs Black)
void on_toggle_player_color(GtkWidget *widget, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    
    // Only allow in single-player mode
    if (gui->two_player || gui->zero_players) {
        return;
    }
    
    gui->player_is_white = !gui->player_is_white;
    
    // Update board flip automatically: if playing Black, start with board flipped
    gui->board_flipped = !gui->player_is_white;
    
    // Restart the game
    chess_init_board(&gui->game);
    gui->status = CHESS_PLAYING;
    gui->move_count = 0;
    gui->move_history_count = 0;
    gui->has_selection = false;
    gui->last_from_row = -1;
    gui->ai_think_time = 0;
    gui->white_total_time = 0;
    gui->black_total_time = 0;
    gui->current_move_start_time = 0;
    
    // Update menu item check mark
    if (GTK_IS_CHECK_MENU_ITEM(gui->player_color_item)) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gui->player_color_item), 
                                       !gui->player_is_white);
    }
    
    // Start AI thinking if player is White (AI starts with Black)
    chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
    
    update_status_text(gui);
    gtk_widget_queue_draw(gui->drawing_area);
}

gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    gui->cell_size = fmin(width / 8.5, height / 8.5);
    gui->board_offset_x = (width - gui->cell_size * 8) / 2;
    gui->board_offset_y = (height - gui->cell_size * 8) / 2;
    
    // Draw background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.25);
    cairo_paint(cr);
    
    // Draw board
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            // Handle board flipping
            int display_r = gui->board_flipped ? (7 - r) : r;
            int display_c = gui->board_flipped ? (7 - c) : c;
            
            bool is_light = (display_r + display_c) % 2 == 0;
            
            if (is_light) {
                cairo_set_source_rgb(cr, 0.9, 0.9, 0.85);
            } else {
                cairo_set_source_rgb(cr, 0.4, 0.5, 0.4);
            }
            
            cairo_rectangle(cr, gui->board_offset_x + c * gui->cell_size,
                          gui->board_offset_y + r * gui->cell_size,
                          gui->cell_size, gui->cell_size);
            cairo_fill(cr);
        }
    }
    
    // Highlight selected square
    if (gui->has_selection) {
        int display_row = gui->board_flipped ? (7 - gui->selected_row) : gui->selected_row;
        int display_col = gui->board_flipped ? (7 - gui->selected_col) : gui->selected_col;
        
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.5);
        cairo_rectangle(cr, gui->board_offset_x + display_col * gui->cell_size,
                       gui->board_offset_y + display_row * gui->cell_size,
                       gui->cell_size, gui->cell_size);
        cairo_fill(cr);
    }
    
    // Highlight last move
    if (gui->last_from_row >= 0) {
        int from_row = gui->board_flipped ? (7 - gui->last_from_row) : gui->last_from_row;
        int from_col = gui->board_flipped ? (7 - gui->last_from_col) : gui->last_from_col;
        int to_row = gui->board_flipped ? (7 - gui->last_to_row) : gui->last_to_row;
        int to_col = gui->board_flipped ? (7 - gui->last_to_col) : gui->last_to_col;
        
        cairo_set_source_rgba(cr, 0.5, 0.8, 1.0, 0.3);
        cairo_rectangle(cr, gui->board_offset_x + from_col * gui->cell_size,
                       gui->board_offset_y + from_row * gui->cell_size,
                       gui->cell_size, gui->cell_size);
        cairo_fill(cr);
        cairo_rectangle(cr, gui->board_offset_x + to_col * gui->cell_size,
                       gui->board_offset_y + to_row * gui->cell_size,
                       gui->cell_size, gui->cell_size);
        cairo_fill(cr);
    }
    
    // Draw coordinates
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, gui->cell_size * 0.2);
    
    for (int i = 0; i < 8; i++) {
        char label[2];
        
        // Handle flipped coordinates
        if (gui->board_flipped) {
            label[0] = 'h' - i;
            label[1] = '\0';
            cairo_move_to(cr, gui->board_offset_x + i * gui->cell_size + gui->cell_size * 0.05,
                         gui->board_offset_y + 8 * gui->cell_size - gui->cell_size * 0.05);
            cairo_show_text(cr, label);
            
            label[0] = '1' + i;
            cairo_move_to(cr, gui->board_offset_x + gui->cell_size * 0.05,
                         gui->board_offset_y + i * gui->cell_size + gui->cell_size * 0.25);
            cairo_show_text(cr, label);
        } else {
            label[0] = 'a' + i;
            label[1] = '\0';
            cairo_move_to(cr, gui->board_offset_x + i * gui->cell_size + gui->cell_size * 0.05,
                         gui->board_offset_y + 8 * gui->cell_size - gui->cell_size * 0.05);
            cairo_show_text(cr, label);
            
            label[0] = '8' - i;
            cairo_move_to(cr, gui->board_offset_x + gui->cell_size * 0.05,
                         gui->board_offset_y + i * gui->cell_size + gui->cell_size * 0.25);
            cairo_show_text(cr, label);
        }
    }
    
    // Draw pieces
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            ChessPiece piece = gui->game.board[r][c];
            if (piece.type != EMPTY) {
                // Convert board coords to display coords if flipped
                int display_r = gui->board_flipped ? (7 - r) : r;
                int display_c = gui->board_flipped ? (7 - c) : c;
                
                double x = gui->board_offset_x + display_c * gui->cell_size;
                double y = gui->board_offset_y + display_r * gui->cell_size;
                
                cairo_save(cr);
                cairo_translate(cr, 2, 2);
                cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
                draw_piece(cr, piece.type, piece.color, x, y, gui->cell_size, 0);
                cairo_restore(cr);
                
                draw_piece(cr, piece.type, piece.color, x, y, gui->cell_size, 0);
            }
        }
    }
    
    return FALSE;
}

void update_status_text(ChessGUI *gui) {
    char status[512];
    char time_str[256];
    
    // Calculate current move time
    double current_move_time = 0;
    if (gui->current_move_start_time > 0) {
        current_move_time = get_current_time() - gui->current_move_start_time;
    }
    
    // Get current player's time
    double white_time = gui->white_total_time;
    double black_time = gui->black_total_time;
    
    // Add current move time if still thinking and timer started
    if (gui->current_move_start_time > 0) {
        if (gui->game.turn == WHITE) {
            white_time += current_move_time;
        } else {
            black_time += current_move_time;
        }
    }
    
    // Read current_depth safely with mutex
    int current_depth = 0;
    bool is_thinking = false;
    pthread_mutex_lock(&gui->thinking_state.lock);
    current_depth = gui->thinking_state.current_depth;
    is_thinking = gui->thinking_state.thinking;
    pthread_mutex_unlock(&gui->thinking_state.lock);
    
    // Use current or last depth for skill display (currently not used but kept for future expansion)
    (void)(is_thinking ? current_depth : gui->last_depth_reached);
    
    // Format time display with current depth
    snprintf(time_str, sizeof(time_str), " | White: %.1fs | Black: %.1fs | Skill: Depth %d",
             white_time, black_time, gui->skill_level);
    
    if (gui->status == CHESS_CHECKMATE_WHITE) {
        snprintf(status, sizeof(status), "Checkmate! Black wins! (Move %d)%s", 
                gui->move_count, time_str);
    } else if (gui->status == CHESS_CHECKMATE_BLACK) {
        snprintf(status, sizeof(status), "Checkmate! White wins! (Move %d)%s",
                gui->move_count, time_str);
    } else if (gui->status == CHESS_STALEMATE) {
        snprintf(status, sizeof(status), "Stalemate! Draw! (Move %d)%s",
                gui->move_count, time_str);
    } else if (gui->move_count >= MAX_MOVES_BEFORE_DRAW) {
        snprintf(status, sizeof(status), "Draw by move limit! (Move %d)%s",
                gui->move_count, time_str);
    } else {
        if (chess_is_in_check(&gui->game, gui->game.turn)) {
            snprintf(status, sizeof(status), "Move %d - %s to move (CHECK!)%s",
                    gui->move_count, gui->game.turn == WHITE ? "White" : "Black", time_str);
        } else {
            snprintf(status, sizeof(status), "Move %d - %s to move%s",
                    gui->move_count, gui->game.turn == WHITE ? "White" : "Black", time_str);
        }
    }
    
    gtk_label_set_text(GTK_LABEL(gui->status_label), status);
    
    // Update undo button sensitivity
    gtk_widget_set_sensitive(gui->undo_button, gui->move_history_count > 0);
    
    // Update flip button sensitivity (only for single player or two-player)
    gtk_widget_set_sensitive(gui->flip_button, !gui->zero_players);
}

gboolean update_time_display(gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    
    if (gui->status == CHESS_PLAYING) {
        update_status_text(gui);
        gtk_widget_queue_draw(gui->drawing_area);
    }
    
    return G_SOURCE_CONTINUE;
}

void record_move(ChessGUI *gui, ChessMove move) {
    if (gui->move_history_count >= MAX_MOVE_HISTORY) {
        return;  // History full
    }
    
    // Start timer on first move
    if (gui->current_move_start_time == 0) {
        gui->current_move_start_time = get_current_time();
    }
    
    // Record time for current move (only after first move)
    if (gui->move_history_count > 0 && gui->current_move_start_time > 0) {
        double move_time = get_current_time() - gui->current_move_start_time;
        if (gui->game.turn == WHITE) {
            gui->white_total_time += move_time;
        } else {
            gui->black_total_time += move_time;
        }
    }
    
    MoveHistoryEntry *entry = &gui->move_history[gui->move_history_count];
    entry->game_state = gui->game;
    entry->move = move;
    gui->move_history_count++;
    
    // Start timer for next move
    gui->current_move_start_time = get_current_time();
}

void undo_last_move(ChessGUI *gui) {
    if (gui->move_history_count < 1) {
        return;  // Can't undo
    }
    
    if (!gui->two_player && !gui->zero_players) {
        // Single-player: undo both AI's move and human's move
        // move_history stores state AFTER each move
        // If we have 2+ moves: [0]=after human move, [1]=after AI move
        // We want to go back to the state BEFORE the human move
        if (gui->move_history_count < 2) {
            return;  // Can't undo - need both human and AI moves
        }
        
        // To go back before human's move, restore state from move_history[0] before decrement
        // Actually, we need to go back to the original position, which is before index 0
        // So we need to reconstruct by going back 2 in the count
        gui->move_history_count -= 2;
        
        // Restore to state before both moves (index will be 0 positions back if count was 2)
        if (gui->move_history_count > 0) {
            gui->game = gui->move_history[gui->move_history_count - 1].game_state;
        } else {
            // If no more moves, reset to starting position
            chess_init_board(&gui->game);
        }
        gui->move_count -= 2;
    } else {
        // Two-player or AI vs AI: just undo one move
        gui->move_history_count--;
        if (gui->move_history_count > 0) {
            gui->game = gui->move_history[gui->move_history_count - 1].game_state;
        } else {
            chess_init_board(&gui->game);
        }
        gui->move_count--;
    }
    
    gui->has_selection = false;
    gui->status = chess_check_game_status(&gui->game);
    
    // Reset time tracking for next move
    gui->current_move_start_time = get_current_time();
    
    // Restart AI thinking if needed
    if (!gui->two_player && !gui->zero_players && gui->status == CHESS_PLAYING) {
        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
        gui->ai_think_time = 0;
    }
    
    update_status_text(gui);
    gtk_widget_queue_draw(gui->drawing_area);
}

void make_ai_move(ChessGUI *gui) {
    
    // Give the thinking thread a moment to complete at least depth 1
    usleep(100000);  // 100ms delay to let thread start searching
    
    // Read the depth BEFORE getting the move (which stops thinking)
    pthread_mutex_lock(&gui->thinking_state.lock);
    (void)gui->thinking_state.current_depth;  // Note: depth available but not currently displayed
    (void)gui->thinking_state.best_score;     // Note: best_score available but not currently displayed
    pthread_mutex_unlock(&gui->thinking_state.lock);
    
    // Now get the best move (this sets thinking = false)
    ChessMove ai_move = chess_get_best_move_now(&gui->thinking_state);
    
    fflush(stdout);
    
    if (chess_is_valid_move(&gui->game, ai_move.from_row, ai_move.from_col,
                            ai_move.to_row, ai_move.to_col)) {
        ChessGameState temp = gui->game;
        chess_make_move(&temp, ai_move);
        
        if (!chess_is_in_check(&temp, gui->game.turn)) {
            gui->last_from_row = ai_move.from_row;
            gui->last_from_col = ai_move.from_col;
            gui->last_to_row = ai_move.to_row;
            gui->last_to_col = ai_move.to_col;
            
            chess_make_move(&gui->game, ai_move);
            record_move(gui, ai_move);
            gui->move_count++;
            
            gui->status = chess_check_game_status(&gui->game);
            
            if (gui->status == CHESS_PLAYING && gui->move_count < MAX_MOVES_BEFORE_DRAW) {
                chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
                gui->ai_think_time = 0;  // Reset think timer
            }
        } else {
            chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
            gui->ai_think_time = 0;
        }
    } else {
        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
        gui->ai_think_time = 0;
    }
    
    update_status_text(gui);
    gtk_widget_queue_draw(gui->drawing_area);
}

gboolean ai_move_timeout(gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    
    if (gui->status != CHESS_PLAYING || gui->move_count >= MAX_MOVES_BEFORE_DRAW) {
        // Game over - restart
        chess_init_board(&gui->game);
        gui->status = CHESS_PLAYING;
        gui->move_count = 0;
        gui->move_history_count = 0;
        gui->last_from_row = -1;
        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
        gui->ai_think_time = 0;
        update_status_text(gui);
        gtk_widget_queue_draw(gui->drawing_area);
        return G_SOURCE_CONTINUE;
    }
    
    bool should_move = false;
    
    if (gui->zero_players) {
        // AI vs AI mode
        should_move = true;
    } else if (gui->two_player) {
        // Two-player mode - no AI moves
        should_move = false;
    } else {
        // Single player - only when it's AI's turn
        should_move = (gui->player_is_white && gui->game.turn == BLACK) ||
                      (!gui->player_is_white && gui->game.turn == WHITE);
    }
    
    if (should_move) {
        gui->ai_think_time += 1.0;
        
        // Force move after 2 seconds for fast-paced play, 10 seconds as hard limit
        if (gui->ai_think_time >= 2.0) {
            make_ai_move(gui);
        }
    }
    
    return G_SOURCE_CONTINUE;
}

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    
    if (gui->zero_players && !gui->two_player) return FALSE;  // No clicking in AI vs AI mode
    if (gui->status != CHESS_PLAYING) return FALSE;
    if (gui->move_count >= MAX_MOVES_BEFORE_DRAW) return FALSE;
    
    // Check if it's player's turn
    if (!gui->two_player && !gui->zero_players) {
        // Single player mode
        if ((gui->player_is_white && gui->game.turn != WHITE) ||
            (!gui->player_is_white && gui->game.turn != BLACK)) {
            return FALSE;
        }
    }
    // In two-player mode, both players can click (we don't check AI turns)
    
    int click_col = (event->x - gui->board_offset_x) / gui->cell_size;
    int click_row = (event->y - gui->board_offset_y) / gui->cell_size;
    
    // Convert display coords back to board coords if flipped
    if (gui->board_flipped) {
        click_row = 7 - click_row;
        click_col = 7 - click_col;
    }
    
    if (click_row < 0 || click_row >= 8 || click_col < 0 || click_col >= 8) return FALSE;
    
    if (!gui->has_selection) {
        // First click - select piece
        ChessPiece piece = gui->game.board[click_row][click_col];
        if (piece.type != EMPTY && piece.color == gui->game.turn) {
            gui->selected_row = click_row;
            gui->selected_col = click_col;
            gui->has_selection = true;
            gtk_widget_queue_draw(widget);
        }
    } else {
        // Second click - try to move
        if (click_row == gui->selected_row && click_col == gui->selected_col) {
            // Clicked same square - deselect
            gui->has_selection = false;
        } else if (chess_is_valid_move(&gui->game, gui->selected_row, gui->selected_col, click_row, click_col)) {
            ChessGameState temp = gui->game;
            ChessMove move = {gui->selected_row, gui->selected_col, click_row, click_col, 0};
            chess_make_move(&temp, move);
            
            if (!chess_is_in_check(&temp, gui->game.turn)) {
                gui->last_from_row = gui->selected_row;
                gui->last_from_col = gui->selected_col;
                gui->last_to_row = click_row;
                gui->last_to_col = click_col;
                
                chess_make_move(&gui->game, move);
                record_move(gui, move);
                gui->move_count++;
                gui->has_selection = false;
                
                gui->status = chess_check_game_status(&gui->game);
                
                if (gui->status == CHESS_PLAYING && gui->move_count < MAX_MOVES_BEFORE_DRAW) {
                    if (!gui->two_player && !gui->zero_players) {
                        // Start AI thinking in single player
                        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
                    }
                }
                
                update_status_text(gui);
            } else {
                gui->has_selection = false;
            }
        } else {
            // Try to select new piece
            ChessPiece piece = gui->game.board[click_row][click_col];
            if (piece.type != EMPTY && piece.color == gui->game.turn) {
                gui->selected_row = click_row;
                gui->selected_col = click_col;
            } else {
                gui->has_selection = false;
            }
        }
        
        gtk_widget_queue_draw(widget);
    }
    
    return TRUE;
}

void on_flip_board(GtkWidget *widget, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    gui->board_flipped = !gui->board_flipped;
    gtk_widget_queue_draw(gui->drawing_area);
}

void on_undo_move(GtkWidget *widget, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    undo_last_move(gui);
}

void on_new_game(GtkWidget *widget, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    
    chess_stop_thinking(&gui->thinking_state);
    chess_init_board(&gui->game);
    gui->status = CHESS_PLAYING;
    gui->move_count = 0;
    gui->move_history_count = 0;
    gui->has_selection = false;
    gui->last_from_row = -1;
    gui->ai_think_time = 0;
    
    // Only reset board flip if in single-player, otherwise keep current state
    if (!gui->two_player && !gui->zero_players) {
        gui->board_flipped = !gui->player_is_white;
    } else {
        gui->board_flipped = false;
    }
    
    // Reset time tracking - do NOT start timer yet
    gui->white_total_time = 0;
    gui->black_total_time = 0;
    gui->current_move_start_time = 0;  // Set to 0, will start on first move
    gui->last_move_end_time = 0;
    
    if (!gui->two_player && !gui->zero_players) {
        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
    } else if (gui->zero_players) {
        chess_start_thinking_depth(&gui->thinking_state, &gui->game, (int)gui->skill_level);
    }
    
    update_status_text(gui);
    gtk_widget_queue_draw(gui->drawing_area);
}

void on_toggle_two_player(GtkWidget *widget, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    gui->two_player = !gui->two_player;
    
    // Restart the game when toggling mode
    on_new_game(widget, data);
}

void on_toggle_zero_players(GtkWidget *widget, gpointer data) {
    ChessGUI *gui = (ChessGUI*)data;
    gui->zero_players = !gui->zero_players;
    gui->two_player = false;  // Can't be both
    
    // Restart the game when toggling mode
    on_new_game(widget, data);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    
    gtk_init(&argc, &argv);
    
    ChessGUI gui;
    memset(&gui, 0, sizeof(gui));
    
    gui.player_is_white = true;
    gui.zero_players = false;
    gui.two_player = false;
    gui.board_flipped = false;
    gui.last_from_row = -1;
    gui.ai_think_time = 0;
    gui.white_total_time = 0;
    gui.black_total_time = 0;
    
    // Parse command line args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--black") == 0 || strcmp(argv[i], "-b") == 0) {
            gui.player_is_white = false;
        }
        if (strcmp(argv[i], "--zero-players") == 0 || strcmp(argv[i], "-z") == 0) {
            gui.zero_players = true;
        }
        if (strcmp(argv[i], "--two-player") == 0 || strcmp(argv[i], "-2") == 0) {
            gui.two_player = true;
            gui.zero_players = false;
        }
    }
    
    // Initialize game
    chess_init_board(&gui.game);
    chess_init_thinking_state(&gui.thinking_state);
    gui.status = CHESS_PLAYING;
    gui.move_count = 0;
    gui.move_history_count = 0;
    gui.current_move_start_time = 0;  // Don't start timer yet, will start on first move
    gui.last_move_end_time = 0;
    gui.skill_level = SKILL_MEDIUM;  // Default to medium
    gui.last_depth_reached = 0;  // Initialize depth tracker
    
    // Start AI thinking if not in two-player or zero-player mode initially
    if (!gui.two_player && !gui.zero_players) {
        chess_start_thinking_depth(&gui.thinking_state, &gui.game, (int)gui.skill_level);
    } else if (gui.zero_players) {
        chess_start_thinking_depth(&gui.thinking_state, &gui.game, (int)gui.skill_level);
    }
    
    // Create window
    gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui.window), "BeatChess");
    gtk_window_set_default_size(GTK_WINDOW(gui.window), 600, 750);
    g_signal_connect(gui.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Create main vbox
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gui.window), main_vbox);
    
    // Create menu bar
    GtkWidget *menu_bar = gtk_menu_bar_new();
    
    // === FILE MENU ===
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    
    GtkWidget *restart_item = gtk_menu_item_new_with_label("New Game");
    g_signal_connect(restart_item, "activate", G_CALLBACK(on_new_game), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), restart_item);
    
    GtkWidget *separator1 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator1);
    
    // Player color toggle (NEW)
    gui.player_color_item = gtk_check_menu_item_new_with_label("Play as Black");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gui.player_color_item), !gui.player_is_white);
    g_signal_connect(gui.player_color_item, "activate", G_CALLBACK(on_toggle_player_color), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gui.player_color_item);
    
    GtkWidget *separator2 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator2);
    
    GtkWidget *two_player_item = gtk_check_menu_item_new_with_label("Two-Player Mode");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(two_player_item), gui.two_player);
    g_signal_connect(two_player_item, "activate", G_CALLBACK(on_toggle_two_player), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), two_player_item);
    
    GtkWidget *zero_players_item = gtk_check_menu_item_new_with_label("AI vs AI (Zero Players)");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(zero_players_item), gui.zero_players);
    g_signal_connect(zero_players_item, "activate", G_CALLBACK(on_toggle_zero_players), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), zero_players_item);
    
    GtkWidget *separator3 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator3);
    
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    
    // === HELP MENU (NEW) ===
    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_item = gtk_menu_item_new_with_label("Help");
    
    GtkWidget *help_about_item = gtk_menu_item_new_with_label("Help & About");
    g_signal_connect(help_about_item, "activate", G_CALLBACK(on_menu_help), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), help_about_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), help_item);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), menu_bar, FALSE, FALSE, 0);
    
    // Create vbox for game content
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), vbox, TRUE, TRUE, 0);
    
    // Status label
    gui.status_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), gui.status_label, FALSE, FALSE, 5);
    update_status_text(&gui);
    
    // Create hbox for buttons
    GtkWidget *button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), button_hbox, FALSE, FALSE, 5);
    
    // Flip board button
    gui.flip_button = gtk_button_new_with_label("Flip Board");
    g_signal_connect(gui.flip_button, "clicked", G_CALLBACK(on_flip_board), &gui);
    gtk_box_pack_start(GTK_BOX(button_hbox), gui.flip_button, FALSE, FALSE, 0);
    
    // Undo button
    gui.undo_button = gtk_button_new_with_label("Undo Move");
    g_signal_connect(gui.undo_button, "clicked", G_CALLBACK(on_undo_move), &gui);
    gtk_box_pack_start(GTK_BOX(button_hbox), gui.undo_button, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(gui.undo_button, FALSE);  // Disabled at start
    
    // Skill level combo box
    gui.skill_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.skill_combo), "Moronic (Depth 2)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.skill_combo), "Easy (Depth 4)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.skill_combo), "Medium (Depth 6)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.skill_combo), "Hard (Depth 8)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.skill_combo), "Expert (Depth 10)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(gui.skill_combo), 2);  // Default to Medium
    g_signal_connect(gui.skill_combo, "changed", G_CALLBACK(on_skill_changed), &gui);
    gtk_box_pack_start(GTK_BOX(button_hbox), gui.skill_combo, FALSE, FALSE, 0);
    
    // Set initial skill level
    gui.skill_level = SKILL_MEDIUM;
    
    // Drawing area
    gui.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(gui.drawing_area, 600, 600);
    gtk_widget_add_events(gui.drawing_area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(gui.drawing_area, "draw", G_CALLBACK(on_draw), &gui);
    g_signal_connect(gui.drawing_area, "button-press-event", G_CALLBACK(on_button_press), &gui);
    gtk_box_pack_start(GTK_BOX(vbox), gui.drawing_area, TRUE, TRUE, 0);
    
    gtk_widget_show_all(gui.window);
    
    // Start AI move timer (1 second delay between moves)
    g_timeout_add(1000, ai_move_timeout, &gui);
    
    // Start time display update timer (100ms updates)
    gui.time_update_source = g_timeout_add(100, update_time_display, &gui);
    
    gtk_main();
    
    return 0;
}
